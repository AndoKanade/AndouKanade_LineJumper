#pragma once

#include "systems/transition/ITransitionEffect.h"

/// <summary>
/// 画面を覆う演出: 黒で徐々に覆う(シーンの切り替えで使う基本の暗転)
/// 画面全体を覆うスプライトだけを使い、黒の濃さを変える。
/// </summary>
class FadeTransition : public ITransitionEffect{
public:
	void Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite) override;
	void Finalize() override;
	float GetCoverDuration() const override{ return kFadeCoverDuration; }
	float GetRevealDuration() const override{ return kFadeRevealDuration; }
	void Update(float coverRate,bool isRevealing) override;
	void Draw() override;

private:
	// 画面全体を覆うスプライト(SceneTransition が持つ。黒にして濃さを変える)
	Sprite* fadeSprite_ = nullptr;

	// Fade の画面を覆いきる・戻しきるまでの時間(秒)
	static constexpr float kFadeCoverDuration = 0.4f;
	static constexpr float kFadeRevealDuration = 0.4f;
};
