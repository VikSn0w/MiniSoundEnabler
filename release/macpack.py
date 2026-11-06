#!/usr/bin/env python3
#
#  macpack.py  —  classic Mac file wrappers for the release
#
#  macpack.py dc42   volume.hfs  image.img  "Volume Name"
#      Raw 1440K HFS volume -> Disk Copy 4.2 image (mounts in Disk Copy
#      6.x and ShrinkWrap on OS 9).
#  macpack.py macbin file  out.bin  "Mac Name" TYPE CREA
#      Data-fork-only file -> MacBinary II.
#  macpack.py binhex file  out.hqx  "Mac Name" TYPE CREA
#      Data-fork-only file -> BinHex 4.0.
#
#  The Disk Copy image only has a data fork, so MacBinary and BinHex
#  just carry it together with its type and creator (dImg/dCpy).

import struct
import sys


def mac_name(name):
    b = name.encode('mac_roman')
    if not 1 <= len(b) <= 31:
        sys.exit('name must be 1-31 characters: %r' % name)
    return b


# ---- Disk Copy 4.2 ----

def dc42_checksum(data):
    s = 0
    for (w,) in struct.iter_unpack('>H', data):
        s = (s + w) & 0xFFFFFFFF
        s = ((s >> 1) | (s << 31)) & 0xFFFFFFFF
    return s


def dc42(src, dst, volname):
    data = open(src, 'rb').read()
    if len(data) != 1440 * 1024:
        sys.exit('%s: expected a 1440K volume, got %d bytes' % (src, len(data)))
    name = mac_name(volname)
    hdr = bytes([len(name)]) + name.ljust(63, b'\0')
    hdr += struct.pack('>IIII', len(data), 0, dc42_checksum(data), 0)
    hdr += bytes([3, 0x22]) + struct.pack('>H', 0x0100)   # 1440K, Mac HFS
    assert len(hdr) == 84
    open(dst, 'wb').write(hdr + data)


# ---- MacBinary II ----

def crc16_xmodem(data):
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else crc << 1
            crc &= 0xFFFF
    return crc


def macbin(src, dst, name, ftype, creator):
    data = open(src, 'rb').read()
    n = mac_name(name)
    h = bytearray(128)
    h[1] = len(n)
    h[2:2 + len(n)] = n
    h[65:69] = ftype.encode('mac_roman')
    h[69:73] = creator.encode('mac_roman')
    struct.pack_into('>II', h, 83, len(data), 0)
    t = 0xB8A2D2C0          # some fixed date: 2001-01-01 (Mac epoch)
    struct.pack_into('>II', h, 91, t, t)
    h[122] = 129            # MacBinary II
    h[123] = 129
    struct.pack_into('>H', h, 124, crc16_xmodem(bytes(h[:124])))
    pad = b'\0' * (-len(data) % 128)
    open(dst, 'wb').write(bytes(h) + data + pad)


# ---- BinHex 4.0 ----

HQX = '!"#$%&\'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr'


def hqx_crc(data):
    crc = 0
    for b in data:
        for i in range(8):
            c = crc & 0x8000
            crc = ((crc << 1) | ((b >> (7 - i)) & 1)) & 0xFFFF
            if c:
                crc ^= 0x1021
    for _ in range(16):          # flush two zero bytes
        c = crc & 0x8000
        crc = (crc << 1) & 0xFFFF
        if c:
            crc ^= 0x1021
    return crc


def rle90(data):
    out = bytearray()
    i = 0
    while i < len(data):
        b = data[i]
        run = 1
        while i + run < len(data) and data[i + run] == b and run < 255:
            run += 1
        out.append(b)
        if b == 0x90:
            out.append(0)
        if run >= 3:
            out += bytes([0x90, run])
            i += run
        else:
            i += 1
    return bytes(out)


def binhex(src, dst, name, ftype, creator):
    data = open(src, 'rb').read()
    n = mac_name(name)
    hdr = bytes([len(n)]) + n + b'\0'
    hdr += ftype.encode('mac_roman') + creator.encode('mac_roman')
    hdr += struct.pack('>HII', 0, len(data), 0)
    stream = hdr + struct.pack('>H', hqx_crc(hdr))
    stream += data + struct.pack('>H', hqx_crc(data))
    stream += struct.pack('>H', hqx_crc(b''))            # empty resource fork
    enc = rle90(stream)
    enc += b'\0' * (-len(enc) % 3)
    chars = []
    for i in range(0, len(enc), 3):
        v = enc[i] << 16 | enc[i + 1] << 8 | enc[i + 2]
        chars += [HQX[v >> 18], HQX[(v >> 12) & 63], HQX[(v >> 6) & 63], HQX[v & 63]]
    body = ':' + ''.join(chars) + ':'
    lines = [body[i:i + 64] for i in range(0, len(body), 64)]
    with open(dst, 'w', newline='\n') as f:
        f.write('(This file must be converted with BinHex 4.0)\n\n')
        f.write('\n'.join(lines) + '\n')


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__ or 'see the comment at the top of macpack.py')
    cmd = sys.argv[1]
    if cmd == 'dc42' and len(sys.argv) == 5:
        dc42(*sys.argv[2:5])
    elif cmd in ('macbin', 'binhex') and len(sys.argv) == 7:
        (macbin if cmd == 'macbin' else binhex)(*sys.argv[2:7])
    else:
        sys.exit('usage: see the comment at the top of macpack.py')


if __name__ == '__main__':
    main()
