#!/bin/sh
#
#  vm.sh  —  Mac OS 9.2.2 in QEMU (mac99 + screamer sound) for testing
#
#  Usage:
#    emu/vm.sh install   boot the OS 9.2.2 CD with the VM disk attached
#    emu/vm.sh run       boot the VM disk, sound to the Mac's speakers
#
#  Needs a QEMU with Mark Cave-Ayland's screamer sound device (branch
#  screamer-v9.1.0, see emu/README.md); stock QEMU has no Mac sound.
#  Settings can be overridden from the environment:
#    QEMU   path to that qemu-system-ppc
#    VMDIR  folder holding os9.img (the VM disk)
#    CD     OS 9.2.2 install CD image
#
#  Guest memory stays at 512 MB: screamer misbehaves above 1023 MB.

set -e

QEMU=${QEMU:-$HOME/Developer/qemu-screamer/build/qemu-system-ppc}
VMDIR=${VMDIR:-$HOME/Developer/os9-vm}
CD=${CD:-"$HOME/Downloads/MacOS 922 Universal.iso"}
DISK="$VMDIR/os9.img"

# The monitor socket lets scripts take screenshots or quit the VM:
#   echo "screendump /tmp/shot.ppm" | nc -U "$VMDIR/monitor.sock"
common() {
    "$QEMU" -M mac99,via=pmu -m 512 -g 1024x768x32 \
        -drive file="$DISK",format=raw,media=disk \
        -global screamer.audiodev=snd0 \
        -monitor unix:"$VMDIR/monitor.sock",server,nowait "$@"
}

case "$1" in
install)
    common -boot d -cdrom "$CD" -audiodev coreaudio,id=snd0
    ;;
run)
    common -boot c -audiodev coreaudio,id=snd0
    ;;
*)
    echo "usage: $0 install|run" >&2
    exit 2
    ;;
esac
