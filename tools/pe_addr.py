#!/usr/bin/env python3
"""Map RVA offsets to nearest COFF/export symbols if present."""
import struct
import sys

def main():
    path = sys.argv[1]
    offs = [int(x, 0) for x in sys.argv[2:]]
    data = open(path, "rb").read()
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    opt = e_lfanew + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic != 0x20B:
        print("not PE32+", hex(magic))
        return
    imagebase = struct.unpack_from("<Q", data, opt + 24)[0]
    print(f"imagebase={imagebase:#x}")
    # section headers
    coff = e_lfanew + 4
    numsec = struct.unpack_from("<H", data, coff + 2)[0]
    opthdrsz = struct.unpack_from("<H", data, coff + 16)[0]
    sec = opt + opthdrsz
    sections = []
    for i in range(numsec):
        off = sec + i * 40
        name = data[off : off + 8].split(b"\0", 1)[0].decode("ascii", "replace")
        vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", data, off + 8)
        sections.append((name, vaddr, vsize, rawptr, rawsize))
        print(f"section {name!r} va={vaddr:#x} vsize={vsize:#x}")
    # try .pdata / no symbols — print file offsets for disassembly
    for o in offs:
        for name, vaddr, vsize, rawptr, rawsize in sections:
            if vaddr <= o < vaddr + max(vsize, rawsize):
                fileoff = rawptr + (o - vaddr)
                print(f"offset {o:#x} -> {name} file={fileoff:#x}")
                break

if __name__ == "__main__":
    main()
