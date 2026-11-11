#pragma once

#include "systems/transition/ITransitionEffect.h"
#include <memory>
#include <vector>

/// <summary>
/// 画面を覆う演出: 斜めのレールの線が走り、そのあとを黄色いインクが塗り進んで覆い、塗った帯が右へ抜けていく(クリアの案)
/// </summary>
class RailPaintTransition : public ITransitionEffect{
public:
	RailPaintTransition();
	~RailPaintTransition() override;

	void Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite) override;
	void Finalize() override;
	float GetCoverDuration() const override{ return kRailPaintCoverDuration; }
	float GetRevealDuration() const override{ return kRailPaintRevealDuration; }
	void Update(float coverRate,bool isRevealing) override;
	void Draw() override;

private:
	// RailPaint の、レールの線・塗り進む帯の本体・帯の先(塗り進む側)と後ろ(抜けていく側)の丸み
	std::vector<std::unique_ptr<Sprite>> railPaintLineSprites_;
	std::vector<std::unique_ptr<Sprite>> railPaintBodySprites_;
	std::vector<std::unique_ptr<Sprite>> railPaintHeadSprites_;
	std::vector<std::unique_ptr<Sprite>> railPaintTailSprites_;

	// RailPaint の調整項目
	// 画面を覆いきる・戻しきるまでの時間(秒)
	static constexpr float kRailPaintCoverDuration = 0.9f;
	static constexpr float kRailPaintRevealDuration = 0.6f;
	// 塗り進む帯の本数と傾き(ラジアン。マイナスで右上がり)
	static constexpr int kRailPaintCount = 8;
	static constexpr float kRailPaintAngle = -0.12f;
	// 帯どうしが塗り始める・抜け始めるタイミングのずれ(覆い具合に対する割合)
	static constexpr float kRailPaintCoverStagger = 0.06f;
	static constexpr float kRailPaintRevealStagger = 0.03f;
	// レールの線が端から端まで走りきる覆い具合
	static constexpr float kRailPaintLineEnd = 0.35f;
	// 帯を塗り進める長さ(帯の長さに対する倍率。先の丸みが画面の外へ出きるよう少し長くする)
	static constexpr float kRailPaintTravelScale = 1.1f;
	// 隣の帯と隙間ができないよう、帯を少し太くする倍率
	static constexpr float kRailPaintOverlapScale = 1.08f;
	// レールの線の太さ(ピクセル)
	static constexpr float kRailPaintLineThickness = 6.0f;
};
