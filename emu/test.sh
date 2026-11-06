#!/bin/sh
#
#  test.sh  —  build, install into the OS 9 VM, run AudioProbe unattended
#
#  Usage: emu/test.sh [tests.txt] [--no-extension] [--show]
#
#  1. Builds the extension (stage 3) and AudioProbe.
#  2. Mounts the VM disk (VM must be shut down) and installs:
#       System Folder:Extensions:MacMini Audio Fix   (unless --no-extension)
#       System Folder:Startup Items:AudioProbe
#       System Folder:Startup Items:AudioProbe Auto  (the test list)
#  3. Boots the VM with sound recorded to a WAV file (no window unless
#     --show); AudioProbe runs the tests and shuts the VM down.
#  4. Copies the log out and prints it with the sound levels found.
#
#  Results go to emu/out/: AudioProbe.log, run.wav, last.png (screen at
#  the end, if the VM had to be stopped).

set -e

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT="$HERE/.."
VMDIR=${VMDIR:-$HOME/Developer/os9-vm}
QEMU=${QEMU:-$HOME/Developer/qemu-screamer/build/qemu-system-ppc}
RETRO68=${RETRO68:-$HOME/Retro68-toolchain}
TIMEOUT=${TIMEOUT:-300}
OUT="$HERE/out"

TESTS="$HERE/tests/default.txt"
INSTALL_EXT=1
DISPLAY_OPT="-display none"
for a in "$@"; do
    case "$a" in
    --no-extension) INSTALL_EXT=0 ;;
    --show) DISPLAY_OPT="" ;;
    *) TESTS="$a" ;;
    esac
done

if pgrep -f "qemu-system-ppc.*$VMDIR/os9.img" >/dev/null; then
    echo "The VM is running; shut it down first." >&2
    exit 1
fi
mkdir -p "$OUT"
rm -f "$OUT/AudioProbe.log" "$OUT/run.wav" "$OUT/last.png"

echo "== build"
"$ROOT/Retro68/build.sh" 3 | tail -1
cmake --build "$ROOT/AudioProbe/build" | tail -1

mount_disk() {   # $1: extra hdiutil flags; prints the mount point
    hdiutil attach $1 -nobrowse -imagekey diskimage-class=CRawDiskImage \
        "$VMDIR/os9.img" 2>/dev/null | awk -F'\t' '/\/Volumes\//{print $NF}' | head -1
}

echo "== install"
MP=$(mount_disk "")
[ -n "$MP" ] || { echo "could not mount $VMDIR/os9.img" >&2; exit 1; }
SYS="$MP/System Folder"
EXT="$SYS/Extensions/MacMini Audio Fix"
SI="$SYS/Startup Items"
rm -f "$EXT" "$SI/AudioProbe" "$SI/AudioProbe Log" "$SI/AudioProbe Auto"
if [ $INSTALL_EXT = 1 ]; then
    python3 -I "$HERE/macbin.py" "$ROOT/Retro68/build/stage3/MacMini Audio Fix.bin" "$EXT"
fi
python3 -I "$HERE/macbin.py" "$ROOT/AudioProbe/build/AudioProbe.bin" "$SI/AudioProbe"
tr '\n' '\r' < "$TESTS" > "$SI/AudioProbe Auto"
# TEXT/ttxt (SimpleText), 32 bytes of Finder info
xattr -wx com.apple.FinderInfo "$(printf '5445585474747874%048d' 0)" "$SI/AudioProbe Auto"
sync
hdiutil detach "$MP" >/dev/null

echo "== run (timeout ${TIMEOUT}s)"
rm -f "$VMDIR/monitor.sock"
"$QEMU" -M mac99,via=pmu -m 512 -g 1024x768x32 $DISPLAY_OPT \
    -drive file="$VMDIR/os9.img",format=raw,media=disk -boot c \
    -audiodev wav,id=snd0,path="$OUT/run.wav" -global screamer.audiodev=snd0 \
    -monitor unix:"$VMDIR/monitor.sock",server,nowait &
QPID=$!
T=0
while kill -0 $QPID 2>/dev/null && [ $T -lt $TIMEOUT ]; do
    sleep 5
    T=$((T + 5))
done
if kill -0 $QPID 2>/dev/null; then
    echo "   timed out: saving the screen and stopping the VM"
    echo "screendump $OUT/last.ppm" | nc -U -w 2 "$VMDIR/monitor.sock" >/dev/null || true
    sleep 2
    sips -s format png "$OUT/last.ppm" --out "$OUT/last.png" >/dev/null 2>&1 || true
    rm -f "$OUT/last.ppm"
    echo quit | nc -U -w 2 "$VMDIR/monitor.sock" >/dev/null || kill $QPID
    sleep 2
fi
wait $QPID 2>/dev/null || true

echo "== results"
MP=$(mount_disk "-readonly")
if [ -f "$MP/System Folder/Startup Items/AudioProbe Log" ]; then
    tr '\r' '\n' < "$MP/System Folder/Startup Items/AudioProbe Log" > "$OUT/AudioProbe.log"
fi
hdiutil detach "$MP" >/dev/null
if [ -f "$OUT/AudioProbe.log" ]; then
    grep -av "^  [a-zA-Z ]\{4\}/.\{4\}/.\{4\}  [0-9A-F]\{8\}  0  " "$OUT/AudioProbe.log" || true
else
    echo "no AudioProbe Log (AudioProbe didn't run?)"
fi
[ -f "$OUT/run.wav" ] && python3 -I "$HERE/wav_levels.py" "$OUT/run.wav"
