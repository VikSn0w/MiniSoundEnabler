#!/bin/sh
#
#  build.sh  —  build Mini Sound Enabler with Retro68 on a modern Mac
#
#  Produces, in Retro68/build/:
#    Mini Sound Enabler.bin   MacBinary (keeps the resource fork)
#    Mini Sound Enabler.dsk   HFS disk image with the extension on it
#
#  Retro68 can't make a PowerPC code resource by itself, so this links
#  the code into a PEF fragment and puts it in INIT 0 behind a Mixed
#  Mode routine descriptor (an "accelerated" code resource, the same
#  thing CodeWarrior's PPC Code Resource target makes). The 68K INIT
#  loader jumps into the descriptor and Mixed Mode has the Code
#  Fragment Manager prepare the PEF and call main().
#
#  Usage: RETRO68=~/Retro68-toolchain ./build.sh [stage]
#
#  stage (default 3) selects how much of v3 is built in:
#    0  v8.0 one-shot fix from the src/ modules
#    1  + resident AmpMonitor and status record
#    2  + volume wrapper answering volume reads (alerts play)
#    3  + volume changes stored (release build)
#  Each stage's result is also copied to build/stage<N>/.
#  BREADCRUMBS=1 builds a debug version that draws a dash icon at each
#  startup step (see Main.c), copied to build/stage<N>-debug/.

set -e

STAGE=${1:-3}
RETRO68=${RETRO68:-$HOME/Retro68-toolchain}
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/.."
OUT="$HERE/build"
NAME="Mini Sound Enabler"

CC="$RETRO68/bin/powerpc-apple-macos-gcc"
MAKEPEF="$RETRO68/bin/MakePEF"
REZ="$RETRO68/bin/Rez"
LIBS="$RETRO68/powerpc-apple-macos/lib"
LDS="$RETRO68/powerpc-apple-macos/cmake/ndrv.lds"

mkdir -p "$OUT"
cd "$OUT"

# 1. Compile and link to XCOFF. No C runtime: the only entry is main(),
#    and everything it calls comes from the system's shared libraries.
#    glue.c stands in for libgcc's __main, which gcc calls from main().
#    min-pagesize=0 because low memory (0x92C) is a real address here.
CFLAGS="-Os -Wall -Wno-main -Wno-unused-function --param=min-pagesize=0"
SUFFIX=
if [ -n "$BREADCRUMBS" ]; then
    CFLAGS="$CFLAGS -DMMAF_BREADCRUMBS"
    SUFFIX=-debug
fi

OBJS=""
for c in "$SRC"/src/*.c; do
    o=$(basename "$c" .c).o
    "$CC" $CFLAGS -DMMAF_STAGE=$STAGE -I"$HERE/include" -I"$SRC/src" \
        -c "$c" -o "$o"
    OBJS="$OBJS $o"
done
"$CC" $CFLAGS -c "$HERE/glue.c" -o glue.o
"$CC" -nostartfiles -nostdlib -T "$LDS" -Wl,-e,main \
    $OBJS glue.o \
    "$LIBS/libNameRegistryLib.a" \
    "$LIBS/libDriverServicesLib.a" \
    "$LIBS/libSoundLib.a" \
    "$LIBS/libInterfaceLib.a" \
    -lgcc -o MacMiniAudioFix.xcoff

# 2. XCOFF -> PEF, then lay it out like CodeWarrior's PPC code resources.
#    Mac OS 9 refuses to load MakePEF's layout as an INIT at startup
#    ("Code Fragment could not be prepared", error type 1010), although
#    the Code Fragment Manager accepts it in an application. --tidy makes
#    zero-fill explicit data, sizes the export hash table like CodeWarrior
#    and sets a timestamp; that combination loads (see TECHNICAL.md).
"$MAKEPEF" MacMiniAudioFix.xcoff -o MacMiniAudioFix-raw.pef
python3 -I "$HERE/repack_pef.py" MacMiniAudioFix-raw.pef MacMiniAudioFix.pef --tidy

# 3. Routine descriptor + PEF = accelerated INIT resource.
#    Same as CodeWarrior's: MixedModeMagic, version 7, one PowerPC
#    routine record whose flags (relative | fragment needs preparing |
#    native ISA) point 32 bytes ahead at the PEF container.
#    procInfo 0 = no args, no result.
printf '\252\376\007\000\000\000\000\000\000\000\000\000'  > INIT.bin
printf '\000\000\000\000'                                  >> INIT.bin
printf '\000\001\000\007\000\000\000\040'                  >> INIT.bin
printf '\000\000\000\000\000\000\000\000'                  >> INIT.bin
cat MacMiniAudioFix.pef >> INIT.bin

# 4. Resources: the INIT plus everything in MacMiniAudioFix.r
#    (Retro68's Rez has no 'read', so the code goes in as hex data.)
#    No resource attributes, as in the CodeWarrior build that loads;
#    Main.c checks the code really is in the system heap before
#    staying resident.
{
    echo '#include "MacMiniAudioFix.r"'
    echo "data 'INIT' (0, \"$NAME\") {"
    xxd -p -c 32 INIT.bin | sed 's/.*/    $"&"/'
    echo '};'
    # An untouched copy of the PEF. At startup the Code Fragment Manager
    # relocates INIT 0's data section in place, so Main.c builds the
    # resident system-heap copy from this resource instead.
    echo "data 'MmPf' (0, \"Resident PEF\") {"
    xxd -p -c 32 MacMiniAudioFix.pef | sed 's/.*/    $"&"/'
    echo '};'
} > Build.r
"$REZ" Build.r -I"$SRC" -I"$HERE/include" -I"$RETRO68/RIncludes" \
    -t INIT -c MmAf \
    -o "$NAME.bin" --cc "$NAME.dsk"

mkdir -p "stage$STAGE$SUFFIX"
cp "$NAME.bin" "$NAME.dsk" "stage$STAGE$SUFFIX/"
echo "Built $OUT/$NAME.bin and $OUT/$NAME.dsk (stage $STAGE$SUFFIX)"
