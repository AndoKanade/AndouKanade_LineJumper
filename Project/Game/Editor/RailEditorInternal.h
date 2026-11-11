#pragma once
#include "RailEditor.h"
#include <cstddef>

/// <summary>
/// RailEditor の実装を分けた各ファイル(RailEditor.cpp・RailEditorDraw.cpp・RailEditorCurve.cpp・RailEditorData.cpp)で共通して使う定数
/// ほかのクラスからは読み込まない。
/// </summary>
namespace RailEditorInternal{
	// 制御点リセット用のデフォルト値(初期制御点/Add Control Pointボタンと同じ初期値)
	inline constexpr RailEditor::ControlPoint kDefaultControlPoint = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f};
	// レールの始点になる制御点のインデックス(0番目は曲線の向きを決めるだけで、曲線は1番目から始まる)
	// 終点も同じく、最後の制御点から同じ数だけ手前の点になる
	inline constexpr size_t kRailStartPointIndex = 1;
	// Catmull-Romの区間数(制御点数から引く数。始点と終点の制御点は曲線の向きを決めるだけで、曲線は通らないため)
	inline constexpr size_t kCatmullRomExtraPointCount = 3;
	// 1周の角度(ラジアン)
	inline constexpr float kTwoPi = 3.14159265f * 2.0f;
}
