import struct
import re

with open(r"versions/archive/2026-10-15_25966348\schemasystem.dll", "rb") as f:
    data = f.read()

# Find all strings matching SchemaSystem_
for m in re.finditer(b"SchemaSystem_[a-zA-Z0-9_]+", data):
    print("Found interface string:", m.group(0).decode('latin-1'), "at offset", hex(m.start()))

# Find other interface names
for m in re.finditer(b"[A-Za-z0-9_]+_0[0-9]{2}", data):
    print("Candidate interface:", m.group(0).decode('latin-1'), "at offset", hex(m.start()))

# Search for class schema strings
schema_strings = re.findall(b"CSchema[A-Za-z0-9_]+", data)
print("\nCSchema classes found:", sorted(list(set(s.decode('latin-1') for s in schema_strings))))
