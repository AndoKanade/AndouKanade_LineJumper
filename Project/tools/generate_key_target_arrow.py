# 最初の鍵の的の上に出す「▼」の目印の画像を生成するスクリプト(仮素材)
# 使い方: python tools/generate_key_target_arrow.py  (Projectフォルダで実行する。Pillow が必要)
# 出力先: resource/UI/keyTargetArrow.png
# 下向きの三角を、自分のインクの黄色に濃い紺の縁取りで描く(画面の端に出すときは、的のある向きへ回して使う)。

from pathlib import Path

from PIL import Image, ImageDraw

OUTPUT_PATH = Path("resource/UI/keyTargetArrow.png")

# 書き出す画像の一辺のピクセル数と、なめらかに描くために大きく描いてから縮める倍率
IMAGE_SIZE = 128
SUPERSAMPLE = 4
# 三角の幅・高さ(画像の一辺に対する割合)と、縁取りの太さ(同じく割合)
TRIANGLE_WIDTH_RATE = 0.78
TRIANGLE_HEIGHT_RATE = 0.62
OUTLINE_RATE = 0.07

# 色(RGBA)。鍵の的の鍵の絵と同じ黄色と縁取りの色
ARROW_COLOR = (255, 217, 38, 255)
OUTLINE_COLOR = (16, 28, 52, 255)


def triangle(size, width_rate, height_rate):
    """画像の真ん中に、下向きの三角の頂点を求める"""
    center = size / 2
    half_width = size * width_rate / 2
    half_height = size * height_rate / 2
    return [(center - half_width, center - half_height), (center + half_width, center - half_height), (center, center + half_height)]


def generate_arrow():
    size = IMAGE_SIZE * SUPERSAMPLE
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    # 縁取りは三角を太らせて描き、その上に本体を描く
    draw.polygon(triangle(size, TRIANGLE_WIDTH_RATE + OUTLINE_RATE * 2, TRIANGLE_HEIGHT_RATE + OUTLINE_RATE * 2), fill=OUTLINE_COLOR)
    draw.polygon(triangle(size, TRIANGLE_WIDTH_RATE, TRIANGLE_HEIGHT_RATE), fill=ARROW_COLOR)
    image = image.resize((IMAGE_SIZE, IMAGE_SIZE), Image.LANCZOS)
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT_PATH)
    print(f"生成しました: {OUTPUT_PATH}")


if __name__ == "__main__":
    generate_arrow()
