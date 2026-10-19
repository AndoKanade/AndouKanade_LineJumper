# インクで塗れる箱のモデル(UV が面ごとに重ならないよう展開した立方体)を生成するスクリプト
# 使い方: python tools/generate_paint_box_model.py  (Projectフォルダで実行する。標準ライブラリだけで動く)
# 出力先(resource/StartPlatform/):
#   startPlatformPaint.obj / .mtl  スタート台
#   gatePillarPaint.obj / .mtl     出発ゲートの左右の柱
#   gateBeamPaint.obj / .mtl       出発ゲートの上の梁
#
# インクの塗りは、モデルの UV の位置に塗りテクスチャ(PaintCanvas)を描き込んで行う。
# 既存の startPlatform.obj は全頂点の UV が同じ1点のため、6つの面を UV の上で重ならないように並べ直したモデルを作る。
# 形は startPlatform.obj と同じ -1〜1 の立方体で、スケールを掛けて使う。
# 面の大きさの比は、実際に置いたときの大きさ(StartPlatform の定数)に合わせ、どの面も同じ細かさでインクが塗られるようにする。
# 台やゲートの大きさを変えたら、下の MODELS も合わせて実行し直す。

from pathlib import Path

# 出力先のフォルダ(Projectフォルダからの相対パス)
MODEL_DIR = Path("resource/StartPlatform")
# 色は StartPlatform 側でマテリアルの色として付けるため、テクスチャは白一色の既存の画像を使う
TEXTURE_NAME = "white.png"

# 生成するモデルの名前と、実際に置いたときの大きさの半分(X: 左右、Y: 高さ、Z: 前後)
MODELS = [
    # スタート台(StartPlatform の kHalfWidth / kHalfHeight / kHalfLength)
    ("startPlatformPaint", 3.0, 0.5, 5.5),
    # ゲートの柱(StartPlatform の kPillarSize の半分)
    ("gatePillarPaint", 0.11, 0.7, 0.11),
    # ゲートの梁(幅は台の幅、高さと奥行きは StartPlatform の kBeamHeight / kBeamDepth の半分)
    ("gateBeamPaint", 3.0, 0.09, 0.13),
]

# UV の上で面と面の間に空ける隙間(ワールドの長さ)
# テクスチャを補間して読むときに、隣の面に塗ったインクがにじみ出さないようにする
GUTTER = 0.2


def build_box(half_width, half_height, half_length):
    """6つの面を UV の上で重ならないように並べた立方体の頂点・UV・法線・三角形を作る"""
    width = half_width * 2.0
    height = half_height * 2.0
    length = half_length * 2.0

    # UV の上での面の並べ方(ワールドの長さで考え、最後に 0〜1 に収める)
    #   上の段: [上面 幅×長さ][下面 幅×長さ][右面 厚み×長さ][左面 厚み×長さ]
    #   下の段: [前面 幅×厚み][後面 幅×厚み](上面・下面の下に並べる)
    # 各面は (UV の左下の位置, U 方向に進むモデル座標の軸, V 方向に進むモデル座標の軸, 外向きの法線, U・V 方向の長さ)
    column_u = [GUTTER]
    for size in (width, width, height, height):
        column_u.append(column_u[-1] + size + GUTTER)
    row_v_top = GUTTER
    row_v_bottom = row_v_top + length + GUTTER

    faces = [
        # 上面(+Y): U は +X、V は +Z
        ((column_u[0], row_v_top), (-1, 1, -1), (2, 0, 0), (0, 0, 2), (0, 1, 0), (width, length)),
        # 下面(-Y): U は +X、V は +Z
        ((column_u[1], row_v_top), (-1, -1, -1), (2, 0, 0), (0, 0, 2), (0, -1, 0), (width, length)),
        # 右面(+X): U は +Y、V は +Z
        ((column_u[2], row_v_top), (1, -1, -1), (0, 2, 0), (0, 0, 2), (1, 0, 0), (height, length)),
        # 左面(-X): U は +Y、V は +Z
        ((column_u[3], row_v_top), (-1, -1, -1), (0, 2, 0), (0, 0, 2), (-1, 0, 0), (height, length)),
        # 前面(+Z): U は +X、V は +Y
        ((column_u[0], row_v_bottom), (-1, -1, 1), (2, 0, 0), (0, 2, 0), (0, 0, 1), (width, height)),
        # 後面(-Z): U は +X、V は +Y
        ((column_u[1], row_v_bottom), (-1, -1, -1), (2, 0, 0), (0, 2, 0), (0, 0, -1), (width, height)),
    ]

    # 縦横で同じ倍率を掛けて 0〜1 に収める(面が縦か横に伸びて、インクがゆがんで塗られないようにする)
    total_u = column_u[-1]
    total_v = row_v_bottom + height + GUTTER
    uv_scale = 1.0 / max(total_u, total_v)

    positions, uvs, normals, triangles = [], [], [], []
    # 四角形の4つの角(U・V 方向にどれだけ進んだか)
    corners = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
    for (uv_origin, origin, axis_u, axis_v, normal, (size_u, size_v)) in faces:
        base = len(positions)
        normal_index = len(normals)
        normals.append(normal)
        for (s, t) in corners:
            positions.append(tuple(origin[i] + axis_u[i] * s + axis_v[i] * t for i in range(3)))
            uvs.append(((uv_origin[0] + size_u * s) * uv_scale, (uv_origin[1] + size_v * t) * uv_scale))

        # U 軸 × V 軸が外向きなら角の順番のまま反時計回り、逆向きなら順番を逆にして外から見て反時計回りにする
        cross = (
            axis_u[1] * axis_v[2] - axis_u[2] * axis_v[1],
            axis_u[2] * axis_v[0] - axis_u[0] * axis_v[2],
            axis_u[0] * axis_v[1] - axis_u[1] * axis_v[0],
        )
        facing = sum(cross[i] * normal[i] for i in range(3))
        order = [0, 1, 2, 3] if facing > 0 else [0, 3, 2, 1]
        quad = [(base + k, base + k, normal_index) for k in order]
        triangles.append([quad[0], quad[1], quad[2]])
        triangles.append([quad[0], quad[2], quad[3]])

    return positions, uvs, normals, triangles


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


def main():
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    for name, half_width, half_height, half_length in MODELS:
        positions, uvs, normals, triangles = build_box(half_width, half_height, half_length)
        write_obj(MODEL_DIR / f"{name}.obj", name, positions, normals, uvs, triangles)
        write_mtl(MODEL_DIR / f"{name}.mtl", TEXTURE_NAME)
        print(f"wrote {MODEL_DIR / name}.obj")


if __name__ == "__main__":
    main()
