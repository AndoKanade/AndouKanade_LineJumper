# ゴールの的(ボスの的)のテクスチャを生成するスクリプト(仮素材)
# 使い方: python tools/generate_goal_target_texture.py  (Projectフォルダで実行する。Pillow が必要)
# 出力先: resource/Bullseye/goalTarget.png
# ゲーム側(GoalTarget)は、体力の減り具合に合わせて黄色から赤へ変わる色を画像に掛ける。
# その色がそのまま見えるよう、明るい部分は白、模様は黒で描く(普通の的・鍵の的と見分けられるよう、真ん中に王冠を描く)。
# モデルは普通の的と同じ円盤で、画像の中央の円(直径は画像の幅とほぼ同じ)が的の面に貼られる。画像の上が的の上になる。

from pathlib import Path

from PIL import Image, ImageDraw

OUTPUT_PATH = Path("resource/Bullseye/goalTarget.png")

# 書き出す画像の一辺のピクセル数と、なめらかに描くために大きく描いてから縮める倍率
IMAGE_SIZE = 512
SUPERSAMPLE = 2

# 的の面に貼られる円の半径(画像の一辺に対する割合。モデルの UV と同じ)
DISC_RADIUS_RATE = 0.4975
# 輪の境目の半径(的の半径に対する割合。外側から 黒い縁 → 白 → 黒い輪 → 白 → 黒い輪 → 白い真ん中)
RING_RADII = (
    (1.00, "dark"),
    (0.90, "light"),
    (0.80, "dark"),
    (0.74, "light"),
    (0.66, "dark"),
    (0.61, "light"),
)
# 外側の縁に並べるとげの数・長さ(的の半径に対する割合。黒い縁の内側に白い三角を並べ、勢いを出す)
SPIKE_COUNT = 16
SPIKE_INNER_RATE = 0.89
SPIKE_OUTER_RATE = 0.97
SPIKE_HALF_ANGLE_RATE = 0.35  # とげ1本の幅の半分(とげの間隔に対する割合)

# 王冠の大きさ(的の半径に対する割合。中心からの位置)
CROWN_HALF_WIDTH_RATE = 0.36
CROWN_BOTTOM_RATE = 0.22        # 王冠の帯の下の端(下が正)
CROWN_BAND_HEIGHT_RATE = 0.12   # 王冠の帯の高さ
CROWN_PEAK_HEIGHT_RATE = 0.30   # 帯の上から山の先までの高さ
CROWN_VALLEY_HEIGHT_RATE = 0.10 # 帯の上から山と山の間の谷までの高さ
CROWN_JEWEL_RADIUS_RATE = 0.045 # 山の先に付ける玉の半径

# 色(RGBA)。明るい部分は白(ゲーム側で黄色〜赤に染まる)、模様は濃い紺
LIGHT_COLOR = (255, 255, 255, 255)
DARK_COLOR = (24, 26, 40, 255)


def generate_goal_target():
    import math

    size = IMAGE_SIZE * SUPERSAMPLE
    center = size / 2
    disc_radius = size * DISC_RADIUS_RATE
    image = Image.new("RGBA", (size, size), DARK_COLOR)
    draw = ImageDraw.Draw(image)

    # 外側から順に輪を重ねる
    for rate, tone in RING_RADII:
        radius = disc_radius * rate
        color = DARK_COLOR if tone == "dark" else LIGHT_COLOR
        draw.ellipse((center - radius, center - radius, center + radius, center + radius), fill=color)

    # 黒い縁の内側に、外へ向いた白いとげを並べる
    step = 2 * math.pi / SPIKE_COUNT
    for i in range(SPIKE_COUNT):
        angle = step * i
        half = step * SPIKE_HALF_ANGLE_RATE
        inner = disc_radius * SPIKE_INNER_RATE
        outer = disc_radius * SPIKE_OUTER_RATE
        points = [
            (center + math.cos(angle - half) * inner, center + math.sin(angle - half) * inner),
            (center + math.cos(angle) * outer, center + math.sin(angle) * outer),
            (center + math.cos(angle + half) * inner, center + math.sin(angle + half) * inner),
        ]
        draw.polygon(points, fill=LIGHT_COLOR)

    # 真ん中に王冠(3つの山と帯)を黒で描き、山の先に玉を付ける
    half_width = disc_radius * CROWN_HALF_WIDTH_RATE
    bottom = center + disc_radius * CROWN_BOTTOM_RATE
    band_top = bottom - disc_radius * CROWN_BAND_HEIGHT_RATE
    peak = band_top - disc_radius * CROWN_PEAK_HEIGHT_RATE
    valley = band_top - disc_radius * CROWN_VALLEY_HEIGHT_RATE
    left = center - half_width
    right = center + half_width
    crown = [
        (left, bottom), (left, peak), (center - half_width / 2, valley), (center, peak),
        (center + half_width / 2, valley), (right, peak), (right, bottom),
    ]
    draw.polygon(crown, fill=DARK_COLOR)
    jewel = disc_radius * CROWN_JEWEL_RADIUS_RATE
    for x in (left, center, right):
        draw.ellipse((x - jewel, peak - jewel, x + jewel, peak + jewel), fill=DARK_COLOR)

    image = image.resize((IMAGE_SIZE, IMAGE_SIZE), Image.LANCZOS)
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT_PATH)
    print(f"生成しました: {OUTPUT_PATH}")


if __name__ == "__main__":
    generate_goal_target()
