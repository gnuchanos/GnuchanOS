import struct
import sys


def pe_exports(dll_path):
    with open(dll_path, "rb") as f:
        data = f.read()

    if data[:2] != b"MZ":
        raise ValueError("MZ header yok")
    (e_lfanew,) = struct.unpack_from("<I", data, 0x3C)

    if data[e_lfanew:e_lfanew + 4] != b"PE\0\0":
        raise ValueError("PE imzasi yok")

    coff = e_lfanew + 4
    machine, nsects = struct.unpack_from("<HH", data, coff)
    (size_opt,) = struct.unpack_from("<H", data, coff + 16)
    opt = coff + 20
    magic, = struct.unpack_from("<H", data, opt)
    if magic != 0x20B:
        raise ValueError("PE32+ bekleniyor (magic=0x%x)" % magic)
    # PE32+: export directory RVA, size — data directory[0]
    dd_rva, dd_size = struct.unpack_from("<II", data, opt + 112)

    if dd_rva == 0:
        raise ValueError("export tablosu yok")

    sections = []
    sec_start = opt + size_opt
    for i in range(nsects):
        off = sec_start + i * 40
        (vsize, vaddr, rawsize, rawptr) = struct.unpack_from("<IIII", data, off + 8)
        sections.append((vaddr, vsize, rawptr, rawsize))

    def rva_to_off(rva):
        for vaddr, vsize, rawptr, rawsize in sections:
            if vaddr <= rva < vaddr + vsize:
                return rawptr + (rva - vaddr)
        raise ValueError("RVA 0x%x section'da degil" % rva)

    eoff = rva_to_off(dd_rva)
    num_names = struct.unpack_from("<I", data, eoff + 24)[0]
    names_rva = struct.unpack_from("<I", data, eoff + 32)[0]

    names = []
    noff = rva_to_off(names_rva)
    for i in range(num_names):
        (name_rva,) = struct.unpack_from("<I", data, noff + i * 4)
        no = rva_to_off(name_rva)
        end = data.index(b"\0", no)
        names.append(data[no:end].decode("ascii", "replace"))

    return sorted(set(names))


def main():
    dll = sys.argv[1]
    out_def = sys.argv[2]
    names = pe_exports(dll)
    with open(out_def, "w", encoding="utf-8") as f:
        f.write("LIBRARY python314.dll\n")
        f.write("EXPORTS\n")
        for name in names:
            f.write("    %s\n" % name)
    print("%d export yazildi -> %s" % (len(names), out_def))


if __name__ == "__main__":
    main()
