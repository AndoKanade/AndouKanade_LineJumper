#pragma once

#include "MyMath.h"
#include <memory>
#include <string>

// 前方宣言
class Sprite;
class SpriteCommon;

/// <summary>
/// ゴールの的を壊したときに出す「STAGE CLEAR!」の表示
/// 大きい状態から押しつけるように縮んで収まり、少し揺れて止まった後、しばらく表示してから上へ抜けながら消える。
/// ゴールの的が現れたときの「撃ちまくれ!!!」も、画像と高さを変えて同じ出し方で出す。
/// ImGui を使わずスプライトで描くため、Release 構成でも表示される。
/// </summary>
class StageClearUI{
public:
	StageClearUI();
	~StageClearUI();

	/// <summary>
	/// テクスチャの読み込みとスプライトの生成
	/// 同じ出し方で別の文字を出すときは、画像と高さを指定する(ゴールの的が現れたときの「撃ちまくれ!!!」など)
	/// </summary>
	/// <param name="spriteCommon">スプライト共通設定</param>
	/// <param name="texturePath">出す画像(省略すると「STAGE CLEAR!」)</param>
	/// <param name="heightRate">出す高さ(画面の上からの割合。省略すると kHeightRate)</param>
	void Initialize(SpriteCommon* spriteCommon,const std::string& texturePath = kStageClearTexturePath,float heightRate = kHeightRate);

	// 何も表示していない状態に戻す
	void Reset();

	// 「STAGE CLEAR!」(Initialize() で指定した画像)を出す
	void Begin();

	// 表示の動きを進める
	void Update(float deltaTime);

	// 描画処理(スプライト共通の描画前処理も含む。表示していないときは何もしない)
	void Draw();

private:
	SpriteCommon* spriteCommon_ = nullptr;

	// STAGE CLEAR!
	std::unique_ptr<Sprite> sprite_;

	// 出してからの経過時間(秒)と、表示中かどうか
	float timer_ = 0.0f;
	bool isActive_ = false;

	// 画像の大きさ(ピクセル)。生成した画像(1280 × 320)を縮めて表示する
	static constexpr Vector2 kSize = {1000.0f, 250.0f};
	// 画面上の位置(画面の幅・高さに対する割合)
	static constexpr float kCenterXRate = 0.5f;
	static constexpr float kHeightRate = 0.38f;
	// 出す高さ(Initialize() で指定したもの)
	float heightRate_ = kHeightRate;
	// 「STAGE CLEAR!」の画像(tools/generate_stage_clear_assets.py で生成する)
	inline static const std::string kStageClearTexturePath = "resource/UI/stageClear.png";

	// 押しつけてくる時間(秒)と、押しつけ始めの大きさの倍率
	static constexpr float kStampDuration = 0.2f;
	static constexpr float kStampStartScale = 3.0f;
	// 押しつけた後に揺れる時間(秒)・揺れの角度の幅(ラジアン)・揺れの速さ(1秒あたりの位相の進み、ラジアン)
	static constexpr float kWobbleDuration = 0.5f;
	static constexpr float kWobbleAngle = 0.08f;
	static constexpr float kWobbleSpeed = 30.0f;
	// 揺れ終わってから消え始めるまで表示し続ける時間(秒)
	static constexpr float kHoldTime = 1.6f;
	// 上へ抜けながら消える時間(秒)と、消え終わりまでに上へ動く距離(ピクセル)
	static constexpr float kFadeDuration = 0.4f;
	static constexpr float kFadeRiseDistance = 60.0f;

	// 中心を基準点にするためのアンカー
	static constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
};
