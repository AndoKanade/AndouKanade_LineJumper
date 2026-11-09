# 場面ごとに画面の上に出す操作の案内(チュートリアル)の画像を生成するスクリプト(仮素材)
# 使い方: python tools/generate_tutorial_hints.py  (Projectフォルダで実行する。Pillow が必要)
# 出力先: resource/UI/hintMove.png など(下の HINTS に並べたもの)
# 操作説明の画像(tools/generate_help_texture.py)と同じ、暗い紺の下地にキーの名前を白い枠で囲んだ見た目にそろえる。
# 文言を変えるときは、下の HINTS を書き換えて実行し直す。

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

# 出力先のフォルダ(Projectフォルダからの相対パス)
OUTPUT_DIR = Path("resource/UI")
# 文字に使うフォント(リポジトリに入っている丸みのある日本語フォント)
FONT_PATH = Path("resource/fonts/MochiyPopPOne-Regular.ttf")

# 書き出すファイル名と、案内の文(キーの名前は ("key", 名前)、ふつうの文は ("text", 文) で並べる)
HINTS = [
    ("hintMove.png", [("key", "WASD"), ("text", "で移動　"), ("key", "マウス"), ("text", "で見回す")]),
    ("hintShoot.png", [("key", "左クリック"), ("text", "でインクを撃つ　台を塗ってみよう")]),
    ("hintGate.png", [("text", "ゲートの前の目印で"), ("key", "SPACE"), ("text", "を押して出発!")]),
    ("hintRailShoot.png", [("text", "的を"), ("key", "左クリック"), ("text", "で撃ちながら進もう!")]),
]

# 画像の大きさ(ピクセル)。文の長さによらず同じ大きさにして、表示する大きさをそろえる
IMAGE_SIZE = (1200, 120)
# 下地の角の丸みと縁の太さ
PANEL_RADIUS = 28
PANEL_EDGE_WIDTH = 4
# 文字の大きさと、キーの枠の高さ・左右の余白・角の丸み・枠と文の間隔
FONT_SIZE = 44
KEY_FONT_SIZE = 38
KEY_HEIGHT = 62
KEY_PADDING_X = 16
KEY_RADIUS = 12
KEY_MARGIN = 10

# 色(RGBA)。操作説明の画像と同じ色にそろえる
PANEL_COLOR = (16, 22, 34, 215)         # 下地(暗い紺の半透明)
PANEL_EDGE_COLOR = (255, 214, 64, 255)  # 下地の縁(インクの黄色)
TEXT_COLOR = (245, 245, 245, 255)       # 文
KEY_COLOR = (245, 245, 245, 255)        # キーの枠
KEY_TEXT_COLOR = (20, 26, 40, 255)      # キーの名前


def measure(draw, parts, font, key_font):
    """文全体の幅を求める(キーは枠の余白と前後の間隔も含める)"""
    width = 0
    for kind, text in parts:
        if kind == "key":
            width += draw.textlength(text, font=key_font) + KEY_PADDING_X * 2 + KEY_MARGIN * 2
        else:
            width += draw.textlength(text, font=font)
    return width


def generate_hint(file_name, parts):
    width, height = IMAGE_SIZE
    image = Image.new("RGBA", IMAGE_SIZE, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((0, 0, width - 1, height - 1), radius=PANEL_RADIUS, fill=PANEL_COLOR, outline=PANEL_EDGE_COLOR, width=PANEL_EDGE_WIDTH)

    font = ImageFont.truetype(str(FONT_PATH), FONT_SIZE)
    key_font = ImageFont.truetype(str(FONT_PATH), KEY_FONT_SIZE)
    center_y = height / 2
    # 文全体を左右の真ん中に置く
    x = (width - measure(draw, parts, font, key_font)) / 2
    for kind, text in parts:
        if kind == "key":
            x += KEY_MARGIN
            right = x + draw.textlength(text, font=key_font) + KEY_PADDING_X * 2
            draw.rounded_rectangle((x, center_y - KEY_HEIGHT / 2, right, center_y + KEY_HEIGHT / 2), radius=KEY_RADIUS, fill=KEY_COLOR)
            draw.text((x + KEY_PADDING_X, center_y), text, font=key_font, fill=KEY_TEXT_COLOR, anchor="lm")
            x = right + KEY_MARGIN
        else:
            draw.text((x, center_y), text, font=font, fill=TEXT_COLOR, anchor="lm")
            x += draw.textlength(text, font=font)

    image.save(OUTPUT_DIR / file_name)
    print(f"生成しました: {OUTPUT_DIR / file_name}")


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    for file_name, parts in HINTS:
        generate_hint(file_name, parts)


if __name__ == "__main__":
    main()
