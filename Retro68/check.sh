#!/bin/sh
#
#  check.sh  —  structural checks on a built MacMini Audio Fix
#
#  Usage: ./check.sh "build/MacMini Audio Fix.bin"
#
#  Prints the file's resources, then the PEF inside INIT 0: which
#  section holds main and which shared libraries and symbols it imports.
#  Reads build/MacMiniAudioFix.pef from the same build.

set -e

RETRO68=${RETRO68:-$HOME/Retro68-toolchain}
HERE=$(cd "$(dirname "$0")" && pwd)
BIN=${1:-"$HERE/build/MacMini Audio Fix.bin"}
PEF="$HERE/build/MacMiniAudioFix.pef"

"$RETRO68/bin/ResInfo" -a "$BIN"

python3 -I - "$PEF" <<'EOF'
import struct, sys
d = open(sys.argv[1], 'rb').read()
assert d[:8] == b'Joy!peff', 'not a PEF container'
nsec = struct.unpack('>H', d[32:34])[0]
secs = [struct.unpack('>iIIIIIBBBB', d[40 + 28 * i:68 + 28 * i]) for i in range(nsec)]
ld = [s for s in secs if s[6] == 4][0]
o = ld[5]
(mainSec, mainOff, _, _, _, _, nlib, nsym, _, _, strOff, _, _, _) = \
    struct.unpack('>iIiIiIIIIIIIII', d[o:o + 56])
print('main: section %d offset 0x%x' % (mainSec, mainOff))
def cstr(p):
    return d[p:d.index(b'\0', p)].decode()
st = o + strOff
for i in range(nlib):
    q = o + 56 + 24 * i
    nameOff, _, _, symCount, firstSym = struct.unpack('>IIIII', d[q:q + 20])
    syms = []
    for j in range(firstSym, firstSym + symCount):
        v = struct.unpack('>I', d[o + 56 + 24 * nlib + 4 * j:o + 60 + 24 * nlib + 4 * j])[0]
        syms.append(cstr(st + (v & 0xFFFFFF)))
    print('lib %s: %s' % (cstr(st + nameOff), ' '.join(sorted(syms))))
EOF
