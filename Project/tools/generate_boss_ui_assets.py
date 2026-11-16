# ボス戦で出す文字の画像を生成するスクリプト(仮素材)
# 使い方: python tools/generate_boss_ui_assets.py  (Projectフォルダで実行する。Pillow が必要)
# 出力先: resource/UI/bossPhaseBanner.png / bossRingHint.png
# 「STAGE CLEAR!」(tools/generate_stage_clear_assets.py)と同じ、太い斜体の文字にそろえるため、文字を描く処理はそちらのものを使う。
# 文言・色・文字の大きさは、下の定数を書き換えてから実行し直せば変えられる。

import generate_stage_clear_assets as stamp

# ボスが2形態目に変わったときに出す文言とファイル名(敵のインクに近いピンク)
PHASE_BANNER_TEXT = "まだ終わらない!!"
PHASE_BANNER_FILE = "bossPhaseBanner.png"
PHASE_BANNER_FONT_SIZE = 128

# 輪を塗り返す予告のときに出す、輪から輪への飛び移り方の案内の文言とファイル名(自分のインクの黄色)
# 文が長いため、画像の幅に収まるよう文字を小さくする
RING_HINT_TEXT = "W / S + SPACE で となりの輪へ!"
RING_HINT_FILE = "bossRingHint.png"
RING_HINT_FONT_SIZE = 68


def generate(text, color, file_name, font_size):
    # 文字の大きさは描く処理の側の定数で決まるため、描く前に差し替える
    stamp.FONT_SIZE = font_size
    stamp.generate_stamp_text(text, color, file_name)


def main():
    stamp.OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    generate(PHASE_BANNER_TEXT, stamp.INK_PINK, PHASE_BANNER_FILE, PHASE_BANNER_FONT_SIZE)
    generate(RING_HINT_TEXT, stamp.INK_YELLOW, RING_HINT_FILE, RING_HINT_FONT_SIZE)


if __name__ == "__main__":
    main()
