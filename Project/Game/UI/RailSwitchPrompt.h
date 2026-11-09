#pragma once
#include "MyMath.h"
#include <array>
#include <memory>

// 前方宣言
class Sprite;
class SpriteCommon;

/// <summary>
/// 鍵の的を壊して出たレールへ飛び移れるときに、そのレールがある側の画面の端に出す「A + SPACE」「D + SPACE」の案内
/// どちらのキーを押せばよいかを考えずに済むよう、レールがある側に、その向きのキーだけを出す。
/// 出している間は、飛び移る向きへ揺らしながら点滅させて目を引く。飛び移るか、飛び移れなくなったら薄くして消す。
/// ImGui を使わずスプライトで描くため、Release 構成でも表示される。
/// </summary>
class RailSwitchPrompt{
public:
	RailSwitchPrompt();
	~RailSwitchPrompt();

	// テクスチャの読み込みとスプライトの生成
	void Initialize(SpriteCommon* spriteCommon);
	// 何も表示していない状態に戻す
	void Reset();

	/// <summary>
	/// 表示の動きを進める
	/// </summary>
	/// <param name="side">飛び移れる鍵のレールがある側(右: 1、左: -1、無い: 0)</param>
	/// <param name="deltaTime">経過時間(秒)</param>
	void Update(int side,float deltaTime);

	// 描画処理(スプライト共通の描画前処理も含む。表示していないときは何もしない)
	void Draw();

private:
	// 左右の案内の番号(左: A + SPACE、右: D + SPACE)
	static constexpr int kLeftIndex = 0;
	static constexpr int kRightIndex = 1;
	static constexpr int kSideCount = 2;

	SpriteCommon* spriteCommon_ = nullptr;
	// 左右の案内のスプライト
	std::array<std::unique_ptr<Sprite>,kSideCount> sprites_;
	// 出している(最後に出した)側の番号
	int shownIndex_ = kRightIndex;
	// 今の濃さ(0〜1)
	float alpha_ = 0.0f;
	// 点滅・揺れの経過時間(秒)
	float blinkTimer_ = 0.0f;

	// 画像の大きさ(ピクセル)。生成した画像(720 × 280)を縮めて表示する
	static constexpr Vector2 kSize = {432.0f, 168.0f};
	// 画面上の位置(画面の幅・高さに対する割合。左右の案内は真ん中から同じ距離に出す)
	static constexpr float kSideCenterXRate = 0.2f;
	static constexpr float kHeightRate = 0.55f;
	// 濃くなる時間・薄くなって消える時間(秒)
	static constexpr float kFadeInDuration = 0.12f;
	static constexpr float kFadeOutDuration = 0.25f;
	// 点滅の速さ(1秒あたりの位相の進み、ラジアン)と、最も薄くなったときの濃さ
	static constexpr float kBlinkSpeed = 10.0f;
	static constexpr float kBlinkMinAlpha = 0.6f;
	// 飛び移る向きへ揺らす幅(ピクセル)
	static constexpr float kNudgeDistance = 10.0f;
	// sin波(-1〜1)を0〜1に直す倍率
	static constexpr float kWaveToRateScale = 0.5f;
	// 画像の中心を基準点にする
	static constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
};
