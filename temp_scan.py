import re, sys

path = "language/_SRC/GCL/SimpleRunner/gcl_runner.c"
data = open(path, "rb").read()
text = data.decode("utf-8", errors="replace")

# Turkish letters + common mojibake (double-encoded UTF-8) patterns
pat = re.compile(r"[çğıİöşüÇĞÖŞÜ]|Ä±|ÄŸ|ÅŸ|Ã§|Ã¼|Ã¶|Ä°|Ã–|Ã‡|Åž|Ãœ|Äž")

for i, line in enumerate(text.split("\n"), 1):
    if pat.search(line):
        print(f"{i}: {line}")