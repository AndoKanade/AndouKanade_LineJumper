#pragma once

#include "systems/transition/ITransitionEffect.h"
#include <memory>
#include <vector>

/// <summary>
/// 画面を覆う演出: レールを模した斜めの帯が左右から走って覆い、始点の方向(左)へ抜けていく
/// </summary>
class RailStripesTransition : public ITransitionEffect{
public:
	RailStripesTransition();
	~RailStripesTransition() override;

	void Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite) override;
	void Finalize() override;
	float GetCoverDuration() const override{ return kStripeCoverDuration; }
	float GetRevealDuration() const override{ return kStripeRevealDuration; }
	void Update(float coverRate,bool isRevealing) override;
	void Draw() override;

private:
	// RailStripes の帯の本体と、帯の縁に引くレールの線
	std::vector<std::unique_ptr<Sprite>> stripeSprites_;
	std::vector<std::unique_ptr<Sprite>> stripeLineSprites_;

	// RailStripes の調整項目
	// 画面を覆いきる・戻しきるまでの時間(秒)
	static constexpr float kStripeCoverDuration = 0.5f;
	static constexpr float kStripeRevealDuration = 0.45f;
	// 帯の本数
	static constexpr int kStripeCount = 7;
	// 帯の傾き(ラジアン。マイナスで右上がり)
	static constexpr float kStripeAngle = -0.35f;
	// 帯どうしが走り出すタイミングのずれ(覆い具合に対する割合)
	static constexpr float kStripeStagger = 0.07f;
	// 隣の帯と隙間ができないよう、帯を少し太くする倍率
	static constexpr float kStripeOverlapScale = 1.05f;
	// レールの線の太さ(ピクセル)
	static constexpr float kStripeLineThickness = 6.0f;
};
