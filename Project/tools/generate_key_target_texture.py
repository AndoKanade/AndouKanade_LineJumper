# 鍵の的(壊すとレールが出る的)のテクスチャを生成するスクリプト(仮素材)
# 使い方: python tools/generate_key_target_texture.py  (Projectフォルダで実行する。Pillow が必要)
# 出力先: resource/Bullseye/keyTarget.png
# 普通の的(緑・オレンジの bullseye.png)とひと目で見分けられるよう、明るい水色の輪と濃い青の真ん中に、黄色い鍵の絵を大きく描く。
# モデルは普通の的と同じ円盤で、画像の中央の円(直径は画像の幅とほぼ同じ)が的の面に貼られる。画像の上が的の上になる。

from pathlib import Path

from PIL import Image, ImageDraw

OUTPUT_PATH = Path("resource/Bullseye/keyTarget.png")

# 書き出す画像の一辺のピクセル数と、なめらかに描くために大きく描いてから縮める倍率
IMAGE_SIZE = 512
SUPERSAMPLE = 2

# 的の面に貼られる円の半径(画像の一辺に対する割合。モデルの UV と同じ)
DISC_RADIUS_RATE = 0.4975
# 輪の境目の半径(的の半径に対する割合。外側から 水色 → 白 → 濃い青)
WHITE_RING_OUTER_RATE = 0.78
INNER_DISC_RATE = 0.70

# 鍵の絵の大きさ(的の半径に対する割合)
KEY_HEAD_CENTER_Y_RATE = -0.26   # 鍵の頭(輪)の中心の高さ(上が負)
KEY_HEAD_RADIUS_RATE = 0.24      # 鍵の頭の外側の半径
KEY_HEAD_HOLE_RATE = 0.10        # 鍵の頭の穴の半径
KEY_SHAFT_HALF_WIDTH_RATE = 0.075  # 鍵の軸の太さの半分
KEY_SHAFT_BOTTOM_RATE = 0.50     # 鍵の軸の下の端の高さ
KEY_TOOTH_LENGTH_RATE = 0.16     # 鍵の歯の横への長さ
KEY_TOOTH_HEIGHT_RATE = 0.08     # 鍵の歯の高さ
KEY_TOOTH_POSITIONS = (0.28, 0.42)  # 鍵の歯を付ける高さ
KEY_OUTLINE_RATE = 0.035         # 鍵の縁取りの太さ

# 色(RGBA)
BACKGROUND_COLOR = (16, 40, 96, 255)    # 円の外(的の側面に貼られる)
OUTER_RING_COLOR = (40, 200, 255, 255)  # 外側の輪(明るい水色)
WHITE_RING_COLOR = (245, 250, 255, 255) # 白い輪
INNER_DISC_COLOR = (24, 70, 170, 255)   # 真ん中(濃い青)
KEY_COLOR = (255, 217, 38, 255)         # 鍵(自分のインクと同じ黄色)
KEY_OUTLINE_COLOR = (16, 28, 52, 255)   # 鍵の縁取り(濃い紺)


def draw_circle(draw, center, radius, fill):
    draw.ellipse((center - radius, center - radius, center + radius, center + radius), fill=fill)


def draw_key(draw, center, disc_radius, grow, fill):
    """鍵の絵(輪の頭・軸・2本の歯)を描く。grow だけ太らせて描くと縁取りになる"""
    head_center_y = center + disc_radius * KEY_HEAD_CENTER_Y_RATE
    head_radius = disc_radius * KEY_HEAD_RADIUS_RATE + grow
    draw.ellipse((center - head_radius, head_center_y - head_radius, center + head_radius, head_center_y + head_radius), fill=fill)

    shaft_half_width = disc_radius * KEY_SHAFT_HALF_WIDTH_RATE + grow
    shaft_bottom = center + disc_radius * KEY_SHAFT_BOTTOM_RATE + grow
    draw.rectangle((center - shaft_half_width, head_center_y, center + shaft_half_width, shaft_bottom), fill=fill)

    tooth_length = disc_radius * KEY_TOOTH_LENGTH_RATE
    tooth_half_height = disc_radius * KEY_TOOTH_HEIGHT_RATE / 2 + grow
    for position in KEY_TOOTH_POSITIONS:
        tooth_center_y = center + disc_radius * position
        draw.rectangle((center, tooth_center_y - tooth_half_height, center + shaft_half_width + tooth_length + grow, tooth_center_y + tooth_half_height), fill=fill)


def generate_key_target():
    size = IMAGE_SIZE * SUPERSAMPLE
    center = size / 2
    disc_radius = size * DISC_RADIUS_RATE
    image = Image.new("RGBA", (size, size), BACKGROUND_COLOR)
    draw = ImageDraw.Draw(image)

    # 外側から順に輪を重ねる
    draw_circle(draw, center, disc_radius, OUTER_RING_COLOR)
    draw_circle(draw, center, disc_radius * WHITE_RING_OUTER_RATE, WHITE_RING_COLOR)
    draw_circle(draw, center, disc_radius * INNER_DISC_RATE, INNER_DISC_COLOR)

    # 鍵は縁取りを先に太く描き、その上に本体を描く。頭の穴は真ん中の色で抜く
    outline = disc_radius * KEY_OUTLINE_RATE
    draw_key(draw, center, disc_radius, outline, KEY_OUTLINE_COLOR)
    draw_key(draw, center, disc_radius, 0.0, KEY_COLOR)
    head_center_y = center + disc_radius * KEY_HEAD_CENTER_Y_RATE
    hole_radius = disc_radius * KEY_HEAD_HOLE_RATE
    draw.ellipse((center - hole_radius - outline, head_center_y - hole_radius - outline, center + hole_radius + outline, head_center_y + hole_radius + outline), fill=KEY_OUTLINE_COLOR)
    draw.ellipse((center - hole_radius, head_center_y - hole_radius, center + hole_radius, head_center_y + hole_radius), fill=INNER_DISC_COLOR)

    image = image.resize((IMAGE_SIZE, IMAGE_SIZE), Image.LANCZOS)
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT_PATH)
    print(f"生成しました: {OUTPUT_PATH}")


if __name__ == "__main__":
    generate_key_target()
