# シーンの切り替え(クリア・ゲームオーバー)の演出で使う画像を生成するスクリプト
# 使い方: python tools/generate_transition_assets.py  (Projectフォルダで実行する。numpy と Pillow が必要)
# 出力先: resource/UI/transition/
#   inkSplat0〜2.png  クリアの演出で画面に弾けるインクのしみ(白で描き、ゲーム側で色を掛ける)
#   dripTip.png       ゲームオーバーの演出で垂れるインクの先の丸み(白い円)

from pathlib import Path

import numpy as np
from PIL import Image

# 出力先のフォルダ(Projectフォルダからの相対パス)
OUTPUT_DIR = Path("resource/UI/transition")

# インクのしみの画像の大きさ(ピクセル)と種類の数
SPLAT_SIZE = 512
SPLAT_VARIANT_COUNT = 3
# しみの形を決める乱数の種(種類ごとに変える)
SPLAT_SEED_BASE = 20261013
# しみの本体の半径(画像の半分の幅に対する割合)
SPLAT_CORE_RADIUS = 0.42
# 本体の縁から突き出す腕(先が丸い太い線)の数・長さ・太さ
SPLAT_ARM_COUNT = (6, 9)
SPLAT_ARM_LENGTH = (0.55, 0.78)
SPLAT_ARM_RADIUS = (0.05, 0.1)
# 本体から飛び散ったしずくの数・中心までの距離・半径
SPLAT_DROPLET_COUNT = (8, 13)
SPLAT_DROPLET_DISTANCE = (0.7, 0.92)
SPLAT_DROPLET_RADIUS = (0.025, 0.06)
# 形どうしをなめらかにつなぐ幅(大きいほど付け根が太く丸くなる)
SPLAT_BLEND = 0.08
# 縁をなめらかにするぼかしの幅(画像の半分の幅に対する割合)
SPLAT_EDGE_SOFTNESS = 0.008

# 垂れるインクの先の丸みの画像の大きさ(ピクセル)
DRIP_TIP_SIZE = 128


def normalized_grid(size):
    """画像の中心を0、端を±1とした座標の格子を返す"""
    coords = (np.arange(size) + 0.5) / size * 2.0 - 1.0
    return np.meshgrid(coords, coords)


def save_white_alpha(path, alpha):
    """白一色にαだけを持たせた画像として保存する(色はゲーム側でマテリアルの色を掛けて付ける)"""
    rgba = np.zeros(alpha.shape + (4,), dtype=np.uint8)
    rgba[..., :3] = 255
    rgba[..., 3] = (np.clip(alpha, 0.0, 1.0) * 255.0).astype(np.uint8)
    Image.fromarray(rgba, "RGBA").save(path)


def smooth_min(a, b, blend):
    """2つの形の距離をなめらかにつなぐ(blend の幅で付け根を丸くする)"""
    h = np.clip(0.5 + 0.5 * (b - a) / blend, 0.0, 1.0)
    return b * (1.0 - h) + a * h - blend * h * (1.0 - h)


def segment_distance(x, y, end_x, end_y):
    """中心から (end_x, end_y) までの線分への距離"""
    length_sq = end_x * end_x + end_y * end_y
    t = np.clip((x * end_x + y * end_y) / length_sq, 0.0, 1.0)
    return np.sqrt((x - end_x * t) ** 2 + (y - end_y * t) ** 2)


def generate_splat(index):
    """インクのしみ: 本体の円から先の丸い腕を突き出し、周りにしずくを飛ばした形にする"""
    rng = np.random.default_rng(SPLAT_SEED_BASE + index)
    x, y = normalized_grid(SPLAT_SIZE)
    # 形の縁までの距離(内側がマイナス)を求め、本体と腕をなめらかにつなぐ
    distance = np.sqrt(x * x + y * y) - SPLAT_CORE_RADIUS
    for _ in range(rng.integers(*SPLAT_ARM_COUNT)):
        angle = rng.uniform(0.0, 2.0 * np.pi)
        length = rng.uniform(*SPLAT_ARM_LENGTH)
        arm = segment_distance(x, y, np.cos(angle) * length, np.sin(angle) * length) - rng.uniform(*SPLAT_ARM_RADIUS)
        distance = smooth_min(distance, arm, SPLAT_BLEND)
    # しずくは本体から離れて飛んだように、つながずに置く
    for _ in range(rng.integers(*SPLAT_DROPLET_COUNT)):
        angle = rng.uniform(0.0, 2.0 * np.pi)
        center_distance = rng.uniform(*SPLAT_DROPLET_DISTANCE)
        droplet = np.sqrt((x - np.cos(angle) * center_distance) ** 2 + (y - np.sin(angle) * center_distance) ** 2) - rng.uniform(*SPLAT_DROPLET_RADIUS)
        distance = np.minimum(distance, droplet)
    save_white_alpha(OUTPUT_DIR / f"inkSplat{index}.png", 0.5 - distance / SPLAT_EDGE_SOFTNESS)


def generate_drip_tip():
    """垂れるインクの先の丸み: 縁を少しぼかした白い円"""
    x, y = normalized_grid(DRIP_TIP_SIZE)
    radius = np.sqrt(x * x + y * y)
    edge = 2.0 / DRIP_TIP_SIZE * 2.0
    save_white_alpha(OUTPUT_DIR / "dripTip.png", (1.0 - radius) / edge)


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    for index in range(SPLAT_VARIANT_COUNT):
        generate_splat(index)
    generate_drip_tip()
    print(f"生成しました: {OUTPUT_DIR}/")


if __name__ == "__main__":
    main()
