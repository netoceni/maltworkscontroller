from gzip import compress
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "websource" / "index.html"
COMPRESSED = ROOT / "websource" / "index.html.gz"
CPP = ROOT / "webassets.cpp"


source = SOURCE.read_bytes()
packed = compress(source, compresslevel=9, mtime=0)
COMPRESSED.write_bytes(packed)

rows = [
    ", ".join(f"0x{byte:02x}" for byte in packed[index:index + 12])
    for index in range(0, len(packed), 12)
]
body = "".join(f"  {row},\n" for row in rows)
CPP.write_text(
    '#include "webassets.h"\n\n'
    "const uint8_t INDEX_HTML_GZ[] PROGMEM = {\n"
    f"{body}"
    "};\n\n"
    "const size_t INDEX_HTML_GZ_LEN = sizeof(INDEX_HTML_GZ);\n",
    encoding="utf-8",
    newline="\n",
)
