# クリア画面(ClearResultUI)で鳴らす効果音を生成するスクリプト(仮素材)
# 本番の音が見つかるまでの仮の音。同じ名前のファイルに置き換えれば差し替えられる
# (mp3 など別の形式に置き換えるときは、Game/UI/ClearResultUI.cpp のファイル名も合わせて変える)
# 使い方: python tools/generate_clear_se.py  (Projectフォルダで実行する。Python の標準ライブラリだけで動く)
# 出力先: resource/music/se/clear_jingle.wav(「GAME CLEAR!」が出るとき) / clear_splat.wav(インクのしみが広がるとき)
#         clear_count_tick.wav(数字が増えるたび) / clear_count_end.wav(数え終わったとき) / clear_rank_stamp.wav(ランクを押したとき)

import math
import random
import struct
import wave
from pathlib import Path

# 出力先のフォルダ(Projectフォルダからの相対パス)
OUTPUT_DIR = Path("resource/music/se")
# 1秒あたりのサンプル数と、1サンプルの大きさ(16bit)
SAMPLE_RATE = 44100
SAMPLE_WIDTH = 2
SAMPLE_MAX = 32767
# 書き出す音の大きさの上限(1.0 で最大。少し余裕を残して音割れしないようにする)
PEAK = 0.8
# 終わりで音がぷつっと切れないよう、最後にだんだん小さくする時間(秒)
FADE_OUT_SECONDS = 0.005
# しぶき・ハンコの雑音を、実行するたびに同じ音にするための乱数の種
RANDOM_SEED = 7

# 音の高さ(Hz)
C5 = 523.25
E5 = 659.26
G5 = 783.99
B5 = 987.77
C6 = 1046.50
D6 = 1174.66
E6 = 1318.51
G6 = 1567.98

# 矩形波に近い音を作るときに足す倍音(奇数倍音を、その番号分の1の大きさで足す)
SQUARE_HARMONICS = (1, 3, 5, 7)


def silence(seconds):
    """指定した長さの無音を作る"""
    return [0.0] * int(seconds * SAMPLE_RATE)


def envelope(t, attack, decay):
    """鳴らし始めは attack 秒で大きくし、その後は decay 秒ごとに約 1/2.7 になるように小さくする"""
    if t < attack:
        return t / attack
    return math.exp(-(t - attack) / decay)


def tone(frequency, seconds, attack, decay, is_square=False):
    """1つの高さの音を作る(is_square が True なら、ゲームらしい矩形波に近い音にする)"""
    harmonics = SQUARE_HARMONICS if is_square else (1,)
    samples = []
    for i in range(int(seconds * SAMPLE_RATE)):
        t = i / SAMPLE_RATE
        value = 0.0
        for harmonic in harmonics:
            # 聞こえる範囲を超える倍音は足さない(折り返して変な音になるため)
            if frequency * harmonic < SAMPLE_RATE / 2:
                value += math.sin(2.0 * math.pi * frequency * harmonic * t) / harmonic
        samples.append(value * envelope(t, attack, decay))
    return samples


def sweep(start_frequency, end_frequency, seconds, decay):
    """高さを start から end へだんだん下げながら鳴らす、ドンという低い音を作る"""
    samples = []
    phase = 0.0
    count = int(seconds * SAMPLE_RATE)
    for i in range(count):
        t = i / SAMPLE_RATE
        frequency = start_frequency * (end_frequency / start_frequency) ** (i / count)
        phase += 2.0 * math.pi * frequency / SAMPLE_RATE
        samples.append(math.sin(phase) * math.exp(-t / decay))
    return samples


def noise(seconds, decay, smoothing, rng):
    """ザッという雑音を作る(smoothing を 1 に近づけるほど高い音を削り、こもった音にする)"""
    samples = []
    filtered = 0.0
    for i in range(int(seconds * SAMPLE_RATE)):
        t = i / SAMPLE_RATE
        filtered = filtered * smoothing + rng.uniform(-1.0, 1.0) * (1.0 - smoothing)
        samples.append(filtered * math.exp(-t / decay))
    return samples


def mix(destination, source, start_seconds, gain):
    """destination の start_seconds 秒の位置から source を重ねる(はみ出した分は捨てる)"""
    offset = int(start_seconds * SAMPLE_RATE)
    for i, value in enumerate(source):
        if offset + i < len(destination):
            destination[offset + i] += value * gain


def write(file_name, samples, volume):
    """一番大きいところが PEAK × volume になるようにそろえ、16bit のモノラルの wav で書き出す"""
    peak = max(abs(value) for value in samples)
    scale = PEAK * volume / peak
    fade_count = int(FADE_OUT_SECONDS * SAMPLE_RATE)
    frames = bytearray()
    for i, value in enumerate(samples):
        fade = min(1.0, (len(samples) - 1 - i) / fade_count)
        frames += struct.pack("<h", int(max(-1.0, min(1.0, value * scale * fade)) * SAMPLE_MAX))

    path = OUTPUT_DIR / file_name
    with wave.open(str(path), "wb") as file:
        file.setnchannels(1)
        file.setsampwidth(SAMPLE_WIDTH)
        file.setframerate(SAMPLE_RATE)
        file.writeframes(bytes(frames))
    print(f"生成しました: {path}")


def make_jingle():
    """「GAME CLEAR!」: ド・ミ・ソと駆け上がってから、1オクターブ上の和音を伸ばす"""
    samples = silence(1.3)
    for index, frequency in enumerate((C5, E5, G5)):
        mix(samples, tone(frequency, 0.2, 0.003, 0.1, is_square=True), index * 0.08, 0.5)
    for frequency in (C6, E6, G6):
        mix(samples, tone(frequency, 1.04, 0.004, 0.35, is_square=True), 0.26, 0.4)
    # 和音の下に柔らかい音を足して、厚みを出す
    mix(samples, tone(C5, 1.04, 0.004, 0.4), 0.26, 0.6)
    return samples


def make_splat(rng):
    """インクのしみが広がる: ボテッという低い音に、ベチャッという雑音を重ねる"""
    samples = silence(0.32)
    mix(samples, sweep(220.0, 70.0, 0.3, 0.07), 0.0, 1.0)
    mix(samples, noise(0.3, 0.05, 0.6, rng), 0.0, 0.7)
    mix(samples, noise(0.15, 0.03, 0.3, rng), 0.03, 0.4)
    return samples


def make_count_tick():
    """数字が増える: 短く高いチッという音(続けて鳴らすので、短く小さめにする)"""
    samples = silence(0.045)
    mix(samples, tone(1600.0, 0.045, 0.001, 0.012, is_square=True), 0.0, 1.0)
    return samples


def make_count_end(rng):
    """数え終わり: ジャンという明るい和音に、叩いた瞬間の雑音を少し足す"""
    samples = silence(0.6)
    for frequency in (G5, B5, D6, G6):
        mix(samples, tone(frequency, 0.6, 0.002, 0.22, is_square=True), 0.0, 0.35)
    mix(samples, noise(0.05, 0.015, 0.2, rng), 0.0, 0.3)
    return samples


def make_rank_stamp(rng):
    """ランクのハンコ: ドンという重い低い音に、押しつけたときのザッという雑音とカッという音を重ねる"""
    samples = silence(0.65)
    mix(samples, sweep(150.0, 45.0, 0.65, 0.18), 0.0, 1.0)
    mix(samples, noise(0.3, 0.06, 0.75, rng), 0.0, 0.6)
    mix(samples, noise(0.03, 0.008, 0.1, rng), 0.0, 0.35)
    return samples


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    rng = random.Random(RANDOM_SEED)
    write("clear_jingle.wav", make_jingle(), 0.9)
    write("clear_splat.wav", make_splat(rng), 0.8)
    write("clear_count_tick.wav", make_count_tick(), 0.45)
    write("clear_count_end.wav", make_count_end(rng), 0.85)
    write("clear_rank_stamp.wav", make_rank_stamp(rng), 1.0)


if __name__ == "__main__":
    main()
