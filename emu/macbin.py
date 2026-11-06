#!/usr/bin/env python3
#
#  macbin.py  —  unpack a MacBinary file onto a Mac-format volume
#
#  Usage: macbin.py file.bin "/Volumes/X/folder/Name"
#
#  Writes the data fork, the resource fork (via ..namedfork/rsrc) and the
#  Finder type/creator, so the file works when the volume is used by
#  Mac OS 9. The destination must be on an HFS+ volume mounted on macOS.

import struct
import subprocess
import sys


def main():
    src, dst = sys.argv[1], sys.argv[2]
    d = open(src, 'rb').read()
    if len(d) < 128 or d[0] != 0 or d[74] != 0:
        sys.exit('%s: not MacBinary' % src)
    ftype, creator = d[65:69], d[69:73]
    flags = d[73] << 8 | d[101]
    data_len, rsrc_len = struct.unpack('>II', d[83:91])
    data_off = 128
    rsrc_off = data_off + (data_len + 127) // 128 * 128

    with open(dst, 'wb') as f:
        f.write(d[data_off:data_off + data_len])
    if rsrc_len:
        with open(dst + '/..namedfork/rsrc', 'wb') as f:
            f.write(d[rsrc_off:rsrc_off + rsrc_len])
    # FinderInfo: type, creator, flags (clear "inited" so the Finder places it)
    finder_info = ftype + creator + struct.pack('>H', flags & ~0x0100) + bytes(22)
    subprocess.run(['xattr', '-wx', 'com.apple.FinderInfo', finder_info.hex(), dst],
                   check=True)


main()
