# クリア画面・ゲームオーバー画面で、今どの画面なのかを示す画像と、ゲームオーバー画面の選択肢の画像を生成するスクリプト(仮素材)
# 使い方: python tools/generate_result_assets.py  (Projectフォルダで実行する。Pillow が必要)
# 出力先: resource/UI/clearTitle.png、resource/UI/gameOverTitle.png、resource/UI/continueOption.png、resource/UI/toTitleOption.png

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

# 出力先のフォルダ(Projectフォルダからの相対パス)
OUTPUT_DIR = Path("resource/UI")
# 文字に使うフォント(リポジトリに入っている丸みのある日本語フォント)
FONT_PATH = Path("resource/fonts/MochiyPopPOne-Regular.ttf")

# 画像の大きさ(ピクセル)
IMAGE_SIZE = (1024, 320)
# 画面の名前の文字の大きさ・縁取りの太さ・中心の高さ
TITLE_FONT_SIZE = 108
TITLE_OUTLINE_WIDTH = 12
TITLE_CENTER_Y = 120
# 操作の案内の文字の大きさ・縁取りの太さ・中心の高さ
HINT_FONT_SIZE = 44
HINT_OUTLINE_WIDTH = 6
HINT_CENTER_Y = 270

# 色(0〜255)
INK_YELLOW = (255, 217, 38, 255)
ENEMY_PINK = (255, 51, 140, 255)
DARK_NAVY = (20, 34, 48, 255)
WHITE = (255, 255, 255, 255)

# 画面ごとの文言と色(ファイル名, 画面の名前, 文字の色, 操作の案内)
RESULTS = [
    ("clearTitle.png", "GAME CLEAR!", INK_YELLOW, "SPACE でタイトルへ"),
    ("gameOverTitle.png", "GAME OVER", ENEMY_PINK, "W/S でえらんで SPACE で決定"),
]

# ゲームオーバー画面の選択肢の画像の大きさ・文字の大きさ・縁取りの太さ
# 選んでいるものはゲーム側で色を付けるため、文字は白で描く
OPTION_SIZE = (640, 100)
OPTION_FONT_SIZE = 64
OPTION_OUTLINE_WIDTH = 8
# 選択肢ごとの文言(ファイル名, 文言)
OPTIONS = [
    ("continueOption.png", "コンティニュー"),
    ("toTitleOption.png", "タイトルへ"),
]


def draw_centered(draw, text, font, center_y, fill, outline_width, image_width=IMAGE_SIZE[0]):
    """文字列を画像の横方向の中央・指定した高さを中心にして、濃い縁取り付きで描く"""
    box = draw.textbbox((0, 0), text, font=font, stroke_width=outline_width)
    x = (image_width - (box[2] - box[0])) / 2 - box[0]
    y = center_y - (box[3] - box[1]) / 2 - box[1]
    draw.text((x, y), text, font=font, fill=fill, stroke_width=outline_width, stroke_fill=DARK_NAVY)


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    title_font = ImageFont.truetype(str(FONT_PATH), TITLE_FONT_SIZE)
    hint_font = ImageFont.truetype(str(FONT_PATH), HINT_FONT_SIZE)
    for file_name, title, color, hint in RESULTS:
        image = Image.new("RGBA", IMAGE_SIZE, (0, 0, 0, 0))
        draw = ImageDraw.Draw(image)
        draw_centered(draw, title, title_font, TITLE_CENTER_Y, color, TITLE_OUTLINE_WIDTH)
        draw_centered(draw, hint, hint_font, HINT_CENTER_Y, WHITE, HINT_OUTLINE_WIDTH)
        image.save(OUTPUT_DIR / file_name)

    option_font = ImageFont.truetype(str(FONT_PATH), OPTION_FONT_SIZE)
    for file_name, text in OPTIONS:
        image = Image.new("RGBA", OPTION_SIZE, (0, 0, 0, 0))
        draw = ImageDraw.Draw(image)
        draw_centered(draw, text, option_font, OPTION_SIZE[1] / 2, WHITE, OPTION_OUTLINE_WIDTH, OPTION_SIZE[0])
        image.save(OUTPUT_DIR / file_name)
    print(f"生成しました: {OUTPUT_DIR}/")


if __name__ == "__main__":
    main()
