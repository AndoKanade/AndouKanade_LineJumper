# プレイ会用の設定のときに、遊べないステージのゲートの前で出す「いまは遊べないよ!!」の画像を生成するスクリプト(仮素材)
# 使い方: python tools/generate_stage_locked_notice.py  (Projectフォルダで実行する。Pillow が必要)
# 出力先: resource/UI/stageLocked.png
# 「STAGE CLEAR!」(tools/generate_stage_clear_assets.py)と同じ、濃い縁取りの太い文字にそろえる。
# 文言・色は、下の定数を書き換えてから実行し直せば変えられる。

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

# 出力先のファイル(Projectフォルダからの相対パス)
OUTPUT_PATH = Path("resource/UI/stageLocked.png")
# 文字に使うフォント(リポジトリに入っている丸みのある日本語フォント)
FONT_PATH = Path("resource/fonts/MochiyPopPOne-Regular.ttf")

# 文言(大きい見出しと、その下の小さい案内)
MAIN_TEXT = "いまは遊べないよ!!"
SUB_TEXT = "正面のゲートのステージ1を遊んでね"

# 画像の大きさ(ピクセル)
IMAGE_SIZE = (1280, 400)
# 見出し・案内の文字の大きさと縁取りの太さ
MAIN_FONT_SIZE = 120
MAIN_OUTLINE_WIDTH = 12
SUB_FONT_SIZE = 52
SUB_OUTLINE_WIDTH = 8
# 見出し・案内の中心の高さ(画像の高さに対する割合)
MAIN_CENTER_RATE = 0.4
SUB_CENTER_RATE = 0.8
# 見出しを右へ傾ける量(斜体)
SHEAR = 0.15
# 外側の淡い光のぼかしの半径と濃さの割合
GLOW_BLUR_RADIUS = 16
GLOW_ALPHA_RATE = 0.5

# 色(0〜255)。見出しは敵のインクに近いピンクにして、まだ行けないことを伝える
MAIN_COLOR = (255, 102, 191)
SUB_COLOR = (255, 255, 255)
DARK_NAVY = (20, 34, 48)


def draw_centered(layer, text, font, center_y, fill, outline_width):
    """文字を左右の中央、高さ center_y に縁取り付きで描く"""
    draw = ImageDraw.Draw(layer)
    box = draw.textbbox((0, 0), text, font=font, stroke_width=outline_width)
    origin = ((layer.width - (box[2] - box[0])) / 2 - box[0], center_y - (box[3] - box[1]) / 2 - box[1])
    draw.text(origin, text, font=font, fill=fill, stroke_width=outline_width, stroke_fill=DARK_NAVY)


def generate_stage_locked():
    width, height = IMAGE_SIZE

    # 見出しは斜体のように右へ傾け、勢いを出す
    main_layer = Image.new("RGBA", IMAGE_SIZE, (0, 0, 0, 0))
    draw_centered(main_layer, MAIN_TEXT, ImageFont.truetype(str(FONT_PATH), MAIN_FONT_SIZE), height * MAIN_CENTER_RATE, MAIN_COLOR, MAIN_OUTLINE_WIDTH)
    main_layer = main_layer.transform(IMAGE_SIZE, Image.AFFINE, (1, SHEAR, -SHEAR * height * MAIN_CENTER_RATE, 0, 1, 0), resample=Image.BICUBIC)

    # 案内はまっすぐ読みやすく描く
    sub_layer = Image.new("RGBA", IMAGE_SIZE, (0, 0, 0, 0))
    draw_centered(sub_layer, SUB_TEXT, ImageFont.truetype(str(FONT_PATH), SUB_FONT_SIZE), height * SUB_CENTER_RATE, SUB_COLOR, SUB_OUTLINE_WIDTH)
    text_layer = Image.alpha_composite(main_layer, sub_layer)

    # 見出しの外側の淡い光
    glow = Image.new("RGBA", IMAGE_SIZE, MAIN_COLOR + (0,))
    glow.putalpha(main_layer.getchannel("A").filter(ImageFilter.GaussianBlur(GLOW_BLUR_RADIUS)).point(lambda a: int(a * GLOW_ALPHA_RATE)))
    image = Image.alpha_composite(glow, text_layer)

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT_PATH)
    print(f"生成しました: {OUTPUT_PATH}")


if __name__ == "__main__":
    generate_stage_locked()
