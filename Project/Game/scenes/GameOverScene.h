#pragma once

#include "BaseScene.h"
#include "MyMath.h"
#include <array>
#include <memory>
#include <string>

// --- 前方宣言 ---
class Input;
class Obj3dCommon;
class Sprite;
class SpriteCommon;
class HelpOverlay;

/// <summary>
/// ゲームオーバー画面シーン
/// プレイヤーの体力が0になった際に表示する
/// 「コンティニュー」(最後に通ったチェックポイントから再開)と「タイトルへ」を W/S で選び、SPACE で決定する
/// </summary>
class GameOverScene : public BaseScene{
public:
	// --- コンストラクタ・デストラクタ ---
	GameOverScene();
	~GameOverScene() override;

	// --- BaseScene オーバーライド ---
	void Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon) override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	// 選べる項目(上から並べる順番)
	enum Option{
		kOptionContinue, // 最後に通ったチェックポイントから再開する
		kOptionToTitle,  // タイトルから始め直す
		kOptionCount,
	};

	// 選んでいる項目は色を付けて大きく、選んでいない項目は暗く表示する
	void ApplyOptionLook();

	// --- メンバ変数：外部依存 (借りてくるもの) ---
	Obj3dCommon* object3dCommon_ = nullptr;
	Input* input_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;

	// --- メンバ変数：内部リソース (所有するもの) ---
	std::unique_ptr<Sprite> backgroundSprite_;
	// 今どの画面なのか分かるよう、画面の名前(「GAME OVER」)と操作の案内を描いた画像
	std::unique_ptr<Sprite> titleSprite_;
	// 選べる項目の文言を描いた画像(Option の順番に並べる)
	std::array<std::unique_ptr<Sprite>,kOptionCount> optionSprites_;
	// H キーで開く操作説明(タイトルへ戻る方法もここで確認できる)
	std::unique_ptr<HelpOverlay> helpOverlay_;

	// 今選んでいる項目(すぐ再開できるよう、最初はコンティニューを選んでおく)
	int selectedOption_ = kOptionContinue;
	// デモのときに、この画面を出してからの経過時間(秒。決まった時間を過ぎたら最初からやり直す)
	float demoTimer_ = 0.0f;
};
