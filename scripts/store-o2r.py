#!/usr/bin/env python3
"""Copy an .o2r archive with every entry STORED (uncompressed), so the 3DS
reads it without spending CPU on inflate. Usage: store-o2r.py <in> <out>"""
import sys
import zipfile

src, dst = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(src) as zin, zipfile.ZipFile(dst, "w", zipfile.ZIP_STORED) as zout:
    for info in zin.infolist():
        data = zin.read(info)
        info.compress_type = zipfile.ZIP_STORED
        zout.writestr(info, data)
