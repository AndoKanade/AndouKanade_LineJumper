#pragma once

#include "systems/transition/ITransitionEffect.h"
#include <memory>
#include <vector>

/// <summary>
/// 画面を覆う演出: 帯が上から落ちてきて覆い、下に沈んでから上へ持ち上げるように抜けていく
/// </summary>
class FallAndLiftTransition : public ITransitionEffect{
public:
	FallAndLiftTransition();
	~FallAndLiftTransition() override;

	void Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite) override;
	void Finalize() override;
	float GetCoverDuration() const override{ return kColumnCoverDuration; }
	float GetRevealDuration() const override{ return kColumnRevealDuration; }
	void Update(float coverRate,bool isRevealing) override;
	void Draw() override;

private:
	// FallAndLift の帯の本体と、帯の下の縁(落ちてくる先頭)に引く線
	std::vector<std::unique_ptr<Sprite>> columnSprites_;
	std::vector<std::unique_ptr<Sprite>> columnLineSprites_;

	// FallAndLift の調整項目
	// 画面を覆いきる・戻しきるまでの時間(秒)
	static constexpr float kColumnCoverDuration = 0.55f;
	static constexpr float kColumnRevealDuration = 0.5f;
	// 帯の本数(傾けたときに画面の左右の端が空かないよう、画面外に1本ずつ余分に置く)
	static constexpr int kColumnCount = 10;
	// 画面の外に置く帯の本数(左右それぞれ1本)
	static constexpr int kColumnMarginCount = 2;
	// 帯の傾き(ラジアン。落ちる勢いを出すため少しだけ傾ける)
	static constexpr float kColumnAngle = 0.17f;
	// 帯どうしが落ち始めるタイミングのずれ(覆い具合に対する割合)
	static constexpr float kColumnStagger = 0.05f;
	// 落ちる順番をばらけさせるため、帯の番号にこの数を掛けて本数で割った余りを順番にする(本数と互いに素な数)
	static constexpr int kColumnOrderStep = 3;
	// 傾けても画面の上下の端が空かないよう、帯を画面より長くする倍率
	static constexpr float kColumnHeightScale = 1.4f;
	// 隣の帯と隙間ができないよう、帯を少し太くする倍率
	static constexpr float kColumnOverlapScale = 1.1f;
	// 画面の外から落ちてくる距離(画面の高さに対する倍率)
	static constexpr float kColumnTravelScale = 1.6f;
	// 帯の下の縁に引く線の太さ(ピクセル)
	static constexpr float kColumnLineThickness = 10.0f;
};
