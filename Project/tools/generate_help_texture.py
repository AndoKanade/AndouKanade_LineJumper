# 操作説明(HelpOverlay)で表示する画像を生成するスクリプト
# ゲームには文字を描く機能が無いため、操作方法と画面の流れを1枚の画像に描いておき、スプライトで表示する。
# 使い方: python tools/generate_help_texture.py  (Projectフォルダで実行する。Pillowが必要)
# 出力先: resource/UI/help.png(説明の本体) / resource/UI/helpHint.png(画面の隅に常に出す「H あそびかた」)
#         resource/UI/quitHint.png(タイトル画面の右上に出す「ESC ゲーム終了」)
# 操作やキーを変えたときは、下の CONTROLS / FLOWS を書き換えて実行し直す。

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

# 出力するファイルのパス(Projectフォルダからの相対パス)
HELP_OUTPUT_PATH = Path("resource/UI/help.png")
HINT_OUTPUT_PATH = Path("resource/UI/helpHint.png")
QUIT_HINT_OUTPUT_PATH = Path("resource/UI/quitHint.png")
# 文字に使うフォント(リポジトリに入っている日本語フォント)
FONT_PATH = "resource/fonts/MochiyPopPOne-Regular.ttf"

# 説明の画像の大きさ(ピクセル)。ゲーム画面(1600x900)の中央に等倍で表示する
HELP_SIZE = (1440, 800)
# 画面の隅に出す案内の画像の大きさ(ピクセル)
HINT_SIZE = (300, 64)
# 「ESC ゲーム終了」の札の大きさ(高さは「H あそびかた」とそろえる)
QUIT_HINT_SIZE = (340, 64)

# 色(RGBA)
PANEL_COLOR = (16, 22, 34, 230)         # 下地(暗い紺の半透明)
PANEL_EDGE_COLOR = (255, 214, 64, 255)  # 下地の縁(インクの黄色)
TITLE_COLOR = (255, 214, 64, 255)       # 見出しの文字
HEADING_COLOR = (120, 220, 255, 255)    # 小見出しの文字(水色)
TEXT_COLOR = (245, 245, 245, 255)       # 本文の文字
SUB_TEXT_COLOR = (180, 186, 200, 255)   # 補足の文字(灰色)
KEY_COLOR = (255, 255, 255, 255)        # キーの枠の中の背景
KEY_TEXT_COLOR = (16, 22, 34, 255)      # キーの枠の中の文字
ARROW_COLOR = (255, 214, 64, 255)       # 画面の流れの矢印
SCENE_BOX_COLOR = (40, 52, 76, 255)     # 画面の流れの各画面の枠

# 下地の角の丸みと縁の太さ(ピクセル)
PANEL_RADIUS = 32
PANEL_EDGE_WIDTH = 6
# 画像の端から文字までの余白(ピクセル)
PADDING = 56

# 操作の一覧(キー, 説明, 補足)
CONTROLS = [
    ("マウス", "照準を動かす", ""),
    ("左クリック", "インクを撃つ", "的と敵を撃ち抜こう"),
    ("SPACE", "ジャンプ / 出発する", "出発はスタート台のゲートの前で押す"),
    ("A/D+SPACE", "横のレールへ乗り換える", "並んでいるレールがあるとき"),
    ("WASD", "スタート台の上・空中で移動する", "別のレールの上に落ちると乗り移る"),
    ("R", "タイトルからやり直す", ""),
    ("TAB", "マウスカーソルの表示切り替え", ""),
    ("H", "この説明を開く / 閉じる", "開いている間はゲームが止まる"),
]

# 画面の流れ(遷移元, 条件, 遷移先)
FLOWS = [
    ("タイトル", "SPACE", "スタート台"),
    ("スタート台", "ゲートの前で SPACE → 開いたらレールに乗る", "プレイ開始"),
    ("プレイ", "レールの終点に着く", "ステージクリア"),
    ("プレイ", "体力(左上)が0になる", "ゲームオーバー"),
    ("プレイ", "レールから落ちる", "スタート台からやり直し"),
    ("クリア", "SPACE", "タイトルへ戻る"),
    ("ゲームオーバー", "W/S で選んで SPACE", "コンティニュー / タイトルへ"),
]


def load_font(size):
    return ImageFont.truetype(FONT_PATH, size)


def draw_key(draw, left, center_y, label, font):
    """キーの名前を白い角丸の枠で囲んで描き、枠の右端のX座標を返す"""
    padding_x = 14
    height = 44
    text_width = draw.textlength(label, font=font)
    right = left + text_width + padding_x * 2
    draw.rounded_rectangle((left, center_y - height / 2, right, center_y + height / 2), radius=10, fill=KEY_COLOR)
    draw.text((left + padding_x, center_y), label, font=font, fill=KEY_TEXT_COLOR, anchor="lm")
    return right


def draw_arrow(draw, left, right, center_y):
    """左から右へ向かう矢印を描く"""
    head = 14
    draw.line((left, center_y, right - head, center_y), fill=ARROW_COLOR, width=6)
    draw.polygon([(right, center_y), (right - head * 1.4, center_y - head), (right - head * 1.4, center_y + head)], fill=ARROW_COLOR)


def generate_help():
    image = Image.new("RGBA", HELP_SIZE, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    width, height = HELP_SIZE

    # 下地
    draw.rounded_rectangle((0, 0, width - 1, height - 1), radius=PANEL_RADIUS, fill=PANEL_COLOR, outline=PANEL_EDGE_COLOR, width=PANEL_EDGE_WIDTH)

    title_font = load_font(56)
    heading_font = load_font(34)
    text_font = load_font(26)
    sub_font = load_font(19)
    key_font = load_font(22)

    # 見出し
    draw.text((width / 2, PADDING + 10), "あそびかた", font=title_font, fill=TITLE_COLOR, anchor="mm")

    top = PADDING + 80
    column_gap = 40
    left_column = PADDING
    right_column = width / 2 + column_gap / 2 - 20

    # 左の列: 操作の一覧
    draw.text((left_column, top), "操作", font=heading_font, fill=HEADING_COLOR, anchor="lt")
    key_area_width = 260
    row_height = 66
    y = top + 70
    for key, text, sub in CONTROLS:
        draw_key(draw, left_column, y, key, key_font)
        if sub:
            draw.text((left_column + key_area_width, y - 4), text, font=text_font, fill=TEXT_COLOR, anchor="ls")
            draw.text((left_column + key_area_width, y + 4), sub, font=sub_font, fill=SUB_TEXT_COLOR, anchor="lt")
        else:
            draw.text((left_column + key_area_width, y), text, font=text_font, fill=TEXT_COLOR, anchor="lm")
        y += row_height

    # 列の区切り線
    separator_x = width / 2 - 20
    draw.line((separator_x, top, separator_x, height - PADDING - 50), fill=SCENE_BOX_COLOR, width=3)

    # 右の列: 画面の流れ
    draw.text((right_column, top), "画面の流れ", font=heading_font, fill=HEADING_COLOR, anchor="lt")
    flow_row_height = 76
    box_height = 46
    y = top + 82
    for source, condition, destination in FLOWS:
        # 遷移元の画面を枠で囲む
        source_width = draw.textlength(source, font=text_font) + 28
        draw.rounded_rectangle((right_column, y - box_height / 2, right_column + source_width, y + box_height / 2), radius=12, fill=SCENE_BOX_COLOR)
        draw.text((right_column + 14, y), source, font=text_font, fill=TEXT_COLOR, anchor="lm")

        # 条件を矢印の上に書く
        arrow_left = right_column + source_width + 16
        arrow_right = arrow_left + 90
        draw_arrow(draw, arrow_left, arrow_right, y)

        # 遷移先
        draw.text((arrow_right + 16, y), destination, font=text_font, fill=TITLE_COLOR, anchor="lm")

        # 条件は遷移元の下に小さく書く
        draw.text((right_column + 14, y + box_height / 2 + 6), "条件: " + condition, font=sub_font, fill=SUB_TEXT_COLOR, anchor="lt")
        y += flow_row_height

    # 一番下の案内
    draw.text((width / 2, height - PADDING + 6), "H キーで閉じる", font=heading_font, fill=TEXT_COLOR, anchor="mm")

    HELP_OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    image.save(HELP_OUTPUT_PATH)


def generate_key_hint(size, key, text, output_path):
    """角丸の札に、キーの枠と説明の文字を並べた画像を書き出す"""
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    width, height = size

    draw.rounded_rectangle((0, 0, width - 1, height - 1), radius=height // 2, fill=PANEL_COLOR, outline=PANEL_EDGE_COLOR, width=3)
    key_right = draw_key(draw, 14, height / 2, key, load_font(26))
    draw.text((key_right + 16, height / 2), text, font=load_font(28), fill=TEXT_COLOR, anchor="lm")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(output_path)


def generate_hint():
    generate_key_hint(HINT_SIZE, "H", "あそびかた", HINT_OUTPUT_PATH)


def generate_quit_hint():
    generate_key_hint(QUIT_HINT_SIZE, "ESC", "ゲーム終了", QUIT_HINT_OUTPUT_PATH)


if __name__ == "__main__":
    generate_help()
    generate_hint()
    generate_quit_hint()
