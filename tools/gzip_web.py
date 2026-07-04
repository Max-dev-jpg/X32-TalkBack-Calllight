# =============================================================================
# gzip_web.py  –  PlatformIO pre-script: pre-compress the web UI assets.
#
# serveStatic() serves "<file>.gz" (with Content-Encoding: gzip) when present,
# so a browser downloads ~5x fewer bytes. That is the difference between the page
# loading or not on a weak/lossy Wi-Fi link.
#
# The .gz files are regenerated from their plain sources on every filesystem
# build (buildfs/uploadfs), so they can never go stale. The plain files stay in
# the image too (fallback for clients that don't accept gzip).
# =============================================================================
Import("env")
import gzip
import os

DATA_DIR = env.subst("$PROJECT_DATA_DIR")
FILES = ["index.html", "app.js", "style.css"]


def gzip_web(*args, **kwargs):
    for name in FILES:
        src = os.path.join(DATA_DIR, name)
        if not os.path.isfile(src):
            continue
        dst = src + ".gz"
        with open(src, "rb") as f:
            raw = f.read()
        with gzip.GzipFile(dst, "wb", compresslevel=9, mtime=0) as f:
            f.write(raw)
        print("[gzip_web] %s  %d -> %d bytes" % (name, len(raw), os.path.getsize(dst)))


# Only needed when building the LittleFS image (buildfs / uploadfs targets).
if any("fs" in t for t in COMMAND_LINE_TARGETS):
    gzip_web()
