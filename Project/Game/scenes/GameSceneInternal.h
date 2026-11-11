#pragma once
#include "systems/SceneTransition.h"
#include "Title/Departure/DepartureDirector.h"
#include "GlobalVariables.h"
#include "Model.h"
#include "MyMath.h"

/// <summary>
/// GameScene の実装を分けた各ファイル(GameScene.cpp・GameSceneGameplay.cpp・GameSceneFlow.cpp・GameSceneDebugUI.cpp)で共通して使う定数と関数
/// ほかのクラスからは読み込まない。
/// </summary>
namespace GameSceneInternal{
	// GlobalVariablesのグループ名(GameSceneの調整項目)
	inline constexpr const char* kGameSceneGroup = "GameScene";

	// プレイ用カメラの名前
	inline constexpr const char* kDefaultCameraName = "default";

	// レールに乗り込んで出発した瞬間のインクのしぶきを飛ばす向き(真上)
	inline constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};

	// 落下リスタート・クリア画面への切り替えの演出を保存する調整項目名(SceneTransition::Style の番号で保存する)
	// GameScene はシーンが切り替わるたびに作り直されるため、ImGui で選んだ演出を調整項目に持たせて保存・保持する
	inline constexpr const char* kFallTransitionKey = "fallTransitionStyle";
	inline constexpr const char* kClearTransitionKey = "clearTransitionStyle";
	// 出発の演出を保存する調整項目名(DepartureStyle の番号で保存する)と、初期値
	inline constexpr const char* kDepartureStyleKey = "departureStyle";
	inline constexpr DepartureStyle kDefaultDepartureStyle = DepartureStyle::CountdownFlip;
	// レールを進む全体の速さの調整項目名(Playerが登録する。Rail Editorで走り切るまでの秒数を出すのに使う)
	inline constexpr const char* kRailSpeedKey = "railSpeed";
	// 復活したときの、透明な体が元に戻る見せ方を保存する調整項目名(Model::RevealStyle の番号で保存する)と、初期値
	inline constexpr const char* kRespawnRevealKey = "respawnRevealStyle";
	inline constexpr Model::RevealStyle kDefaultRespawnRevealStyle = Model::RevealStyle::Stripes;

	// 調整項目に保存した番号から演出の種類を取得する
	inline SceneTransition::Style GetTransitionStyle(const char* key){
		return static_cast<SceneTransition::Style>(GlobalVariables::GetInstance()->GetIntValue(kGameSceneGroup,key));
	}

	// 調整項目に保存した番号から、復活したときの見せ方を取得する
	inline Model::RevealStyle GetRespawnRevealStyle(){
		return static_cast<Model::RevealStyle>(GlobalVariables::GetInstance()->GetIntValue(kGameSceneGroup,kRespawnRevealKey));
	}

	// 調整項目に保存した番号から出発の演出の種類を取得する
	inline DepartureStyle GetDepartureStyle(){
		return static_cast<DepartureStyle>(GlobalVariables::GetInstance()->GetIntValue(kGameSceneGroup,kDepartureStyleKey));
	}
}
