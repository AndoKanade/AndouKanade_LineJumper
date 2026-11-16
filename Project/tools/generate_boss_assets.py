# ボスのモデル(OBJ)とテクスチャを生成するスクリプト(仮素材)
# 使い方: python tools/generate_boss_assets.py  (Projectフォルダで実行する。Pillow が必要)
# 出力先: resource/Boss/boss.obj, boss.mtl, boss.png
# 形は、球の体のまわりにとげを並べ、正面(+Z)に黒い一つ目を付けたもの。
# ゲーム側(Boss)は、体力の減り具合に合わせて変わる色を画像に掛ける。その色がそのまま見えるよう、体は白、目は黒で塗る。
# 原点は体の中心、体の半径は 1(ゲーム側の表示スケールがそのまま体の半径になる)。

import math
from pathlib import Path

from PIL import Image

OUTPUT_DIR = Path("resource/Boss")
MODEL_NAME = "boss"

# パレット画像(左半分が体の白、右半分が目の黒。各パーツはどちらかのマスの中心のUVを使う)
PALETTE_SIZE = 64
BODY_COLOR = (255, 255, 255, 255)
EYE_COLOR = (24, 26, 40, 255)
BODY_UV = (0.25, 0.5)
EYE_UV = (0.75, 0.5)

# 体の球の半径と分割数
BODY_RADIUS = 1.0
BODY_SEGMENTS = 24
BODY_RINGS = 14

# とげ(円すい)の根元の半径・長さ・側面の分割数
SPIKE_BASE_RADIUS = 0.22
SPIKE_LENGTH = 0.55
SPIKE_SEGMENTS = 8
# とげを体に少し埋めて、根元にすき間が見えないようにする量
SPIKE_SINK = 0.06
# 横一周に並べるとげの数と、上下に傾けて並べる段(真横からの角度。度)
SPIKE_COUNT_PER_ROW = 8
SPIKE_ROW_ELEVATIONS = (-35.0, 0.0, 35.0)
# 正面の目と重ならないよう、とげを置かない正面からの角度の範囲(度)
SPIKE_FRONT_CLEAR_ANGLE = 40.0

# 目(体の表面に貼り付ける丸い板)の半径・分割数・体の中心からの距離
EYE_RADIUS = 0.42
EYE_SEGMENTS = 20
EYE_DISTANCE = 1.02


def normalize(v):
    length = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    return (v[0] / length, v[1] / length, v[2] / length)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def scale(v, s):
    return (v[0] * s, v[1] * s, v[2] * s)


class MeshBuilder:
    """三角形を集めて OBJ として書き出す"""

    def __init__(self):
        self.corners = []  # (座標, 法線, UV) を三角形ごとに3つずつ

    def add_triangle(self, p0, p1, p2, n0, n1, n2, uv, outward):
        # 形の外側(outward)から見て表になる向きに、頂点の並びをそろえる
        face_normal = cross((p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]), (p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]))
        facing = face_normal[0] * outward[0] + face_normal[1] * outward[1] + face_normal[2] * outward[2]
        if facing < 0.0:
            p1, p2 = p2, p1
            n1, n2 = n2, n1
        for p, n in ((p0, n0), (p1, n1), (p2, n2)):
            self.corners.append((p, normalize(n), uv))

    def add_sphere(self, radius, uv):
        for ring in range(BODY_RINGS):
            lat0 = math.pi * ring / BODY_RINGS - math.pi / 2
            lat1 = math.pi * (ring + 1) / BODY_RINGS - math.pi / 2
            for segment in range(BODY_SEGMENTS):
                lon0 = 2 * math.pi * segment / BODY_SEGMENTS
                lon1 = 2 * math.pi * (segment + 1) / BODY_SEGMENTS
                corners = []
                for lat, lon in ((lat0, lon0), (lat0, lon1), (lat1, lon1), (lat1, lon0)):
                    direction = (math.cos(lat) * math.sin(lon), math.sin(lat), math.cos(lat) * math.cos(lon))
                    corners.append(direction)
                center = normalize(add(add(corners[0], corners[1]), add(corners[2], corners[3])))
                points = [scale(c, radius) for c in corners]
                # 上下の端では四角形がつぶれて三角形になるため、面積のある三角形だけを足す
                if ring > 0:
                    self.add_triangle(points[0], points[1], points[2], corners[0], corners[1], corners[2], uv, center)
                if ring < BODY_RINGS - 1:
                    self.add_triangle(points[0], points[2], points[3], corners[0], corners[2], corners[3], uv, center)

    def add_cone(self, base_center, direction, base_radius, length, uv):
        # 向き(direction)に垂直な2つの軸を作り、根元の円を並べる
        helper = (0.0, 1.0, 0.0) if abs(direction[1]) < 0.9 else (1.0, 0.0, 0.0)
        side = normalize(cross(helper, direction))
        up = cross(direction, side)
        tip = add(base_center, scale(direction, length))
        for segment in range(SPIKE_SEGMENTS):
            a0 = 2 * math.pi * segment / SPIKE_SEGMENTS
            a1 = 2 * math.pi * (segment + 1) / SPIKE_SEGMENTS
            r0 = add(scale(side, math.cos(a0)), scale(up, math.sin(a0)))
            r1 = add(scale(side, math.cos(a1)), scale(up, math.sin(a1)))
            p0 = add(base_center, scale(r0, base_radius))
            p1 = add(base_center, scale(r1, base_radius))
            # 側面の法線は、外向きの成分に先へ向かう成分を少し足して、円すいの傾きに合わせる
            slope = base_radius / length
            n0 = add(r0, scale(direction, slope))
            n1 = add(r1, scale(direction, slope))
            self.add_triangle(p0, p1, tip, n0, n1, add(n0, n1), uv, add(r0, r1))

    def add_disc(self, center, normal, radius, uv):
        helper = (0.0, 1.0, 0.0) if abs(normal[1]) < 0.9 else (1.0, 0.0, 0.0)
        side = normalize(cross(helper, normal))
        up = cross(normal, side)
        for segment in range(EYE_SEGMENTS):
            a0 = 2 * math.pi * segment / EYE_SEGMENTS
            a1 = 2 * math.pi * (segment + 1) / EYE_SEGMENTS
            p0 = add(center, scale(add(scale(side, math.cos(a0)), scale(up, math.sin(a0))), radius))
            p1 = add(center, scale(add(scale(side, math.cos(a1)), scale(up, math.sin(a1))), radius))
            self.add_triangle(center, p0, p1, normal, normal, normal, uv, normal)

    def write_obj(self, directory, name):
        directory.mkdir(parents=True, exist_ok=True)

        # 同じ座標・法線・UVはまとめて、ファイルを小さくする
        tables = ({}, {}, {})
        lists = ([], [], [])

        def index_of(kind, value):
            key = tuple(round(float(c), 6) for c in value)
            if key not in tables[kind]:
                lists[kind].append(key)
                tables[kind][key] = len(lists[kind])
            return tables[kind][key]

        face_lines = []
        for i in range(0, len(self.corners), 3):
            parts = []
            for position, normal, uv in self.corners[i:i + 3]:
                parts.append(f"{index_of(0, position)}/{index_of(2, uv)}/{index_of(1, normal)}")
            face_lines.append("f " + " ".join(parts))

        lines = [f"mtllib {name}.mtl", f"o {name}"]
        lines += [f"v {p[0]:.6f} {p[1]:.6f} {p[2]:.6f}" for p in lists[0]]
        lines += [f"vt {t[0]:.6f} {t[1]:.6f}" for t in lists[2]]
        lines += [f"vn {n[0]:.6f} {n[1]:.6f} {n[2]:.6f}" for n in lists[1]]
        lines += ["usemtl Material", "s off"]
        lines += face_lines
        (directory / f"{name}.obj").write_text("\n".join(lines) + "\n", encoding="utf-8")

        mtl_lines = [
            "newmtl Material",
            "Ka 1.000000 1.000000 1.000000",
            "Kd 1.000000 1.000000 1.000000",
            "d 1.000000",
            "illum 1",
            f"map_Kd {name}.png",
        ]
        (directory / f"{name}.mtl").write_text("\n".join(mtl_lines) + "\n", encoding="utf-8")


def generate_texture():
    image = Image.new("RGBA", (PALETTE_SIZE, PALETTE_SIZE), BODY_COLOR)
    half = PALETTE_SIZE // 2
    image.paste(EYE_COLOR, (half, 0, PALETTE_SIZE, PALETTE_SIZE))
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT_DIR / f"{MODEL_NAME}.png")


def generate_model():
    mesh = MeshBuilder()
    mesh.add_sphere(BODY_RADIUS, BODY_UV)

    # とげを、横一周に並べたものを上下に傾けて3段置く(段ごとに半分ずらして、重なって見えないようにする)
    for row, elevation_degree in enumerate(SPIKE_ROW_ELEVATIONS):
        elevation = math.radians(elevation_degree)
        for i in range(SPIKE_COUNT_PER_ROW):
            angle = 2 * math.pi * (i + 0.5 * (row % 2)) / SPIKE_COUNT_PER_ROW
            direction = (math.cos(elevation) * math.sin(angle), math.sin(elevation), math.cos(elevation) * math.cos(angle))
            # 正面(+Z)の目の近くには置かない
            if math.degrees(math.acos(max(-1.0, min(1.0, direction[2])))) < SPIKE_FRONT_CLEAR_ANGLE:
                continue
            mesh.add_cone(scale(direction, BODY_RADIUS - SPIKE_SINK), direction, SPIKE_BASE_RADIUS, SPIKE_LENGTH + SPIKE_SINK, BODY_UV)

    # 真上にも1本立てて、王冠のように見せる
    mesh.add_cone((0.0, BODY_RADIUS - SPIKE_SINK, 0.0), (0.0, 1.0, 0.0), SPIKE_BASE_RADIUS, SPIKE_LENGTH + SPIKE_SINK, BODY_UV)

    # 正面(+Z)に一つ目を貼る
    mesh.add_disc((0.0, 0.0, EYE_DISTANCE), (0.0, 0.0, 1.0), EYE_RADIUS, EYE_UV)

    mesh.write_obj(OUTPUT_DIR, MODEL_NAME)


if __name__ == "__main__":
    generate_texture()
    generate_model()
    print(f"generated: {OUTPUT_DIR}")
