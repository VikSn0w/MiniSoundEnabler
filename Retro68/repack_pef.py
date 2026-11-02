#!/usr/bin/env python3
#
#  repack_pef.py  —  lay out a PEF container the way CodeWarrior does
#
#  Usage: repack_pef.py in.pef out.pef
#
#  MakePEF packs section contents right after the headers with 4-byte
#  alignment. CodeWarrior's PPC code resources put each section's
#  contents on a 16-byte boundary (alignment field 4), loader section
#  first. Section contents are copied unchanged: loader offsets are
#  relative to the loader section and relocations refer to section
#  numbers, so moving whole sections is safe.

import struct
import sys

HEADER = 40
SECTION = 28
ALIGN = 16
KIND_LOADER = 4


def align(n):
    return (n + ALIGN - 1) // ALIGN * ALIGN


KIND_DATA = 1


def tidy(d, secs, body):
    """--tidy: match the remaining CodeWarrior details.
    Zero-fill (bss) becomes explicit data, the export hash table gets
    2 entries (power 1) instead of 1, and the header gets a timestamp."""
    for i, f in enumerate(secs):
        if f[6] == KIND_DATA and f[2] > f[3]:   # totalLength > unpackedLength
            body[i] += bytes(f[2] - f[3])
            f[3] = f[4] = f[2]
        if f[6] == KIND_LOADER:
            ld = bytearray(body[i])
            hashOff, hashPow, nexp = struct.unpack('>III', ld[44:56])
            if nexp == 0:
                # With no exports the hash table is just zeroed entries at
                # the end. Rebuild it 4-byte aligned with 2 entries.
                if hashOff + 4 * (1 << hashPow) != len(ld):
                    sys.exit('unexpected loader layout')
                newOff = (hashOff + 3) // 4 * 4
                ld = ld[:hashOff] + bytes(newOff - hashOff) + bytes(4 * 2)
                struct.pack_into('>II', ld, 44, newOff, 1)
                body[i] = bytes(ld)
                f[3] = 0
                f[4] = len(ld)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    src, dst = args[0], args[1]
    d = open(src, 'rb').read()
    if d[:8] != b'Joy!peff':
        sys.exit('%s: not a PEF container' % src)
    nsec = struct.unpack('>H', d[32:34])[0]

    secs = []
    for i in range(nsec):
        f = list(struct.unpack('>iIIIIIBBBB', d[HEADER + SECTION * i:HEADER + SECTION * (i + 1)]))
        if f[0] != -1:
            sys.exit('named sections are not supported')
        secs.append(f)
    body = {i: d[f[5]:f[5] + f[4]] for i, f in enumerate(secs)}  # containerOffset, containerLength
    header = bytearray(d[:HEADER + SECTION * nsec])
    if '--tidy' in sys.argv:
        tidy(d, secs, body)
        struct.pack_into('>I', header, 16, 0xE5E499BF)       # dateTimeStamp

    # loader first, then the others in their original order
    order = sorted(range(nsec), key=lambda i: secs[i][6] != KIND_LOADER)
    out = header
    pos = align(len(out))
    for i in order:
        f = secs[i]
        f[5] = pos
        f[8] = 4                             # alignment: 2^4 = 16 bytes
        pos = align(pos + f[4])

    out += bytes(pos - len(out))
    for i in range(nsec):
        f = secs[i]
        struct.pack_into('>iIIIIIBBBB', out, HEADER + SECTION * i, *f)
        out[f[5]:f[5] + f[4]] = body[i]
    open(dst, 'wb').write(bytes(out))


main()
