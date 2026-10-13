#pragma once

#include "MyMath.h"
#include <memory>

// 前方宣言
class Sprite;
class SpriteCommon;

/// <summary>
/// 出発までの画面表示
/// 出発台のゲートの前に立ったときの操作案内と、
/// レールに乗り込んだ瞬間の「GO!」を表示する。
/// ImGui を使わずスプライトで描くため、Release 構成でも表示される。
/// </summary>
class StageIntroUI{
public:
	StageIntroUI();
	~StageIntroUI();

	// テクスチャの読み込みとスプライトの生成
	void Initialize(SpriteCommon* spriteCommon);

	// 何も表示していない状態に戻す
	void Reset();

	// 操作案内を出すかどうか(切り替えるとふわっと出たり消えたりする)
	void SetPromptVisible(bool isVisible){ isPromptVisible_ = isVisible; }

	// 「GO!」を出す
	void BeginGo();

	// 表示の動きを進める
	void Update(float deltaTime);

	// 描画処理(スプライト共通の描画前処理も含む)
	void Draw();

private:
	SpriteCommon* spriteCommon_ = nullptr;

	// 操作案内(SPACE 出発する)
	std::unique_ptr<Sprite> promptSprite_;
	// GO!
	std::unique_ptr<Sprite> goSprite_;

	// 操作案内を出すかどうかと、今の濃さ・揺れの経過時間
	bool isPromptVisible_ = false;
	float promptAlpha_ = 0.0f;
	float promptTime_ = 0.0f;
	// GO! を出してからの経過時間(秒)と、表示中かどうか
	float goTimer_ = 0.0f;
	bool isGoActive_ = false;

	// 画像の大きさ(ピクセル)。生成した画像を縮めて表示する
	static constexpr Vector2 kPromptSize = {384.0f, 96.0f};
	static constexpr Vector2 kGoSize = {480.0f, 240.0f};
	// 画面上の位置(画面の幅・高さに対する割合)
	static constexpr float kPromptHeightRate = 0.78f;
	static constexpr float kGoHeightRate = 0.42f;

	// 操作案内が出たり消えたりする速さ(1秒あたりに残りの差を縮める割合の指数)
	static constexpr float kPromptFadeSharpness = 12.0f;
	// 操作案内がこの濃さより薄くなったら描かない(なめらかに薄くするため、ちょうど0にはならない)
	static constexpr float kPromptMinVisibleAlpha = 0.01f;
	// 操作案内の上下の揺れ幅(ピクセル)と速さ(1秒あたりの位相の進み、ラジアン)
	static constexpr float kPromptBobHeight = 4.0f;
	static constexpr float kPromptBobSpeed = 4.0f;

	// GO! が飛び出してくる時間(秒)と、飛び出し始めの大きさの倍率
	static constexpr float kGoPopDuration = 0.25f;
	static constexpr float kGoPopStartScale = 1.8f;
	// GO! を表示し続ける時間(秒。飛び出してから消え始めるまで)
	static constexpr float kGoHoldTime = 0.65f;
	// GO! が広がりながら消える時間(秒)と、消え終わりの大きさの倍率
	static constexpr float kGoFadeDuration = 0.3f;
	static constexpr float kGoFadeEndScale = 1.3f;

	// 中心を基準点にするためのアンカー
	static constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
};
