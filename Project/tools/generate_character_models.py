# プレイヤー・雑魚敵・レールの制御点の目印のモデル(OBJ)を生成するスクリプト
# 配布モデルの代わりに、円柱・楕円体・箱などの単純な形を組み合わせたローポリモデルを作る。
# 体の色はゲーム側でマテリアルの色として掛けるため、テクスチャは白・灰・黒の明暗だけを持つパレット画像にする。
# 使い方: python tools/generate_character_models.py  (Projectフォルダで実行する。numpyとPillowが必要)
# 出力先: resource/Player/, resource/EnemyBot/, resource/RailMarker/

import math
from pathlib import Path

import numpy as np
from PIL import Image

# 出力先のフォルダ(Projectフォルダからの相対パス)
RESOURCE_DIR = Path("resource")

# パレット画像
# 2×2のマスに明暗を塗り分け、各パーツはどれか1マスの中心のUVを使う
# (ミップマップで縮小されても隣のマスの色が混ざりにくいよう、マスを大きくしている)
PALETTE_FILE_NAME = "palette.png"
PALETTE_SIZE = 256
PALETTE_GRID = 2
# 明暗の種類と、パレットのマス(列, 行)・明るさ
# BODY はマテリアルの色がそのまま出る部分、ACCENT は少し暗い部分、DARK は目やバイザーなどの黒い部分
BODY = "body"
LIGHT = "light"
ACCENT = "accent"
DARK = "dark"
PALETTE_CELLS = {
    BODY: (0, 0, 1.0),
    LIGHT: (1, 0, 0.82),
    ACCENT: (0, 1, 0.55),
    DARK: (1, 1, 0.12),
}

# 円柱・楕円体の分割数(ローポリの見た目にしつつ、丸みが分かる程度にする)
CYLINDER_SEGMENTS = 14
SPHERE_SEGMENTS = 18
SPHERE_RINGS = 10

# モデルの向きと大きさは、差し替え前の人型モデルに合わせる(原点は足元、正面は+Z方向、身長は約1.66)
# こうしておくと、Player/Enemyの表示スケールや弾の発射位置の定数をそのまま使える


class MeshBuilder:
    """三角形を集めてOBJとして書き出す"""

    def __init__(self):
        self.positions = []
        self.normals = []
        self.shades = []
        self.faces = []

    def add_triangle(self, p0, p1, p2, n0, n1, n2, shade):
        base = len(self.positions)
        for p, n in ((p0, n0), (p1, n1), (p2, n2)):
            self.positions.append(np.asarray(p, dtype=float))
            self.normals.append(normalize(np.asarray(n, dtype=float)))
            self.shades.append(shade)
        self.faces.append((base, base + 1, base + 2))

    def add_flat_triangle(self, p0, p1, p2, shade, outward_hint):
        # 面の法線は頂点の並びから求め、形の外側(outward_hint)を向くようにそろえる
        n = np.cross(np.asarray(p1) - p0, np.asarray(p2) - p0)
        if np.dot(n, outward_hint) < 0.0:
            n = -n
        self.add_triangle(p0, p1, p2, n, n, n, shade)

    def bounds(self):
        points = np.array(self.positions)
        return points.min(axis=0), points.max(axis=0)

    def write_obj(self, directory, name):
        directory.mkdir(parents=True, exist_ok=True)
        obj_path = directory / f"{name}.obj"
        mtl_path = directory / f"{name}.mtl"

        # 同じ座標・法線の頂点はまとめて、ファイルを小さくする
        position_index = {}
        normal_index = {}
        position_list = []
        normal_list = []

        def index_of(table, values, key):
            if key not in table:
                table[key] = len(values) + 1
                values.append(key)
            return table[key]

        shade_names = list(PALETTE_CELLS.keys())
        face_lines = []
        for face in self.faces:
            corners = []
            for vertex in face:
                p_key = tuple(round(float(c), 6) for c in self.positions[vertex])
                n_key = tuple(round(float(c), 6) for c in self.normals[vertex])
                vi = index_of(position_index, position_list, p_key)
                ni = index_of(normal_index, normal_list, n_key)
                ti = shade_names.index(self.shades[vertex]) + 1
                corners.append(f"{vi}/{ti}/{ni}")
            face_lines.append("f " + " ".join(corners))

        lines = [f"mtllib {name}.mtl", f"o {name}"]
        lines += [f"v {p[0]:.6f} {p[1]:.6f} {p[2]:.6f}" for p in position_list]
        for shade in shade_names:
            u, v = palette_uv(shade)
            lines.append(f"vt {u:.6f} {v:.6f}")
        lines += [f"vn {n[0]:.6f} {n[1]:.6f} {n[2]:.6f}" for n in normal_list]
        lines += ["usemtl Material", "s off"]
        lines += face_lines
        obj_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

        mtl_lines = [
            "newmtl Material",
            "Ka 1.000000 1.000000 1.000000",
            "Kd 1.000000 1.000000 1.000000",
            "d 1.000000",
            "illum 1",
            f"map_Kd {PALETTE_FILE_NAME}",
        ]
        mtl_path.write_text("\n".join(mtl_lines) + "\n", encoding="utf-8")
        write_palette(directory / PALETTE_FILE_NAME)


def normalize(v):
    length = np.linalg.norm(v)
    return v / length if length > 1e-9 else v


def palette_uv(shade):
    # パレットのマスの中心のUV
    # エンジンはUVのVを反転して読み込むため(aiProcess_FlipUVs)、OBJには画像の上下を逆にした値を書く
    column, row, _ = PALETTE_CELLS[shade]
    u = (column + 0.5) / PALETTE_GRID
    v_image = (row + 0.5) / PALETTE_GRID
    return u, 1.0 - v_image


def write_palette(path):
    cell_size = PALETTE_SIZE // PALETTE_GRID
    image = np.zeros((PALETTE_SIZE, PALETTE_SIZE, 4), dtype=np.uint8)
    for column, row, brightness in PALETTE_CELLS.values():
        value = int(round(brightness * 255))
        image[row * cell_size:(row + 1) * cell_size, column * cell_size:(column + 1) * cell_size] = (value, value, value, 255)
    Image.fromarray(image, "RGBA").save(path)


def add_ellipsoid(mesh, center, radii, shade):
    """楕円体(なめらかな法線)"""
    center = np.asarray(center, dtype=float)
    radii = np.asarray(radii, dtype=float)

    def point(ring, segment):
        theta = math.pi * ring / SPHERE_RINGS
        phi = 2.0 * math.pi * segment / SPHERE_SEGMENTS
        unit = np.array([math.sin(theta) * math.cos(phi), math.cos(theta), math.sin(theta) * math.sin(phi)])
        local = unit * radii
        # 楕円体の法線は、単位球の法線を半径で割った向きになる
        local_normal = unit / radii
        return center + local, local_normal

    for ring in range(SPHERE_RINGS):
        for segment in range(SPHERE_SEGMENTS):
            a = point(ring, segment)
            b = point(ring + 1, segment)
            c = point(ring + 1, segment + 1)
            d = point(ring, segment + 1)
            if ring != 0:
                mesh.add_triangle(a[0], b[0], d[0], a[1], b[1], d[1], shade)
            if ring != SPHERE_RINGS - 1:
                mesh.add_triangle(d[0], b[0], c[0], d[1], b[1], c[1], shade)


def add_frustum(mesh, start, end, start_radius, end_radius, shade, cap_shade=None, squash=1.0, segments=CYLINDER_SEGMENTS):
    """円錐台(start→endの軸に沿う。end_radiusを0にすると円錐になる)

    squash は断面を楕円にするときの、2本目の軸方向の半径の倍率
    """
    start = np.asarray(start, dtype=float)
    end = np.asarray(end, dtype=float)
    axis = normalize(end - start)
    length = np.linalg.norm(end - start)
    cap_shade = shade if cap_shade is None else cap_shade

    # 軸に垂直な2本の基準軸を求める(軸がY方向に近いときはX方向を基準にする)
    helper = np.array([1.0, 0.0, 0.0]) if abs(axis[1]) > 0.9 else np.array([0.0, 1.0, 0.0])
    side = normalize(np.cross(helper, axis))
    up = np.cross(axis, side)

    def ring_point(radius, segment, origin):
        phi = 2.0 * math.pi * segment / segments
        radial = side * math.cos(phi) + up * math.sin(phi) * squash
        return origin + radial * radius, side * math.cos(phi) + up * math.sin(phi) / squash

    # 側面の法線は、半径の変化の分だけ軸方向に傾ける
    slope = (start_radius - end_radius) / length
    for segment in range(segments):
        p0, r0 = ring_point(start_radius, segment, start)
        p1, r1 = ring_point(start_radius, segment + 1, start)
        p2, r2 = ring_point(end_radius, segment + 1, end)
        p3, r3 = ring_point(end_radius, segment, end)
        n0 = normalize(r0) + axis * slope
        n1 = normalize(r1) + axis * slope
        if start_radius > 0.0:
            mesh.add_triangle(p0, p1, p2, n0, n1, n1, shade)
        if end_radius > 0.0:
            mesh.add_triangle(p0, p2, p3, n0, n1, n0, shade)

    # 両端のふた
    for radius, origin, normal in ((start_radius, start, -axis), (end_radius, end, axis)):
        if radius <= 0.0:
            continue
        for segment in range(segments):
            p0, _ = ring_point(radius, segment, origin)
            p1, _ = ring_point(radius, segment + 1, origin)
            mesh.add_triangle(origin, p0, p1, normal, normal, normal, cap_shade)


def add_box(mesh, center, size, shade):
    """直方体(面ごとの平らな法線)"""
    center = np.asarray(center, dtype=float)
    half = np.asarray(size, dtype=float) * 0.5
    for axis in range(3):
        for sign in (-1.0, 1.0):
            normal = np.zeros(3)
            normal[axis] = sign
            u_axis = np.zeros(3)
            v_axis = np.zeros(3)
            u_axis[(axis + 1) % 3] = half[(axis + 1) % 3]
            v_axis[(axis + 2) % 3] = half[(axis + 2) % 3]
            face_center = normal * half[axis]
            corners = [face_center - u_axis - v_axis, face_center + u_axis - v_axis, face_center + u_axis + v_axis, face_center - u_axis + v_axis]
            corners = [center + c for c in corners]
            mesh.add_triangle(corners[0], corners[1], corners[2], normal, normal, normal, shade)
            mesh.add_triangle(corners[0], corners[2], corners[3], normal, normal, normal, shade)


def add_limb(mesh, points, radius, shade, joint_shade=None):
    """関節でつないだ手足(点列の間を円柱でつなぎ、関節に球を置く)"""
    joint_shade = shade if joint_shade is None else joint_shade
    for start, end in zip(points[:-1], points[1:]):
        add_frustum(mesh, start, end, radius, radius, shade)
    for joint in points[1:-1]:
        add_ellipsoid(mesh, joint, (radius, radius, radius), joint_shade)


def build_player():
    """プレイヤー: インクタンクを背負い、両手でブラスターを構えたキャラクター"""
    mesh = MeshBuilder()

    for side in (-1.0, 1.0):
        # 靴
        add_box(mesh, (0.15 * side, 0.07, 0.05), (0.2, 0.14, 0.34), ACCENT)
        # 脚
        add_limb(mesh, [(0.15 * side, 0.12, 0.0), (0.16 * side, 0.36, 0.03), (0.16 * side, 0.6, 0.0)], 0.085, BODY)

    # 腰(ショートパンツ)
    add_frustum(mesh, (0.0, 0.52, 0.0), (0.0, 0.72, 0.0), 0.25, 0.25, ACCENT, squash=0.8)
    # 胴体(上に向かって少し広げる)
    add_frustum(mesh, (0.0, 0.7, 0.0), (0.0, 1.12, 0.0), 0.24, 0.29, BODY, squash=0.78)
    # 胸のライン
    add_frustum(mesh, (0.0, 0.93, 0.0), (0.0, 0.99, 0.0), 0.275, 0.282, LIGHT, squash=0.8)
    # 首
    add_frustum(mesh, (0.0, 1.1, 0.0), (0.0, 1.2, 0.0), 0.11, 0.11, ACCENT)

    # 頭
    head_center = (0.0, 1.36, 0.0)
    add_ellipsoid(mesh, head_center, (0.29, 0.27, 0.27), BODY)
    # バイザー(顔の前面の黒い帯)
    add_ellipsoid(mesh, (0.0, 1.38, 0.15), (0.25, 0.1, 0.15), DARK)
    # 後ろへ垂れる2本の髪(ゲソ風の太い房)
    for side in (-1.0, 1.0):
        add_frustum(mesh, (0.12 * side, 1.34, -0.17), (0.2 * side, 1.04, -0.27), 0.1, 0.045, BODY)
        add_ellipsoid(mesh, (0.2 * side, 1.03, -0.27), (0.05, 0.05, 0.05), BODY)
    # 頭のてっぺんの小さなとさか
    add_frustum(mesh, (0.0, 1.58, 0.02), (0.0, 1.66, -0.06), 0.07, 0.0, BODY)

    # 背中のインクタンク
    add_frustum(mesh, (0.0, 0.74, -0.28), (0.0, 1.02, -0.28), 0.11, 0.11, LIGHT, cap_shade=ACCENT)
    add_frustum(mesh, (0.0, 1.02, -0.28), (0.0, 1.07, -0.28), 0.05, 0.05, DARK)
    # タンクを留めるベルト
    add_frustum(mesh, (0.0, 0.97, 0.0), (0.0, 1.03, 0.0), 0.3, 0.3, DARK, squash=0.8)

    # 腕(肩から前へ伸ばし、体の前でブラスターを持つ)
    gun_center = np.array([0.0, 1.14, 0.42])
    for side in (-1.0, 1.0):
        shoulder = (0.3 * side, 1.07, 0.0)
        elbow = (0.3 * side, 0.94, 0.2)
        hand = (0.1 * side, 1.08, 0.38)
        add_ellipsoid(mesh, shoulder, (0.09, 0.09, 0.09), BODY)
        add_limb(mesh, [shoulder, elbow, hand], 0.065, BODY)
        add_ellipsoid(mesh, hand, (0.07, 0.07, 0.07), ACCENT)

    # ブラスター(本体・タンク・銃口)
    add_box(mesh, gun_center, (0.15, 0.17, 0.4), DARK)
    add_frustum(mesh, gun_center + (0.0, 0.11, -0.06), gun_center + (0.0, 0.11, 0.1), 0.06, 0.06, ACCENT)
    add_frustum(mesh, gun_center + (0.0, 0.0, 0.2), gun_center + (0.0, 0.0, 0.34), 0.055, 0.075, ACCENT, cap_shade=DARK)

    return mesh


def build_enemy():
    """雑魚敵: 4本脚で歩く、一つ目と砲台を持った丸いロボット"""
    mesh = MeshBuilder()
    body_center = np.array([0.0, 0.98, 0.0])

    # 胴体と、胴回りの装甲の帯
    add_ellipsoid(mesh, body_center, (0.55, 0.47, 0.52), BODY)
    add_frustum(mesh, body_center + (0.0, -0.12, 0.0), body_center + (0.0, -0.02, 0.0), 0.57, 0.57, ACCENT, squash=0.95)
    # 底面の黒い部分
    add_ellipsoid(mesh, body_center + (0.0, -0.3, 0.0), (0.38, 0.12, 0.36), DARK)

    # 一つ目(黒い縁・白目・瞳)
    eye_center = body_center + (0.0, 0.08, 0.42)
    add_ellipsoid(mesh, eye_center, (0.25, 0.25, 0.13), DARK)
    add_ellipsoid(mesh, eye_center + (0.0, 0.0, 0.05), (0.18, 0.18, 0.1), LIGHT)
    add_ellipsoid(mesh, eye_center + (0.0, 0.0, 0.11), (0.08, 0.08, 0.05), DARK)

    # 頭の上の砲台(Enemy の弾の発射位置(足元から0.5/表示スケール0.4=1.25)の近くに銃口が来るようにする)
    turret_center = body_center + (0.0, 0.42, 0.05)
    add_frustum(mesh, turret_center + (0.0, -0.1, 0.0), turret_center + (0.0, 0.06, 0.0), 0.22, 0.18, ACCENT, cap_shade=LIGHT)
    add_frustum(mesh, turret_center + (0.0, 0.0, 0.1), turret_center + (0.0, 0.0, 0.6), 0.075, 0.075, ACCENT)
    add_frustum(mesh, turret_center + (0.0, 0.0, 0.52), turret_center + (0.0, 0.0, 0.66), 0.105, 0.105, DARK)

    # 背中のとげ
    for index, (height, back, size) in enumerate(((1.36, -0.22, 0.13), (1.22, -0.42, 0.11), (1.02, -0.52, 0.09))):
        base = np.array([0.0, height, back])
        tip = base + np.array([0.0, size * 1.2, -size * 1.4])
        add_frustum(mesh, base, tip, size, 0.0, BODY if index % 2 == 0 else LIGHT, segments=6)

    # 脚(4本を斜め前後に広げる。付け根→ひざ→足先)
    for side_x in (-1.0, 1.0):
        for side_z in (-1.0, 1.0):
            hip = (0.36 * side_x, 0.78, 0.26 * side_z)
            knee = (0.56 * side_x, 0.6, 0.4 * side_z)
            foot = (0.6 * side_x, 0.08, 0.44 * side_z)
            add_ellipsoid(mesh, hip, (0.1, 0.1, 0.1), ACCENT)
            add_limb(mesh, [hip, knee, foot], 0.065, BODY, joint_shade=ACCENT)
            add_frustum(mesh, (foot[0], 0.0, foot[2]), (foot[0], 0.12, foot[2]), 0.12, 0.08, DARK)

    return mesh


def build_rail_marker():
    """レールの制御点の目印: 上下に尖った宝石の形(半径1。差し替え前の球と同じ大きさ)"""
    mesh = MeshBuilder()
    marker_segments = 8
    ring_radius = 0.75
    ring_height = 0.15
    top = np.array([0.0, 1.0, 0.0])
    bottom = np.array([0.0, -1.0, 0.0])

    def ring_point(segment, height):
        phi = 2.0 * math.pi * segment / marker_segments
        return np.array([ring_radius * math.cos(phi), height, ring_radius * math.sin(phi)])

    for segment in range(marker_segments):
        upper0 = ring_point(segment, ring_height)
        upper1 = ring_point(segment + 1, ring_height)
        lower0 = ring_point(segment, -ring_height)
        lower1 = ring_point(segment + 1, -ring_height)
        outward = normalize((upper0 + upper1) * 0.5)
        # 上の尖り・帯・下の尖り(帯だけ暗くして、向きが分かりやすいようにする)
        mesh.add_flat_triangle(top, upper0, upper1, BODY, outward + top)
        mesh.add_flat_triangle(upper0, lower0, lower1, LIGHT, outward)
        mesh.add_flat_triangle(upper0, lower1, upper1, LIGHT, outward)
        mesh.add_flat_triangle(bottom, lower1, lower0, BODY, outward + bottom)

    return mesh


def report(name, mesh):
    # ゲーム側の当たり判定(Model::CalculateBoundingSphere)と同じ求め方で境界球を表示する
    minimum, maximum = mesh.bounds()
    center = (minimum + maximum) * 0.5
    radius = max(np.linalg.norm(p - center) for p in mesh.positions)
    print(f"{name}: triangles={len(mesh.faces)} min={np.round(minimum, 3)} max={np.round(maximum, 3)} boundingRadius={radius:.3f}")


def main():
    models = [
        ("Player", "player", build_player()),
        ("EnemyBot", "enemyBot", build_enemy()),
        ("RailMarker", "railMarker", build_rail_marker()),
    ]
    for folder, name, mesh in models:
        mesh.write_obj(RESOURCE_DIR / folder, name)
        report(f"{folder}/{name}.obj", mesh)


if __name__ == "__main__":
    main()
