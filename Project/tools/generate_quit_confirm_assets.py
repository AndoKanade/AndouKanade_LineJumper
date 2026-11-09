# ESC キーで出す終了確認メニュー(「ゲームをやめる?」→ はい / いいえ)の画像を生成するスクリプト(仮素材)
# 使い方: python tools/generate_quit_confirm_assets.py  (Projectフォルダで実行する。numpy と Pillow が必要)
# 出力先: resource/UI/quitConfirm/
#   panel.png      インクがたれた形の板。見出しと操作の案内を描く(色はそのまま使う)
#   yesButton.png  「はい」のボタン(白いインクのかたまりに紺の文字。ゲーム側で色を掛ける)
#   noButton.png   「いいえ」のボタン(同上)
# 文言・色・形は、下の定数を書き換えてから実行し直せば変えられる。

from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

# 出力先のフォルダ(Projectフォルダからの相対パス)
OUTPUT_DIR = Path("resource/UI/quitConfirm")
# 文字に使うフォント(リポジトリに入っている丸みのある日本語フォント)
FONT_PATH = Path("resource/fonts/MochiyPopPOne-Regular.ttf")

# 色(0〜255)。自分のインクの黄色・縁取りの濃い紺色・案内の白
INK_YELLOW = (255, 217, 38)
DARK_NAVY = (20, 34, 48)
WHITE = (255, 255, 255)

# --- 板 ---
PANEL_SIZE = (1100, 680)
# 板の形を決める乱数の種
PANEL_SEED = 20261109
# 板の本体(角の丸い四角)の半分の幅・高さと角の丸み(画像の半分の幅・高さに対する割合)
PANEL_HALF_EXTENT = (0.82, 0.62)
PANEL_CORNER_RADIUS = 0.18
# 縁のふくらみ(インクのかたまりらしいでこぼこ)の数と半径
PANEL_BUMP_COUNT = 18
PANEL_BUMP_RADIUS = (0.06, 0.11)
# 下の縁からたれるインクの数・長さ・太さ
PANEL_DRIP_COUNT = 6
PANEL_DRIP_LENGTH = (0.12, 0.3)
PANEL_DRIP_RADIUS = (0.035, 0.06)
# 形どうしをなめらかにつなぐ幅
PANEL_BLEND = 0.06
# 外側の黄色いインクの縁の太さ(紺の本体をこの分だけ内側へ縮める)
PANEL_RIM_WIDTH = 0.035
# 紺の本体の濃さ(少し透かして、後ろのゲーム画面が見えるようにする)
PANEL_BODY_ALPHA = 0.92
# 縁をなめらかにするぼかしの幅
EDGE_SOFTNESS = 0.006

# 見出し・操作の案内の文言・文字の大きさ・縁取りの太さ・中心の高さ(画像の高さに対する割合)
TITLE_TEXT = "ゲームをやめる？"
TITLE_FONT_SIZE = 100
TITLE_OUTLINE_WIDTH = 10
TITLE_CENTER_RATE = 0.27
# 見出しを右へ傾ける量(斜体)
TITLE_SHEAR = 0.12
HINT_TEXT = "A / D : えらぶ　　SPACE : けってい　　ESC : もどる"
HINT_FONT_SIZE = 32
HINT_OUTLINE_WIDTH = 0
HINT_CENTER_RATE = 0.82

# --- ボタン ---
BUTTON_SIZE = (420, 200)
# ボタンの形を決める乱数の種(ボタンごとに変える)
BUTTON_SEED_BASE = 20261110
# ボタンの本体の半分の幅・高さと角の丸み(画像の半分の幅・高さに対する割合)
BUTTON_HALF_EXTENT = (0.78, 0.6)
BUTTON_CORNER_RADIUS = 0.4
BUTTON_BUMP_COUNT = 10
BUTTON_BUMP_RADIUS = (0.1, 0.16)
BUTTON_BLEND = 0.08
# ボタンの縁の紺色の線の太さ
BUTTON_OUTLINE_WIDTH = 0.05
BUTTON_FONT_SIZE = 84
# ボタンの文言とファイル名
BUTTONS = [("はい", "yesButton.png"), ("いいえ", "noButton.png")]


def normalized_grid(size):
    """画像の中心を0、端を±1とした座標の格子を返す(横・縦それぞれの大きさで割る)"""
    width, height = size
    xs = (np.arange(width) + 0.5) / width * 2.0 - 1.0
    ys = (np.arange(height) + 0.5) / height * 2.0 - 1.0
    return np.meshgrid(xs, ys)


def smooth_min(a, b, blend):
    """2つの形の距離をなめらかにつなぐ(blend の幅で付け根を丸くする)"""
    h = np.clip(0.5 + 0.5 * (b - a) / blend, 0.0, 1.0)
    return b * (1.0 - h) + a * h - blend * h * (1.0 - h)


def rounded_rect_distance(x, y, half_extent, corner_radius):
    """角の丸い四角の縁までの距離(内側がマイナス)"""
    qx = np.abs(x) - (half_extent[0] - corner_radius)
    qy = np.abs(y) - (half_extent[1] - corner_radius)
    outside = np.sqrt(np.maximum(qx, 0.0) ** 2 + np.maximum(qy, 0.0) ** 2)
    inside = np.minimum(np.maximum(qx, qy), 0.0)
    return outside + inside - corner_radius


def bumpy_blob_distance(x, y, rng, half_extent, corner_radius, bump_count, bump_radius, blend):
    """角の丸い四角の縁に円のふくらみを並べ、インクのかたまりのようなでこぼこの形にする"""
    distance = rounded_rect_distance(x, y, half_extent, corner_radius)
    for index in range(bump_count):
        # 縁に沿って一周するように角度を割り振り、少しだけばらつかせる
        angle = (index + rng.uniform(-0.3, 0.3)) / bump_count * 2.0 * np.pi
        center_x = np.cos(angle) * (half_extent[0] - corner_radius * 0.5)
        center_y = np.sin(angle) * (half_extent[1] - corner_radius * 0.5)
        bump = np.sqrt((x - center_x) ** 2 + (y - center_y) ** 2) - rng.uniform(*bump_radius)
        distance = smooth_min(distance, bump, blend)
    return distance


def to_alpha(distance):
    """縁までの距離から、縁をなめらかにした濃さ(0〜1)を求める"""
    return np.clip(0.5 - distance / EDGE_SOFTNESS, 0.0, 1.0)


def fill_layer(size, alpha, color, alpha_scale=1.0):
    """1色で塗り、濃さだけを形に合わせた画像を作る"""
    rgba = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    rgba[..., :3] = color
    rgba[..., 3] = (alpha * alpha_scale * 255.0).astype(np.uint8)
    return Image.fromarray(rgba, "RGBA")


def draw_centered_text(layer, text, font, center_y, fill, outline_width):
    """文字を左右の中央、高さ center_y に縁取り付きで描く"""
    draw = ImageDraw.Draw(layer)
    box = draw.textbbox((0, 0), text, font=font, stroke_width=outline_width)
    origin = ((layer.width - (box[2] - box[0])) / 2 - box[0], center_y - (box[3] - box[1]) / 2 - box[1])
    draw.text(origin, text, font=font, fill=fill, stroke_width=outline_width, stroke_fill=DARK_NAVY)


def generate_panel():
    """板: 黄色いインクの縁取りをした紺のかたまりの下からインクをたらし、見出しと案内を描く"""
    rng = np.random.default_rng(PANEL_SEED)
    width, height = PANEL_SIZE
    x, y = normalized_grid(PANEL_SIZE)
    distance = bumpy_blob_distance(x, y, rng, PANEL_HALF_EXTENT, PANEL_CORNER_RADIUS, PANEL_BUMP_COUNT, PANEL_BUMP_RADIUS, PANEL_BLEND)
    # 下の縁からたれるインク(先の丸い縦の線)を、左右に散らしてなめらかにつなぐ
    for index in range(PANEL_DRIP_COUNT):
        drip_x = ((index + 0.5) / PANEL_DRIP_COUNT * 2.0 - 1.0) * PANEL_HALF_EXTENT[0] * 0.85 + rng.uniform(-0.05, 0.05)
        drip_top = PANEL_HALF_EXTENT[1] * 0.8
        drip_bottom = PANEL_HALF_EXTENT[1] + rng.uniform(*PANEL_DRIP_LENGTH)
        drip_y = np.clip(y, drip_top, drip_bottom)
        drip = np.sqrt((x - drip_x) ** 2 + (y - drip_y) ** 2) - rng.uniform(*PANEL_DRIP_RADIUS)
        distance = smooth_min(distance, drip, PANEL_BLEND)

    # 外側を黄色いインク、内側を少し縮めた紺の本体にして、インクで縁取ったように見せる
    rim = fill_layer(PANEL_SIZE, to_alpha(distance), INK_YELLOW)
    body = fill_layer(PANEL_SIZE, to_alpha(distance + PANEL_RIM_WIDTH), DARK_NAVY, PANEL_BODY_ALPHA)
    image = Image.alpha_composite(rim, body)

    # 見出しは斜体のように右へ傾け、勢いを出す
    title_layer = Image.new("RGBA", PANEL_SIZE, (0, 0, 0, 0))
    draw_centered_text(title_layer, TITLE_TEXT, ImageFont.truetype(str(FONT_PATH), TITLE_FONT_SIZE), height * TITLE_CENTER_RATE, INK_YELLOW, TITLE_OUTLINE_WIDTH)
    title_layer = title_layer.transform(PANEL_SIZE, Image.AFFINE, (1, TITLE_SHEAR, -TITLE_SHEAR * height * TITLE_CENTER_RATE, 0, 1, 0), resample=Image.BICUBIC)
    image = Image.alpha_composite(image, title_layer)

    # 操作の案内はまっすぐ読みやすく描く
    hint_layer = Image.new("RGBA", PANEL_SIZE, (0, 0, 0, 0))
    draw_centered_text(hint_layer, HINT_TEXT, ImageFont.truetype(str(FONT_PATH), HINT_FONT_SIZE), height * HINT_CENTER_RATE, WHITE, HINT_OUTLINE_WIDTH)
    image = Image.alpha_composite(image, hint_layer)
    image.save(OUTPUT_DIR / "panel.png")


def generate_button(index, text, filename):
    """ボタン: 紺の線で縁取った白いインクのかたまりに、紺の文字を描く(白い所だけがゲーム側で掛ける色になる)"""
    rng = np.random.default_rng(BUTTON_SEED_BASE + index)
    width, height = BUTTON_SIZE
    x, y = normalized_grid(BUTTON_SIZE)
    distance = bumpy_blob_distance(x, y, rng, BUTTON_HALF_EXTENT, BUTTON_CORNER_RADIUS, BUTTON_BUMP_COUNT, BUTTON_BUMP_RADIUS, BUTTON_BLEND)
    outline = fill_layer(BUTTON_SIZE, to_alpha(distance), DARK_NAVY)
    body = fill_layer(BUTTON_SIZE, to_alpha(distance + BUTTON_OUTLINE_WIDTH), WHITE)
    image = Image.alpha_composite(outline, body)
    draw_centered_text(image, text, ImageFont.truetype(str(FONT_PATH), BUTTON_FONT_SIZE), height * 0.5, DARK_NAVY, 0)
    image.save(OUTPUT_DIR / filename)


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    generate_panel()
    for index, (text, filename) in enumerate(BUTTONS):
        generate_button(index, text, filename)
    print(f"生成しました: {OUTPUT_DIR}/")


if __name__ == "__main__":
    main()
