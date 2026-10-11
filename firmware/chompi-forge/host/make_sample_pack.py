#!/usr/bin/env python3
"""Render Choowi's sample pack (host/sample_pack/pack.js) with Web Audio in headless Chromium.

Writes TAPE-named WAVs for bank a, purple by default (jammi_a1-14 chromatic at C4, cubbi_a1-14 kit; 48 kHz 16-bit mono),
a README with the slot list and licence, a listening page and choowi-samples.zip for the
release. Needs Playwright with Chromium (pip install playwright; playwright install chromium).
Seeded: the same script gives the same sound (bytes may differ by one 16-bit step between runs).

    python host/make_sample_pack.py [--out samples/choowi-pack]
"""
import argparse
import base64
import html
from pathlib import Path
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
LICENCE = ("Choowi's sample pack, made for Forge by DeeCee and Claude with Web Audio (host/sample_pack/pack.js).\n"
           "Released under CC0 1.0: no rights reserved, use it for anything, no credit needed.\n"
           "https://creativecommons.org/publicdomain/zero/1.0/\n")


def render(bank):
    from playwright.sync_api import sync_playwright
    with sync_playwright() as pw:
        browser = pw.chromium.launch()
        page = browser.new_page()
        page.goto("about:blank")
        page.add_script_tag(path=str(HERE / "sample_pack" / "pack.js"))
        files = page.evaluate("bank => renderPack(bank)", bank)
        browser.close()
    return files


COLOURS = dict(zip("abcde", ("purple", "orange", "teal", "red", "lime")))   # CHOMPI's sample bank colours (0.19.2)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=ROOT / "samples" / "choowi-pack")
    parser.add_argument("--bank", default="a", choices="abcde", help="sample bank (default a, purple)")
    args = parser.parse_args()
    out, bank, colour = args.out, args.bank, COLOURS[args.bank]
    out.mkdir(parents=True, exist_ok=True)
    files = render(bank)
    for f in files:
        (out / f["file"]).write_bytes(base64.b64decode(f["wav"]))
    rows = "\n".join(f"| `{f['file']}` | {f['name']} | {f['seconds']:.2f} s |" for f in files)
    (out / "README.md").write_text(
        f"# Choowi's sample pack\n\nForge's default samples: chromatic and kit bank **{colour}** (bank {bank}) on CHOMPI's SD card, in\n"
        "TAPE's layout (Forge and TAPE both play them). Copy the `.wav` files to the card's top folder (they replace\n"
        "files of the same name). On CHOMPI: toggle down, hold CHOMPI, TAPE's page (teal light); press **KEY_16**\n"
        f"until its light is {colour} for the chromatic bank, then a white key picks a sound; **KEY_17** for the kit.\n"
        "Chromatic sounds play at their own pitch on middle C.\n\n| File | Sound | Length |\n| --- | --- | --- |\n"
        f"{rows}\n\n48 kHz, 16-bit mono, peak -1 dBFS. Regenerate: `python host/make_sample_pack.py`.\n\n## Licence\n\n{LICENCE}",
        encoding="utf-8")
    items = "\n".join(f'<li><b>{html.escape(f["file"])}</b> {html.escape(f["name"])}<audio controls preload="none" src="{f["file"]}"></audio></li>'
                      for f in files)
    (out / "listen.html").write_text(
        '<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">'
        "<title>Choowi's sample pack</title><style>body{font-family:system-ui,sans-serif;max-width:760px;margin:auto;padding:16px;"
        "background:#fbfaf7;color:#1b1b22}li{display:grid;grid-template-columns:110px 1fr;gap:4px 12px;align-items:center;margin:10px 0}"
        "audio{grid-column:1/-1;width:100%}@media(prefers-color-scheme:dark){body{background:#16161c;color:#f4f2ec}}</style></head>"
        f"<body><h1>Choowi's sample pack</h1><p>Chromatic bank {colour} (played at C4), then kit bank {colour}.</p><ol>{items}</ol></body></html>",
        encoding="utf-8")
    with zipfile.ZipFile(out.parent / "choowi-samples.zip", "w", zipfile.ZIP_DEFLATED) as z:
        for f in files: z.write(out / f["file"], f["file"])
        z.write(out / "README.md", "README.md")
    print(f"{len(files)} samples in {out}; zip: {out.parent / 'choowi-samples.zip'}")


if __name__ == "__main__":
    main()
