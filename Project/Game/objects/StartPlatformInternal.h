#pragma once
#include <iterator>

/// <summary>
/// StartPlatform の実装を分けた各ファイル(StartPlatform.cpp・StartPlatformInk.cpp・StartPlatformCollision.cpp)で共通して使う定数
/// ほかのクラスからは読み込まない。
/// </summary>
namespace StartPlatformInternal{
	// 左右の柱を並べる向き(左が -1、右が +1)
	inline constexpr float kPillarSides[] = {-1.0f, 1.0f};
	inline constexpr int kPillarCount = static_cast<int>(std::size(kPillarSides));
	// 大きさから、中心から面までの距離や立方体(-1〜1)に掛けるスケールを求める倍率(半分)
	inline constexpr float kHalf = 0.5f;
}
