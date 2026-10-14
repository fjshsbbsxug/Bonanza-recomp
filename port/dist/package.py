#!/usr/bin/env python3
"""Package port builds and assemble the builder website.

  package.py native  --platform linux|windows --exe PATH --out ZIP
  package.py web     --build DIR --out DIR
  package.py site    --out DIR [--version TEXT]
  package.py single  --site DIR --out FILE

single: the builder as one HTML file (BonanzaBuilder.html) that carries the runtimes
(the site's packages and web/bonanza-single.js) and the zip library inside it, so it
works opened from disk with no web server or network.

The site layout is what builder/index.html expects:
  index.html, levelmaker.html (+ levelcore.js, jszip.min.js), play.html, record.html
  packages/manifest.json
  packages/bonanza-windows-x64.zip, bonanza-linux-x64.zip, bonanza-android.apk
  web/bonanza.js, web/bonanza.wasm, web/bonanza-single.js (level maker test play)
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


def web(args):
    os.makedirs(args.out, exist_ok=True)
    for name in ("bonanza.js", "bonanza.wasm", "bonanza-single.js"):
        if name == "bonanza-single.js" and not os.path.exists(os.path.join(args.build, name)):
            continue                     # (older build directories)
        shutil.copy(os.path.join(args.build, name), os.path.join(args.out, name))
    print("copied web runtime to", args.out)


def site(args):
    out = args.out
    os.makedirs(os.path.join(out, "packages"), exist_ok=True)
    for name in ("index.html", "jszip.min.js", "levelmaker.html", "levelcore.js", "play.html", "record.html"):
        shutil.copy(os.path.join(ROOT, "builder", name), os.path.join(out, name))
    pk = {}
    for key, name in (("windows", "packages/bonanza-windows-x64.zip"), ("linux", "packages/bonanza-linux-x64.zip"),
                      ("android", "packages/bonanza-android.apk"), ("web", "web/bonanza.js")):
        if os.path.exists(os.path.join(out, name)):
            pk[key] = name
    manifest = {"version": args.version or "dev", "packages": pk}
    with open(os.path.join(out, "packages", "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
    print("site ready in", out, "with", ", ".join(sorted(pk)) or "no packages")


def single(args):
    import base64
    site_dir = args.site
    with open(os.path.join(ROOT, "builder", "index.html"), encoding="utf-8") as f:
        page = f.read()
    with open(os.path.join(ROOT, "builder", "jszip.min.js"), encoding="utf-8") as f:
        jszip = f.read()
    b64 = lambda path: base64.b64encode(open(path, "rb").read()).decode("ascii")
    try:
        with open(os.path.join(site_dir, "packages", "manifest.json")) as f:
            version = json.load(f).get("version", "dev")
    except OSError:
        version = "dev"
    embed = {"version": version, "packages": {}}
    for key, name in (("windows", "packages/bonanza-windows-x64.zip"), ("linux", "packages/bonanza-linux-x64.zip"),
                      ("android", "packages/bonanza-android.apk")):
        path = os.path.join(site_dir, name)
        if os.path.exists(path):
            embed["packages"][key] = b64(path)
    web = os.path.join(site_dir, "web", "bonanza-single.js")
    if os.path.exists(web):
        embed["web"] = b64(web)
    embed["record"] = b64(os.path.join(ROOT, "builder", "record.html"))
    tag = '<script src="jszip.min.js"></script>'
    assert tag in page, "builder/index.html: the jszip script tag moved"
    # the zip library and the runtimes in the page ("</" never occurs in base64 or JSON here)
    page = page.replace(tag, "<script>" + jszip.replace("</script", "<\\/script") + "</script>\n"
                        + '<script type="application/json" id="bonanza-embedded">' + json.dumps(embed) + "</script>", 1)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        f.write(page)
    print("wrote", args.out, "(%.1f MB) with" % (os.path.getsize(args.out) / 1e6),
          ", ".join(sorted(embed["packages"]) + (["web"] if "web" in embed else [])))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("native")
    p.add_argument("--platform", choices=["linux", "windows"], required=True)
    p.add_argument("--exe", required=True)
    p.add_argument("--out", required=True)
    p = sub.add_parser("web")
    p.add_argument("--build", required=True)
    p.add_argument("--out", required=True)
    p = sub.add_parser("site")
    p.add_argument("--out", required=True)
    p.add_argument("--version")
    p = sub.add_parser("single")
    p.add_argument("--site", required=True)
    p.add_argument("--out", required=True)
    args = ap.parse_args()
    {"native": native, "web": web, "site": site, "single": single}[args.cmd](args)


if __name__ == "__main__":
    main()
