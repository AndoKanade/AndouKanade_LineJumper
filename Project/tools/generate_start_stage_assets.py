# ステージ開始演出(スタート台の出発ゲート・開始時の画面表示)で使うモデルと画像を生成するスクリプト
# 使い方: python tools/generate_start_stage_assets.py  (Projectフォルダで実行する。numpy と Pillow が必要)
# 出力先:
#   resource/StartPlatform/  ゲートの前の目印・出発時に広がる輪(ring)
#                            (台とゲートの柱・ランプ・遮断棒は、既存の立方体 startPlatform.obj を使う)
#   resource/UI/             出発の操作案内(startPrompt)、ステージ情報(stageCard)、GO!(go)、目標の表示(missionText)
# ステージ名・目標の文言・色は、下の定数を書き換えてから実行し直せば変えられる。

from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

# 出力先のフォルダ(Projectフォルダからの相対パス)
MODEL_DIR = Path("resource/StartPlatform")
UI_DIR = Path("resource/UI")
# 文字に使うフォント(リポジトリに入っている丸みのある日本語フォント)
FONT_PATH = Path("resource/fonts/MochiyPopPOne-Regular.ttf")

# 画面表示の文言
STAGE_NUMBER = "01"
STAGE_LABEL = "STAGE"
STAGE_NAME = "雲海ライン"
MISSION_TEXT = "的を撃ちぬいて ゴールをめざせ!"
PROMPT_KEY = "SPACE"
PROMPT_TEXT = "出発する"
GO_TEXT = "GO!"

# 色(0〜255)。自分のインクの黄色(Player の kPlayerColor と同じ色)を基調にする
INK_YELLOW = (255, 217, 38)
DARK_NAVY = (20, 34, 48)
PANEL_COLOR = (16, 40, 52, 205)
WHITE = (255, 255, 255)

# 輪の画像(足元の目印・出発時に広がる輪)
RING_TEXTURE_SIZE = 256
RING_INNER_RATE = 0.72                    # 輪の内側の半径(画像の半分の幅に対する割合)
RING_OUTER_RATE = 0.94                    # 輪の外側の半径
RING_INNER_GLOW_RATE = 0.25               # 輪の内側に入れる薄い光の濃さ


def load_font(size):
    return ImageFont.truetype(str(FONT_PATH), size)


def write_obj(path, name, positions, normals, uvs, faces):
    """OBJ ファイルを書き出す(faces は (位置, UV, 法線) の番号の3つ組を3つ並べた三角形の配列。番号は0始まり)"""
    lines = [f"mtllib {name}.mtl", f"o {name}"]
    lines += [f"v {p[0]:.6f} {p[1]:.6f} {p[2]:.6f}" for p in positions]
    lines += [f"vt {t[0]:.6f} {t[1]:.6f}" for t in uvs]
    lines += [f"vn {n[0]:.6f} {n[1]:.6f} {n[2]:.6f}" for n in normals]
    lines += ["usemtl Material", "s off"]
    for face in faces:
        lines.append("f " + " ".join(f"{v + 1}/{t + 1}/{n + 1}" for v, t, n in face))
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_mtl(path, texture_name):
    path.write_text(
        "newmtl Material\n"
        "Ka 1.000000 1.000000 1.000000\n"
        "Kd 1.000000 1.000000 1.000000\n"
        "d 1.000000\n"
        "illum 1\n"
        f"map_Kd {texture_name}\n",
        encoding="utf-8",
    )





def generate_ring_model():
    """足元の目印・出発時に広がる輪: XZ平面に寝かせた1辺2の板(上向き)に輪の画像を貼る"""
    positions = [(-1.0, 0.0, -1.0), (1.0, 0.0, -1.0), (1.0, 0.0, 1.0), (-1.0, 0.0, 1.0)]
    uvs = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
    normals = [(0.0, 1.0, 0.0)]
    faces = [((0, 0, 0), (2, 2, 0), (1, 1, 0)), ((0, 0, 0), (3, 3, 0), (2, 2, 0))]
    write_obj(MODEL_DIR / "ring.obj", "ring", positions, normals, uvs, faces)
    write_mtl(MODEL_DIR / "ring.mtl", "ring.png")


def generate_ring_texture():
    """白い輪の画像(色はゲーム側でマテリアルの色を掛けて付ける)。内側にだけ薄い光を入れる"""
    size = RING_TEXTURE_SIZE
    coords = (np.arange(size) + 0.5) / size * 2.0 - 1.0
    x, y = np.meshgrid(coords, coords)
    radius = np.sqrt(x * x + y * y)
    # 輪の部分は不透明、縁は少しぼかす
    edge = 2.0 / size * 2.0
    ring = np.clip((radius - RING_INNER_RATE) / edge, 0.0, 1.0) * np.clip((RING_OUTER_RATE - radius) / edge, 0.0, 1.0)
    glow = np.where(radius < RING_INNER_RATE, (radius / RING_INNER_RATE) ** 3 * RING_INNER_GLOW_RATE, 0.0)
    alpha = np.clip(ring + glow, 0.0, 1.0)
    rgba = np.zeros((size, size, 4), dtype=np.uint8)
    rgba[..., :3] = 255
    rgba[..., 3] = (alpha * 255.0).astype(np.uint8)
    Image.fromarray(rgba, "RGBA").save(MODEL_DIR / "ring.png")


def draw_outlined_text(draw, position, text, font, fill, outline, outline_width):
    draw.text(position, text, font=font, fill=fill, stroke_width=outline_width, stroke_fill=outline)


def generate_prompt():
    """出発の操作案内: キーの形の「SPACE」と「出発する」を並べた札"""
    width, height = 512, 128
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle([4, 14, width - 5, height - 15], radius=48, fill=PANEL_COLOR)
    # キーの形
    key_font = load_font(30)
    draw.rounded_rectangle([30, 34, 196, 94], radius=14, fill=WHITE)
    draw.rounded_rectangle([30, 86, 196, 98], radius=6, fill=(180, 186, 190))
    key_box = draw.textbbox((0, 0), PROMPT_KEY, font=key_font)
    draw.text((113 - (key_box[2] - key_box[0]) / 2 - key_box[0], 62 - (key_box[3] - key_box[1]) / 2 - key_box[1]),
              PROMPT_KEY, font=key_font, fill=DARK_NAVY)
    # 案内の文字
    text_font = load_font(44)
    text_box = draw.textbbox((0, 0), PROMPT_TEXT, font=text_font)
    draw.text((226 - text_box[0], 64 - (text_box[3] - text_box[1]) / 2 - text_box[1]), PROMPT_TEXT, font=text_font, fill=WHITE)
    image.save(UI_DIR / "startPrompt.png")


def generate_stage_card():
    """ステージ情報: 番号の丸・ステージ名・目標を並べた横長の札"""
    width, height = 1024, 256
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle([8, 24, width - 9, height - 25], radius=36, fill=PANEL_COLOR)
    # 左端の黄色の帯
    draw.rounded_rectangle([8, 24, 40, height - 25], radius=16, fill=INK_YELLOW)
    draw.rectangle([24, 24, 40, height - 25], fill=INK_YELLOW)
    # 番号の丸
    center = (148, height // 2)
    radius = 78
    draw.ellipse([center[0] - radius, center[1] - radius, center[0] + radius, center[1] + radius], fill=INK_YELLOW)
    label_font = load_font(22)
    label_box = draw.textbbox((0, 0), STAGE_LABEL, font=label_font)
    draw.text((center[0] - (label_box[2] - label_box[0]) / 2 - label_box[0], center[1] - 50 - label_box[1]), STAGE_LABEL, font=label_font, fill=DARK_NAVY)
    number_font = load_font(70)
    number_box = draw.textbbox((0, 0), STAGE_NUMBER, font=number_font)
    draw.text((center[0] - (number_box[2] - number_box[0]) / 2 - number_box[0], center[1] - 22 - number_box[1]), STAGE_NUMBER, font=number_font, fill=DARK_NAVY)
    # ステージ名
    name_font = load_font(64)
    draw_outlined_text(draw, (260, 52), STAGE_NAME, name_font, WHITE, DARK_NAVY, 3)
    # 目標(矢印の後に文言)
    draw.line([(262, 172), (950, 172)], fill=(255, 255, 255, 70), width=2)
    arrow_y = 196
    draw.polygon([(262, arrow_y - 14), (286, arrow_y), (262, arrow_y + 14)], fill=INK_YELLOW)
    mission_font = load_font(34)
    mission_box = draw.textbbox((0, 0), MISSION_TEXT, font=mission_font)
    draw.text((300 - mission_box[0], arrow_y - (mission_box[3] - mission_box[1]) / 2 - mission_box[1]), MISSION_TEXT, font=mission_font, fill=WHITE)
    image.save(UI_DIR / "stageCard.png")


def generate_go():
    """GO!: インクの黄色の太い文字に、濃い縁取りと斜体の傾き、外側の淡い光を付ける"""
    width, height = 640, 320
    font = load_font(220)
    text_layer = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(text_layer)
    box = draw.textbbox((0, 0), GO_TEXT, font=font, stroke_width=14)
    origin = ((width - (box[2] - box[0])) / 2 - box[0], (height - (box[3] - box[1])) / 2 - box[1])
    draw.text(origin, GO_TEXT, font=font, fill=INK_YELLOW, stroke_width=14, stroke_fill=DARK_NAVY)
    # 文字の上半分に白っぽいハイライトを重ねて、つやのあるインクに見せる
    highlight = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    ImageDraw.Draw(highlight).text((origin[0], origin[1]), GO_TEXT, font=font, fill=(255, 245, 190, 255))
    mask = Image.new("L", (width, height), 0)
    ImageDraw.Draw(mask).rectangle([0, 0, width, int(origin[1] + box[1] + (box[3] - box[1]) * 0.42)], fill=150)
    highlight.putalpha(Image.composite(highlight.getchannel("A"), Image.new("L", (width, height), 0), mask))
    text_layer = Image.alpha_composite(text_layer, highlight)
    # 斜体のように右へ傾ける
    shear = 0.18
    text_layer = text_layer.transform((width, height), Image.AFFINE, (1, shear, -shear * height / 2, 0, 1, 0), resample=Image.BICUBIC)
    # 外側の淡い光
    glow = Image.new("RGBA", (width, height), INK_YELLOW + (0,))
    glow.putalpha(text_layer.getchannel("A").filter(ImageFilter.GaussianBlur(18)).point(lambda a: int(a * 0.6)))
    image = Image.alpha_composite(glow, text_layer)
    image.save(UI_DIR / "go.png")


def generate_mission_text():
    """目標の表示: プレイ開始直後に画面上部へ出す白い文字(濃い縁取り付き)"""
    width, height = 1024, 128
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    font = load_font(60)
    box = draw.textbbox((0, 0), MISSION_TEXT, font=font, stroke_width=8)
    origin = ((width - (box[2] - box[0])) / 2 - box[0], (height - (box[3] - box[1])) / 2 - box[1])
    draw.text(origin, MISSION_TEXT, font=font, fill=WHITE, stroke_width=8, stroke_fill=DARK_NAVY)
    image.save(UI_DIR / "missionText.png")


def main():
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    UI_DIR.mkdir(parents=True, exist_ok=True)
    generate_ring_model()
    generate_ring_texture()
    generate_prompt()
    generate_stage_card()
    generate_go()
    generate_mission_text()
    print(f"生成しました: {MODEL_DIR}/, {UI_DIR}/")


if __name__ == "__main__":
    main()
