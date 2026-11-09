#pragma once

#include <array>
#include <string>

/// <summary>
/// ステージの番号と、ステージごとのデータ(レール・的・敵)の置き場所
/// スタート台の正面の辺がステージ1、右の辺がステージ2、左の辺がステージ3、後ろの辺が最後のステージ(ステージ1〜3をすべてクリアすると出られる)。
/// シーンは切り替えのたびに作り直されるため、今のステージは StageResult と同じく static な値でシーンをまたいで残す。
/// </summary>
struct StageInfo{
	// ステージの数(スタート台の4つの辺に1つずつ)
	static constexpr int kStageCount = 4;
	// 最後のステージ(後ろの辺。ほかのステージをすべてクリアすると出られる)
	static constexpr int kFinalStageIndex = kStageCount - 1;

	// 今編集・プレイしているステージ(0始まり)
	inline static int currentStageIndex = 0;
	// ステージごとに、ゴールの的を壊してクリアしたか(並びはステージの番号と同じ。ゲームオーバーからのコンティニューでは残す)
	inline static std::array<bool,kStageCount> clearedStages = {};

	// 最後のステージに出られるか(ほかのステージをすべてクリアしたか)
	static bool IsFinalStageUnlocked(){
		for(int i = 0; i < kFinalStageIndex; ++i){
			if(!clearedStages[i]){
				return false;
			}
		}
		return true;
	}

	// 指定したステージを選べるか(最後のステージは、ほかのステージをすべてクリアするまで選べない)
	static bool IsStageSelectable(int stageIndex){
		if(!IsValidStageIndex(stageIndex)){
			return false;
		}
		return stageIndex != kFinalStageIndex || IsFinalStageUnlocked();
	}

	// クリアしたステージをすべて忘れる(タイトルから新しく始めるときに呼ぶ)
	static void ResetProgress(){ clearedStages.fill(false); }

	// ステージの番号を、画面に出す番号(1始まり)に直す
	static int ToStageNumber(int stageIndex){ return stageIndex + kStageNumberOffset; }

	// ステージのデータを置くフォルダ(resource/stage/stage1/ など)
	static std::string GetStageDirectory(int stageIndex){
		return kStageRootDirectory + "stage" + std::to_string(ToStageNumber(stageIndex)) + "/";
	}

	// エディターの一覧に出すステージ名(どの辺から出るステージか分かるようにする)
	static const char* GetStageLabel(int stageIndex){
		if(stageIndex < 0 || stageIndex >= kStageCount){
			return kUnknownStageLabel;
		}
		return kStageLabels[stageIndex];
	}

	// ステージの番号が範囲内か
	static bool IsValidStageIndex(int stageIndex){ return stageIndex >= 0 && stageIndex < kStageCount; }

private:
	// 0始まりの番号と、画面に出す1始まりの番号の差
	static constexpr int kStageNumberOffset = 1;
	// ステージごとのフォルダを置く場所(resource配下の相対パス)
	inline static const std::string kStageRootDirectory = "resource/stage/";
	// エディターの一覧に出すステージ名(並びはステージの番号と同じ)
	static constexpr const char* kStageLabels[kStageCount] = {
		"Stage 1 (Front)",
		"Stage 2 (Right)",
		"Stage 3 (Left)",
		"Stage 4 (Final / Back)",
	};
	// 範囲外のステージを指定されたときの名前
	static constexpr const char* kUnknownStageLabel = "Unknown Stage";
};
