with open(r"versions/archive/2026-10-15_25966348\tier0.dll", "rb") as f:
    data = f.read()

import re
# Let's search for typical ConVar string formats: "%s = \"%s\" ( def. \"%s\" )"
matches = re.findall(b"\"[a-zA-Z0-9_% ]*\" \\( def\\. [^)]*\\)", data)
print("ConVar print formats found:", matches)

# Find all formats containing "def."
matches2 = re.findall(b"[^\x00]*def\\.[^\x00]*", data)
print("Def strings:", [m.decode('latin-1') for m in matches2[:5]])
