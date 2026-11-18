#pragma once

#include "objects/StageInfo.h"
#include <algorithm>
#include <array>

/// <summary>
/// クリア画面へ渡すプレイの結果
/// シーンは切り替えのたびに作り直されるため、GameScene がクリア直前に書き込み、ClearScene が読み出す。
/// ステージごとの結果はクリアした瞬間に残しておき、クリア画面へ移る直前に全ステージの合計を求める。
/// </summary>
struct StageResult{
	// 壊した的の数(全ステージの合計)
	inline static int destroyedTargetCount = 0;
	// 配置されていた的の総数(全ステージの合計)
	inline static int totalTargetCount = 0;
	// 一番長く続いた連鎖数(全ステージの中で一番長いもの)
	inline static int maxCombo = 0;

	// ステージごとの、クリアしたときの結果(並びはステージの番号と同じ。クリアし直したら上書きする)
	inline static std::array<int,StageInfo::kStageCount> stageDestroyedTargetCounts = {};
	inline static std::array<int,StageInfo::kStageCount> stageTotalTargetCounts = {};
	inline static std::array<int,StageInfo::kStageCount> stageMaxCombos = {};

	// クリアしたステージの結果を残す
	static void RecordStage(int stageIndex,int destroyedCount,int totalCount,int stageMaxCombo){
		if(!StageInfo::IsValidStageIndex(stageIndex)){
			return;
		}
		stageDestroyedTargetCounts[stageIndex] = destroyedCount;
		stageTotalTargetCounts[stageIndex] = totalCount;
		stageMaxCombos[stageIndex] = stageMaxCombo;
	}

	// 残したステージごとの結果から、クリア画面で出す全ステージの合計を求める
	static void CalculateTotal(){
		destroyedTargetCount = 0;
		totalTargetCount = 0;
		maxCombo = 0;
		for(int i = 0; i < StageInfo::kStageCount; ++i){
			destroyedTargetCount += stageDestroyedTargetCounts[i];
			totalTargetCount += stageTotalTargetCounts[i];
			maxCombo = (std::max)(maxCombo,stageMaxCombos[i]);
		}
	}

	// 残したステージごとの結果をすべて忘れる(タイトルから新しく始めるときに、StageInfo::ResetProgress() と一緒に呼ぶ)
	static void ResetStageRecords(){
		stageDestroyedTargetCounts.fill(0);
		stageTotalTargetCounts.fill(0);
		stageMaxCombos.fill(0);
	}
};
