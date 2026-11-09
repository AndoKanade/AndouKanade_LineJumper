# 鍵の的を壊して出たレールへ飛び移れるときに、画面の左右に出す「A + SPACE」「D + SPACE」の案内の画像を生成するスクリプト(仮素材)
# 使い方: python tools/generate_rail_switch_prompt.py  (Projectフォルダで実行する。Pillow が必要)
# 出力先: resource/UI/railSwitchLeft.png / railSwitchRight.png
# 操作の案内(tools/generate_tutorial_hints.py)と同じ、キーの名前を白い枠で囲んだ見た目に、飛び移る向きの矢印を付ける。

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUTPUT_DIR = Path("resource/UI")
FONT_PATH = Path("resource/fonts/MochiyPopPOne-Regular.ttf")

# 書き出すファイル名・押すキー・矢印の向き(-1: 左、1: 右)
PROMPTS = [
    ("railSwitchLeft.png", "A", -1),
    ("railSwitchRight.png", "D", 1),
]
JUMP_KEY = "SPACE"
# 下に添える文
CAPTION = "となりのレールへ!"

# 画像の大きさ(ピクセル)
IMAGE_SIZE = (720, 280)
# 下地の角の丸みと縁の太さ
PANEL_RADIUS = 36
PANEL_EDGE_WIDTH = 6
# キーの枠の高さ・左右の余白・角の丸み、キーの文字の大きさ、「+」と添える文の文字の大きさ
KEY_HEIGHT = 96
KEY_PADDING_X = 26
KEY_RADIUS = 18
KEY_FONT_SIZE = 64
PLUS_FONT_SIZE = 60
CAPTION_FONT_SIZE = 40
# キーと「+」の間隔
GAP = 18
# キーの並びと添える文の中心の高さ(画像の高さに対する割合)
KEY_CENTER_RATE = 0.4
CAPTION_CENTER_RATE = 0.8
# 矢印の長さ・太さ・頭の大きさと、キーの並びとの間隔
ARROW_LENGTH = 70
ARROW_WIDTH = 16
ARROW_HEAD = 30
ARROW_GAP = 22

# 色(RGBA)。操作の案内と同じ色にそろえ、矢印は自分のインクの黄色にする
PANEL_COLOR = (16, 22, 34, 225)
PANEL_EDGE_COLOR = (255, 214, 64, 255)
TEXT_COLOR = (245, 245, 245, 255)
KEY_COLOR = (245, 245, 245, 255)
KEY_TEXT_COLOR = (20, 26, 40, 255)
ARROW_COLOR = (255, 214, 64, 255)


def key_width(draw, label, font):
    return draw.textlength(label, font=font) + KEY_PADDING_X * 2


def draw_key(draw, left, center_y, label, font):
    right = left + key_width(draw, label, font)
    draw.rounded_rectangle((left, center_y - KEY_HEIGHT / 2, right, center_y + KEY_HEIGHT / 2), radius=KEY_RADIUS, fill=KEY_COLOR)
    draw.text(((left + right) / 2, center_y), label, font=font, fill=KEY_TEXT_COLOR, anchor="mm")
    return right


def draw_arrow(draw, start_x, center_y, direction):
    """start_x から direction の向きへ伸びる矢印を描く"""
    tip_x = start_x + direction * ARROW_LENGTH
    base_x = tip_x - direction * ARROW_HEAD
    draw.rectangle((min(start_x, base_x), center_y - ARROW_WIDTH / 2, max(start_x, base_x), center_y + ARROW_WIDTH / 2), fill=ARROW_COLOR)
    draw.polygon([(tip_x, center_y), (base_x, center_y - ARROW_HEAD), (base_x, center_y + ARROW_HEAD)], fill=ARROW_COLOR)


def generate_prompt(file_name, side_key, direction):
    width, height = IMAGE_SIZE
    image = Image.new("RGBA", IMAGE_SIZE, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((0, 0, width - 1, height - 1), radius=PANEL_RADIUS, fill=PANEL_COLOR, outline=PANEL_EDGE_COLOR, width=PANEL_EDGE_WIDTH)

    key_font = ImageFont.truetype(str(FONT_PATH), KEY_FONT_SIZE)
    plus_font = ImageFont.truetype(str(FONT_PATH), PLUS_FONT_SIZE)
    caption_font = ImageFont.truetype(str(FONT_PATH), CAPTION_FONT_SIZE)
    center_y = height * KEY_CENTER_RATE

    # 「キー + SPACE」と矢印をまとめた幅を求めて、左右の真ん中に置く(矢印は飛び移る向きの側に付ける)
    plus_width = draw.textlength("+", font=plus_font)
    keys_width = key_width(draw, side_key, key_font) + GAP + plus_width + GAP + key_width(draw, JUMP_KEY, key_font)
    total_width = keys_width + ARROW_GAP + ARROW_LENGTH
    left = (width - total_width) / 2
    if direction < 0:
        draw_arrow(draw, left + ARROW_LENGTH, center_y, direction)
        left += ARROW_LENGTH + ARROW_GAP

    x = draw_key(draw, left, center_y, side_key, key_font) + GAP
    draw.text((x + plus_width / 2, center_y), "+", font=plus_font, fill=TEXT_COLOR, anchor="mm")
    x = draw_key(draw, x + plus_width + GAP, center_y, JUMP_KEY, key_font)
    if direction > 0:
        draw_arrow(draw, x + ARROW_GAP, center_y, direction)

    draw.text((width / 2, height * CAPTION_CENTER_RATE), CAPTION, font=caption_font, fill=TEXT_COLOR, anchor="mm")
    image.save(OUTPUT_DIR / file_name)
    print(f"生成しました: {OUTPUT_DIR / file_name}")


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    for file_name, side_key, direction in PROMPTS:
        generate_prompt(file_name, side_key, direction)


if __name__ == "__main__":
    main()
