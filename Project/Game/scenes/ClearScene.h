#pragma once

#include "BaseScene.h"
#include "MyMath.h"
#include <memory>
#include <string>

// --- 前方宣言 ---
class Input;
class Obj3dCommon;
class SpriteCommon;
class HelpOverlay;
class ClearResultUI;

/// <summary>
/// クリア画面シーン
/// レールを最後まで進めてゲームをクリアした際に表示する
/// </summary>
class ClearScene : public BaseScene{
public:
	// --- コンストラクタ・デストラクタ ---
	ClearScene();
	~ClearScene() override;

	// --- BaseScene オーバーライド ---
	void Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon) override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	// --- メンバ変数：外部依存 (借りてくるもの) ---
	Obj3dCommon* object3dCommon_ = nullptr;
	Input* input_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;

	// --- メンバ変数：内部リソース (所有するもの) ---
	// 「GAME CLEAR!」・結果のカウントアップ・ランクを順に見せる表示
	std::unique_ptr<ClearResultUI> resultUI_;
	// H キーで開く操作説明(タイトルへ戻る方法もここで確認できる)
	std::unique_ptr<HelpOverlay> helpOverlay_;

	// 1フレームの時間(秒)。フレームレートは TimeManager で60fpsに固定している
	static constexpr float kDeltaTime = 1.0f / 60.0f;
};
