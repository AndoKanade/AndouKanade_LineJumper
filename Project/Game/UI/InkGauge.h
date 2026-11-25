#pragma once

#include "MyMath.h"
#include <memory>

// 前方宣言
class Sprite;
class SpriteCommon;

/// <summary>
/// インクタンクの残りのゲージ
/// 狙っている場所から目を離さずに見られるよう、レティクルの右横に縦長の小さなゲージを出し、残りに合わせて下から伸ばす。
/// インクが足りなくて撃てなかったときは、少しの間ゲージを赤く点滅させる。
/// ImGui を使わずスプライトで描くため、Release 構成でも表示される。
/// </summary>
class InkGauge{
public:
	InkGauge();
	~InkGauge();

	/// <summary>
	/// テクスチャの読み込みとスプライトの生成
	/// </summary>
	/// <param name="spriteCommon">スプライト共通設定</param>
	void Initialize(SpriteCommon* spriteCommon);

	// 点滅を止める(やり直したときに呼ぶ)
	void Reset();

	// インクが足りなくて撃てなかったときに呼ぶ(赤い点滅を始める。点滅している最中に呼ぶと、点滅の時間を延ばす)
	void NotifyShortage();

	/// <summary>
	/// 表示内容の更新
	/// </summary>
	/// <param name="rate">インクの残りの割合(0: 空 〜 1: 満タン)</param>
	/// <param name="deltaTime">経過時間(秒)</param>
	void Update(float rate,float deltaTime);

	// 描画処理(スプライト共通の描画前処理も含む)
	void Draw();

private:
	SpriteCommon* spriteCommon_ = nullptr;

	// 下地(暗い帯)と、インクの残りの帯
	std::unique_ptr<Sprite> frameSprite_;
	std::unique_ptr<Sprite> fillSprite_;

	// 赤く点滅させる残り時間(秒)
	float shortageTimer_ = 0.0f;
	// 点滅し始めてからの経過時間(秒。赤くするか元の色にするかを決める。押しっぱなしで撃てない間も点滅し続けるよう、残り時間とは別に数える)
	float blinkTimer_ = 0.0f;

	// 下地の大きさ(ピクセル)と、レティクルの中心から下地の中心までの横のずれ(ピクセル。レティクルの外枠 48 × 48 に重ならない距離)
	static constexpr Vector2 kFrameSize = {10.0f, 56.0f};
	static constexpr float kOffsetFromReticleX = 40.0f;
	// 下地の縁の太さ(インクの帯は、下地よりこの分だけ内側に描く)
	static constexpr float kFramePadding = 2.0f;
	// 画面中央の位置を求めるための係数(画面サイズの半分)
	static constexpr float kScreenCenterRate = 0.5f;

	// 足りなくて撃てなかったときに赤く点滅させる時間(秒)と、赤くする・元に戻すを切り替える間隔(秒)
	static constexpr float kShortageBlinkDuration = 0.5f;
	static constexpr float kShortageBlinkInterval = 0.08f;

	// 帯に使うテクスチャ(白一色。色はスプライトの色で付ける)
	static inline const char* kWhiteTexturePath = "resource/RailTube/white.png";
	// 下地の基準点(中心)と、下から伸ばす帯の基準点(下端の中央)
	static constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
	static constexpr Vector2 kBottomCenterAnchor = {0.5f, 1.0f};
	// 下地の色・インクの帯の色(プレイヤーのインクの黄色)・足りなくて点滅しているときの色(赤)
	static constexpr Vector4 kFrameColor = {0.05f, 0.07f, 0.12f, 0.7f};
	static constexpr Vector4 kFillColor = {1.0f, 0.9f, 0.2f, 1.0f};
	static constexpr Vector4 kShortageColor = {1.0f, 0.2f, 0.2f, 1.0f};
};
