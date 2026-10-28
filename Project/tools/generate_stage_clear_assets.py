# ゴールの的を壊したときに出す「STAGE CLEAR!」の画像を生成するスクリプト(仮素材)
# 使い方: python tools/generate_stage_clear_assets.py  (Projectフォルダで実行する。Pillow が必要)
# 出力先: resource/UI/stageClear.png
# GO!(tools/generate_start_stage_assets.py)と同じ、インクの黄色の太い斜体の文字にそろえる。
# 文言・色は、下の定数を書き換えてから実行し直せば変えられる。

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

# 出力先のフォルダ(Projectフォルダからの相対パス)
OUTPUT_DIR = Path("resource/UI")
# 文字に使うフォント(リポジトリに入っている丸みのある日本語フォント)
FONT_PATH = Path("resource/fonts/MochiyPopPOne-Regular.ttf")

# 文言と、書き出すファイル名
STAGE_CLEAR_TEXT = "STAGE CLEAR!"
STAGE_CLEAR_FILE = "stageClear.png"

# 画像の大きさ(ピクセル)・文字の大きさ・縁取りの太さ
IMAGE_SIZE = (1280, 320)
FONT_SIZE = 128
OUTLINE_WIDTH = 14
# 文字の上側にハイライトを重ねる範囲(文字の高さに対する割合)と、ハイライトの濃さ(0〜255)
HIGHLIGHT_HEIGHT_RATE = 0.42
HIGHLIGHT_ALPHA = 150
# 右へ傾ける量(斜体)
SHEAR = 0.18
# 外側の淡い光のぼかしの半径と濃さの割合
GLOW_BLUR_RADIUS = 18
GLOW_ALPHA_RATE = 0.6

# 色(0〜255)。自分のインクの黄色(Player の kPlayerColor と同じ色)を基調にする
INK_YELLOW = (255, 217, 38)
HIGHLIGHT_COLOR = (255, 245, 190, 255)
DARK_NAVY = (20, 34, 48)


def generate_stage_clear():
    """インクの黄色の太い文字に、濃い縁取りと斜体の傾き、外側の淡い光を付ける"""
    width, height = IMAGE_SIZE
    font = ImageFont.truetype(str(FONT_PATH), FONT_SIZE)
    text_layer = Image.new("RGBA", IMAGE_SIZE, (0, 0, 0, 0))
    draw = ImageDraw.Draw(text_layer)
    box = draw.textbbox((0, 0), STAGE_CLEAR_TEXT, font=font, stroke_width=OUTLINE_WIDTH)
    origin = ((width - (box[2] - box[0])) / 2 - box[0], (height - (box[3] - box[1])) / 2 - box[1])
    draw.text(origin, STAGE_CLEAR_TEXT, font=font, fill=INK_YELLOW, stroke_width=OUTLINE_WIDTH, stroke_fill=DARK_NAVY)

    # 文字の上側に白っぽいハイライトを重ねて、つやのあるインクに見せる
    highlight = Image.new("RGBA", IMAGE_SIZE, (0, 0, 0, 0))
    ImageDraw.Draw(highlight).text(origin, STAGE_CLEAR_TEXT, font=font, fill=HIGHLIGHT_COLOR)
    mask = Image.new("L", IMAGE_SIZE, 0)
    ImageDraw.Draw(mask).rectangle([0, 0, width, int(origin[1] + box[1] + (box[3] - box[1]) * HIGHLIGHT_HEIGHT_RATE)], fill=HIGHLIGHT_ALPHA)
    highlight.putalpha(Image.composite(highlight.getchannel("A"), Image.new("L", IMAGE_SIZE, 0), mask))
    text_layer = Image.alpha_composite(text_layer, highlight)

    # 斜体のように右へ傾ける
    text_layer = text_layer.transform(IMAGE_SIZE, Image.AFFINE, (1, SHEAR, -SHEAR * height / 2, 0, 1, 0), resample=Image.BICUBIC)

    # 外側の淡い光
    glow = Image.new("RGBA", IMAGE_SIZE, INK_YELLOW + (0,))
    glow.putalpha(text_layer.getchannel("A").filter(ImageFilter.GaussianBlur(GLOW_BLUR_RADIUS)).point(lambda a: int(a * GLOW_ALPHA_RATE)))
    image = Image.alpha_composite(glow, text_layer)
    image.save(OUTPUT_DIR / STAGE_CLEAR_FILE)


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    generate_stage_clear()
    print(f"生成しました: {OUTPUT_DIR / STAGE_CLEAR_FILE}")


if __name__ == "__main__":
    main()
