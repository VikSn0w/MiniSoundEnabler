#!/bin/sh
#
#  variants.sh  —  diagnostic: INIT packaging variants on one disk image
#
#  Usage: ./variants.sh "path/to/working v8 extension"
#  (the file itself, with its resource fork, as copied to this Mac)
#
#  Builds stage 0 and packages its PEF several ways, next to the
#  CodeWarrior-built v8.0 extension, on build/INIT Tests.dsk together
#  with AudioProbe. AudioProbe tries to prepare each file's INIT 0 with
#  the Code Fragment Manager and logs the result, without a restart.
#
#    Test 1 v8              working CodeWarrior build (resource fork copy)
#    Test 2 ours            stage 0 as build.sh makes it
#    Test 3 ours proc0      + procInfo 0, no sysHeap/locked attributes
#    Test 4 repacked        stage 0 PEF re-laid out like CodeWarrior's
#    Test 5 repacked proc0  repacked + procInfo 0 + no attributes
#    Test 6 tidy            Test 5 + bss as data, hash table power 1, timestamp

set -e

V8FILE=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
RETRO68=${RETRO68:-$HOME/Retro68-toolchain}
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/.."
OUT="$HERE/build/variants"
REZ="$RETRO68/bin/Rez"

"$HERE/build.sh" 0 >/dev/null
rm -rf "$OUT"
mkdir -p "$OUT"
cd "$OUT"

# 32-byte routine descriptor with the given procInfo (hex, 8 digits)
descriptor() {
    printf '\252\376\007\000\000\000\000\000\000\000\000\000'
    printf "$(echo "$1" | sed 's/../\\x&/g')"
    printf '\000\001\000\007\000\000\000\040'
    printf '\000\000\000\000\000\000\000\000'
}

python3 -I "$HERE/repack_pef.py" ../MacMiniAudioFix.pef repacked.pef
python3 -I "$HERE/repack_pef.py" ../MacMiniAudioFix.pef tidy.pef --tidy

{ descriptor 00000001; cat ../MacMiniAudioFix.pef; } > ours.init
{ descriptor 00000000; cat ../MacMiniAudioFix.pef; } > ours0.init
{ descriptor 00000001; cat repacked.pef; } > repacked.init
{ descriptor 00000000; cat repacked.pef; } > repacked0.init
{ descriptor 00000000; cat tidy.pef; } > tidy0.init

# variant <file name> <INIT data> <resource attributes>
variant() {
    {
        echo '#include "MacMiniAudioFix.r"'
        echo "data 'INIT' (0, \"$1\"$3) {"
        xxd -p -c 32 "$2" | sed 's/.*/    $"&"/'
        echo '};'
    } > v.r
    "$REZ" v.r -I"$SRC" -I"$HERE/include" -I"$RETRO68/RIncludes" -t INIT -c MmAf -o "$1.bin"
}

echo > empty.r
"$REZ" empty.r --copy "$V8FILE" -t INIT -c MmAf -o "Test 1 v8.bin"
variant "Test 2 ours" ours.init ", sysHeap, locked"
variant "Test 3 ours proc0" ours0.init ""
variant "Test 4 repacked" repacked.init ", sysHeap, locked"
variant "Test 5 repacked proc0" repacked0.init ""
variant "Test 6 tidy" tidy0.init ""

# One HFS image with everything
DSK="$HERE/build/INIT Tests.dsk"
dd if=/dev/zero of="$DSK" bs=1024 count=1440 2>/dev/null
"$RETRO68/bin/hformat" -l "INIT Tests" "$DSK" >/dev/null
"$RETRO68/bin/hmount" "$DSK" >/dev/null
for f in "Test 1 v8" "Test 2 ours" "Test 3 ours proc0" "Test 4 repacked" \
         "Test 5 repacked proc0" "Test 6 tidy"; do
    "$RETRO68/bin/hcopy" -m "$f.bin" ":$f"
done
"$RETRO68/bin/hcopy" -m "$SRC/AudioProbe/build/AudioProbe.bin" ":AudioProbe"
"$RETRO68/bin/hls" -l
"$RETRO68/bin/humount" >/dev/null

# The same files as MacBinary, for when Disk Copy won't mount a raw image
KIT="$HERE/build/INIT Tests"
rm -rf "$KIT"
mkdir -p "$KIT"
cp Test*.bin "$KIT/"
cp "$SRC/AudioProbe/build/AudioProbe.bin" "$KIT/"
echo "Built $DSK and $KIT/"
