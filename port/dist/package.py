#!/usr/bin/env python3
"""Package the port's engines and make the builder pages.

  package.py native  --platform linux|windows --exe PATH --out ZIP
  package.py kit     --tcc DIR --out gamekit.zip
  package.py builder --platform windows|linux|android --package PKG --kit gamekit.zip
                     --pyodide DIR --recorder bonanza-sounds.js --out FILE [--version TEXT]

The packages hold the engine alone: the game's code is not in them. A builder page
(one for each platform, a single HTML file that works opened from disk) makes it from
the player's ROM in the browser: the recompiler (tools/, in Pyodide) translates the
game's code to C and TinyCC (build_tcc.sh) builds the library the engine loads
(port/src/gamelib.h); the page puts it, the ROM and the settings into the engine's
package (Android: the APK, signed in the page: builder/apk.js). The page also records
the game's sounds from the ROM (the game's recorder as WebAssembly, port/src/sndrec_web.c,
on every core: builder/sounds.js), so the game has them from its first start.

kit: what the page needs for that, in one zip: the recompiler and the sources it
reads, the headers the game's code is built with, TinyCC for each kind of library.
"""
import argparse
import json
import os
import shutil
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))


def add(zf, path, arcname, mode=0o644):
    info = zipfile.ZipInfo(arcname)
    info.create_system = 3  # unix, so permissions are kept
    info.external_attr = (0o100000 | mode) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    with open(path, "rb") as f:
        zf.writestr(info, f.read())


def native(args):
    folder = "BonanzaBros/"
    exe = "bonanza.exe" if args.platform == "windows" else "bonanza"
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with zipfile.ZipFile(args.out, "w") as zf:
        add(zf, args.exe, folder + exe, 0o755)
        add(zf, os.path.join(HERE, "README.txt"), folder + "README.txt")
        add(zf, os.path.join(HERE, "LICENSES.txt"), folder + "LICENSES.txt")
    print("wrote", args.out)


def kit(args):
    """gamekit.zip: recomp/ (the tools and what they read), include/ (headers), tcc/"""
    files = {}
    for name in ("recomp.py", "z80recomp.py", "disasm.py", "m68k.py", "m68kdec.py", "config.py"):
        files["recomp/tools/" + name] = os.path.join(ROOT, "tools", name)
    files["recomp/port/ext/musashi/m68kops.c"] = os.path.join(ROOT, "port", "ext", "musashi", "m68kops.c")
    files["recomp/port/ext/z80/z80.c"] = os.path.join(ROOT, "port", "ext", "z80", "z80.c")
    files["recomp/port/src/draw.h"] = os.path.join(ROOT, "port", "src", "draw.h")
    files["recomp/port/src/logic.h"] = os.path.join(ROOT, "port", "src", "logic.h")

    def tree(src, to, ext=(".h",)):
        for name in sorted(os.listdir(src)):
            p = os.path.join(src, name)
            if os.path.isfile(p) and (name.endswith(".c") is False if ext is None else name.endswith(ext)):
                files[to + name] = p
    tree(os.path.join(ROOT, "port", "src"), "include/src/")
    tree(os.path.join(ROOT, "port", "gamelib", "include"), "include/gamelib/")
    tree(os.path.join(ROOT, "port", "ext", "musashi"), "include/musashi/")
    tree(os.path.join(ROOT, "port", "ext", "musashi", "softfloat"), "include/musashi/softfloat/", None)
    files["include/ext/z80/z80.h"] = os.path.join(ROOT, "port", "ext", "z80", "z80.h")
    tree(os.path.join(args.tcc, "include"), "include/tcc/")
    for kind in ("win64", "linux64", "arm64", "arm"):
        files["tcc/tcc-%s.js" % kind] = os.path.join(args.tcc, "tcc-%s.js" % kind)
        files["tcc/%s-libtcc1.a" % kind] = os.path.join(args.tcc, "%s-libtcc1.a" % kind)
    files["tcc/COPYING"] = os.path.join(args.tcc, "TINYCC-COPYING")
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with zipfile.ZipFile(args.out, "w", zipfile.ZIP_DEFLATED) as zf:
        for arc, path in sorted(files.items()):
            zf.write(path, arc)
    print("wrote", args.out, "(%.1f MB, %d files)" % (os.path.getsize(args.out) / 1e6, len(files)))


PYODIDE = ("pyodide.mjs", "pyodide.asm.mjs", "pyodide.asm.wasm", "python_stdlib.zip", "pyodide-lock.json")


def builder(args):
    """one builder page for a platform: everything inside, the big parts gzipped"""
    import base64
    import gzip
    b64 = lambda data: base64.b64encode(data).decode("ascii")
    gz = lambda data: b64(gzip.compress(data, 9, mtime=0))
    read = lambda path: open(path, "rb").read()
    page = open(os.path.join(ROOT, "builder", "index.html"), encoding="utf-8").read()
    embed = {"version": args.version or "dev", "platform": args.platform,
             "packages": {args.platform: b64(read(args.package))},
             "res": {"gamekit.zip": gz(read(args.kit)), "bonanza-sounds.js": gz(read(args.recorder))}}
    for name in PYODIDE:
        embed["res"]["pyodide/" + name] = gz(read(os.path.join(args.pyodide, name)))
    for name in ("jszip.min.js", "apk.js", "gamecode.js", "sounds.js"):
        tag = '<script src="%s"></script>' % name
        assert tag in page, "builder/index.html: no " + tag
        js = open(os.path.join(ROOT, "builder", name), encoding="utf-8").read()
        page = page.replace(tag, "<script>" + js.replace("</script", "<\\/script") + "</script>", 1)
    tag = '<script type="application/json" id="bonanza-embedded"></script>'
    assert tag in page, "builder/index.html: no embedded data tag"
    # base64 and JSON: "</" never occurs in them
    page = page.replace(tag, '<script type="application/json" id="bonanza-embedded">' + json.dumps(embed) + "</script>", 1)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        f.write(page)
    print("wrote", args.out, "(%.1f MB)" % (os.path.getsize(args.out) / 1e6))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("native")
    p.add_argument("--platform", choices=["linux", "windows"], required=True)
    p.add_argument("--exe", required=True)
    p.add_argument("--out", required=True)
    p = sub.add_parser("kit")
    p.add_argument("--tcc", required=True)
    p.add_argument("--out", required=True)
    p = sub.add_parser("builder")
    p.add_argument("--platform", choices=["windows", "linux", "android"], required=True)
    p.add_argument("--package", required=True)
    p.add_argument("--kit", required=True)
    p.add_argument("--pyodide", required=True)
    p.add_argument("--recorder", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--version")
    args = ap.parse_args()
    {"native": native, "kit": kit, "builder": builder}[args.cmd](args)


if __name__ == "__main__":
    main()
