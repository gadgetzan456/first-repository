"""ショート動画の確認用画像を集める。

使い方:
    python3 shorts/tools/fetch_images.py shorts/<動画名>/images.tsv

images.tsv（タブ区切り）の各行の url から画像を取得し、
同じフォルダの ref_images/ に保存する。Pillow があれば、
全画像を番号つきで並べた確認用シート ref_images/_sheet.png も作る。
ref_images/ は .gitignore 済み（画像はリポジトリにコミットしない）。
"""
import csv
import sys
import urllib.request
from pathlib import Path

UA = "Mozilla/5.0"


def fetch(rows, out):
    out.mkdir(exist_ok=True)
    ok = []
    for r in rows:
        dest = out / r["file"]
        try:
            req = urllib.request.Request(r["url"], headers={"User-Agent": UA, "Referer": r["source_page"]})
            with urllib.request.urlopen(req, timeout=30) as res:
                dest.write_bytes(res.read())
            print(f"OK   {r['no']:>2} {r['label']} -> {dest}")
            ok.append((r, dest))
        except Exception as e:
            print(f"FAIL {r['no']:>2} {r['label']}: {e}  (出典ページから手動で保存: {r['source_page']})")
    return ok


def sheet(items, path):
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        print("Pillow がないので確認用シートは作らない（pip install pillow）")
        return
    font = None
    for f in ["/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
              "C:/Windows/Fonts/meiryo.ttc", "C:/Windows/Fonts/msgothic.ttc",
              "/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc"]:
        try:
            font = ImageFont.truetype(f, 18)
            break
        except OSError:
            pass
    font = font or ImageFont.load_default()
    cell, cap, cols = 220, 44, 4
    rows = (len(items) + cols - 1) // cols
    img = Image.new("RGB", (cols * cell, rows * (cell + cap)), "white")
    d = ImageDraw.Draw(img)
    for i, (r, p) in enumerate(items):
        x, y = (i % cols) * cell, (i // cols) * (cell + cap)
        try:
            im = Image.open(p).convert("RGBA")
            im.thumbnail((cell - 20, cell - 20))
            bg = Image.new("RGBA", im.size, "white")
            img.paste(Image.alpha_composite(bg, im).convert("RGB"),
                      (x + (cell - im.width) // 2, y + (cell - im.height) // 2))
        except Exception as e:
            d.text((x + 10, y + 10), f"読めない: {e}", fill="red", font=font)
        d.text((x + 8, y + cell + 4), f"{r['no']}. {r['label']}"[:14], fill="black", font=font)
        d.rectangle([x, y, x + cell - 1, y + cell + cap - 1], outline="#ccc")
    img.save(path)
    print(f"確認用シート -> {path}")


def main():
    tsv = Path(sys.argv[1])
    with tsv.open(encoding="utf-8") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    out = tsv.parent / "ref_images"
    sheet(fetch(rows, out), out / "_sheet.png")


if __name__ == "__main__":
    main()
