#pragma once
#include "GlobalVariables.h"
#include <algorithm>

/// <summary>
/// StartSequence の実装を分けた各ファイル(StartSequence.cpp・StartSequenceState.cpp)で共通して使う定数と関数
/// ほかのクラスからは読み込まない。
/// </summary>
namespace StartSequenceInternal{
	// タイトル中に流すBGM
	inline constexpr const char* kTitleBgmPath = "resource/music/bgm/nemui.mp3";
	// SPACE を押してスタートしたとき・出発ゲートを開けたときの決定音
	inline constexpr const char* kDecideSePath = "resource/music/se/decide.mp3";
	// ゲートのカウントダウンの 3・2・1 で鳴らす音と、「GO!」を出すときに鳴らす音
	inline constexpr const char* kCountdownBeepSePath = "resource/music/se/countdown_beep.mp3";
	inline constexpr const char* kCountdownGoSePath = "resource/music/se/countdown_go.mp3";
	// GlobalVariablesのグループ名(タイトル演出の調整項目)
	inline constexpr const char* kTitleGroup = "Title";
	// BGM・SEの音量の調整項目名
	inline constexpr const char* kBgmVolumeKey = "bgmVolume";
	inline constexpr const char* kSeVolumeKey = "seVolume";
	// ゲートを映す画のカメラ座標・注目する座標の調整項目名
	inline constexpr const char* kGateCameraPositionKey = "gateCameraPosition";
	inline constexpr const char* kGateCameraTargetKey = "gateCameraTarget";
	// 音量の範囲(0=無音, 1=元の音量)
	inline constexpr float kMinVolume = 0.0f;
	inline constexpr float kMaxVolume = 1.0f;

	// 調整項目から音量を取得する(ImGuiで範囲外の値を入れられても音が割れないよう範囲内に収める)
	inline float GetVolume(const char* key){
		return std::clamp(GlobalVariables::GetInstance()->GetFloatValue(kTitleGroup,key),kMinVolume,kMaxVolume);
	}
}
