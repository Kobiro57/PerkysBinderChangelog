"""Read-only r9 PE/artifact verification; never loads or executes binaries."""
import hashlib
import json
from pathlib import Path
import struct
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


class PE:
    def __init__(self, path):
        self.path = Path(path)
        self.data = self.path.read_bytes()
        require(self.data[:2] == b"MZ", "Missing DOS header")
        start = self.u32(0x3C)
        require(self.data[start:start + 4] == b"PE\0\0", "Missing PE signature")
        self.machine = self.u16(start + 4)
        count = self.u16(start + 6)
        self.characteristics = self.u16(start + 22)
        self.optional = start + 24
        require(self.u16(self.optional) == 0x10B, "Expected PE32")
        self.sections = []
        table = self.optional + self.u16(start + 20)
        for index in range(count):
            row = table + index * 40
            virtual_size, rva, size, raw = struct.unpack_from("<IIII", self.data, row + 8)
            self.sections.append((rva, max(virtual_size, size), raw, size))

    def u16(self, offset):
        return struct.unpack_from("<H", self.data, offset)[0]

    def u32(self, offset):
        return struct.unpack_from("<I", self.data, offset)[0]

    def offset(self, rva):
        for start, span, raw, size in self.sections:
            if start <= rva < start + span:
                require(rva - start < size, "RVA has no raw file data")
                return raw + rva - start
        raise ValueError(f"Unmapped RVA: {rva:#x}")

    def imports(self):
        directory = self.u32(self.optional + 96 + 8)
        if not directory:
            return []
        row = self.offset(directory)
        result = []
        while any(struct.unpack_from("<IIIII", self.data, row)):
            name = self.offset(self.u32(row + 12))
            end = self.data.index(0, name)
            result.append(self.data[name:end].decode("ascii"))
            row += 20
        return sorted(result, key=str.lower)

    def resource(self, type_id, resource_id):
        base = self.offset(self.u32(self.optional + 96 + 16))

        def entries(relative):
            head = base + relative
            total = self.u16(head + 12) + self.u16(head + 14)
            return [struct.unpack_from("<II", self.data, head + 16 + i * 8) for i in range(total)]

        directory = 0
        for expected in (type_id, resource_id):
            candidates = [value for name, value in entries(directory) if name == expected]
            require(len(candidates) == 1, f"Resource ID {expected} missing or ambiguous")
            require(candidates[0] & 0x80000000, "Expected resource directory")
            directory = candidates[0] & 0x7FFFFFFF
        languages = entries(directory)
        require(len(languages) == 1, "Expected one embedded worker language")
        leaf = languages[0][1]
        require(not leaf & 0x80000000, "Expected resource data entry")
        rva, size = struct.unpack_from("<II", self.data, base + leaf)
        start = self.offset(rva)
        return self.data[start:start + size]


def main():
    root = Path(__file__).resolve().parents[1]
    asi = PE(sys.argv[1] if len(sys.argv) > 1 else root / "build/Release/Keybinder.asi")
    helper = PE(root / "build/ReconnectHelper.exe")
    require(asi.machine == helper.machine == 0x14C, "Expected both binaries to be x86")
    require(asi.characteristics & 0x2000, "ASI must be a DLL")
    require(not helper.characteristics & 0x2000, "Worker must be an executable")
    require(asi.resource(10, 300) == helper.data, "Embedded helper differs from built helper")
    imports = asi.imports()
    require(not any(name.lower().startswith(("vcruntime", "msvcp", "ucrtbased")) for name in imports),
            "Unexpected external C++ runtime dependency")
    try:
        display_path = asi.path.resolve().relative_to(root).as_posix()
    except ValueError:
        display_path = asi.path.name
    result = {
        "revision": "v0.1.1-r9",
        "asi": display_path,
        "bytes": len(asi.data),
        "format": "PE32 x86 DLL",
        "sha256": hashlib.sha256(asi.data).hexdigest(),
        "imports": imports,
        "embedded_helper_id": 300,
        "embedded_helper_bytes": len(helper.data),
        "helper_sha256": hashlib.sha256(helper.data).hexdigest(),
        "embedded_helper_matches": True,
    }
    report = json.dumps(result, indent=2)
    (root / "build/revision9-artifact.json").write_text(report + "\n", encoding="utf-8")
    print(report)


if __name__ == "__main__":
    main()
