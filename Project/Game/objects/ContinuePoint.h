#pragma once

#include <vector>

/// <summary>
/// ゲームオーバー画面でコンティニューを選んだときに、再開する位置
/// シーンは切り替えのたびに作り直されるため、GameScene がゲームオーバー直前に最後に通ったチェックポイントを書き込み、
/// GameOverScene がコンティニューを選んだら isRequested を立て、次の GameScene が Play に入るときに読み出す。
/// </summary>
struct ContinuePoint{
	// 次の GameScene をタイトルからではなく、ここから始めるか(使ったら GameScene が下ろす)
	inline static bool isRequested = false;
	// 最後に通ったチェックポイントの制御点があるレール(-1: 通る前に体力が尽きたため、レールの始点から始める)
	inline static int railIndex = -1;
	// 最後に通ったチェックポイントの制御点
	inline static int pointIndex = -1;
	// チェックポイントを通った時点での的ごとの生存状態
	inline static std::vector<bool> targetAliveStates;
};
