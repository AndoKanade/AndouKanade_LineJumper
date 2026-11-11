#pragma once

#include "systems/transition/ITransitionEffect.h"
#include <memory>
#include <vector>

/// <summary>
/// 画面を覆う演出: 自分の黄色いインクが画面の中心から外へ次々に弾けて塗りつぶし、下へ垂れ落ちて抜けていく(クリア)
/// </summary>
class InkBurstTransition : public ITransitionEffect{
public:
	InkBurstTransition();
	~InkBurstTransition() override;

	void Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite) override;
	void Finalize() override;
	float GetCoverDuration() const override{ return kInkBurstCoverDuration; }
	float GetRevealDuration() const override{ return kInkBurstRevealDuration; }
	void Update(float coverRate,bool isRevealing) override;
	void Draw() override;

private:
	// 画面全体を覆うスプライト(SceneTransition が持つ。インクの色にして、しみの隙間を塗るのに使う)
	Sprite* fadeSprite_ = nullptr;
	// InkBurst の、画面に弾けるインクのしみ(全面を塗る色は fadeSprite_ に色を掛けて使う)
	std::vector<std::unique_ptr<Sprite>> inkSplatSprites_;

	// InkBurst の調整項目(しみの位置・大きさ・傾きの並びは InkBurstTransition.cpp の kInkSplatLayouts)
	// 画面を覆いきる・戻しきるまでの時間(秒)
	static constexpr float kInkBurstCoverDuration = 0.9f;
	static constexpr float kInkBurstRevealDuration = 0.7f;
	// しみどうしが弾け始める・落ち始めるタイミングのずれ(覆い具合に対する割合)
	static constexpr float kInkSplatCoverStagger = 0.05f;
	static constexpr float kInkSplatRevealStagger = 0.04f;
	// しみが落ちていく距離(画面の高さに対する倍率)
	static constexpr float kInkSplatFallScale = 1.8f;
	// しみの隙間を埋めるため、画面全体をインクの色で塗り始める覆い具合
	static constexpr float kInkFillCoverStart = 0.8f;
	// 画面を戻すときに、全体の塗りが消えきる進み具合
	static constexpr float kInkFillRevealEnd = 0.3f;
};
