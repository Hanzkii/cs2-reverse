with open(r"versions/archive/2026-10-15_25966348\engine2.dll", "rb") as f:
    data = f.read()

import re
matches = re.findall(b"[^\x00]*def\\.[^\x00]*", data)
print("Def strings in engine2.dll:", [m.decode('latin-1') for m in matches[:10]])

matches_fmt = re.findall(b"\"%s\" = \"[^\"]*\"", data)
print("Format strings in engine2.dll:", [m.decode('latin-1') for m in matches_fmt[:10]])

# Search for "FCVAR_REPLICATED" references in engine2.dll
repl = data.find(b"FCVAR_REPLICATED")
print("FCVAR_REPLICATED offset in engine2.dll:", hex(repl))
if repl != -1:
    print("Context:", data[max(0, repl-100):min(len(data), repl+200)])
