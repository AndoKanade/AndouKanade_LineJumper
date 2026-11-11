#pragma once

#include "systems/transition/ITransitionEffect.h"
#include "MyMath.h"
#include <memory>
#include <vector>

/// <summary>
/// 画面を覆う演出: 下から雲が湧き上がって画面を包み、左右に割れて抜けていく(クリアの案)
/// </summary>
class CloudRiseTransition : public ITransitionEffect{
public:
	CloudRiseTransition();
	~CloudRiseTransition() override;

	void Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite) override;
	void Finalize() override;
	float GetCoverDuration() const override{ return kCloudCoverDuration; }
	float GetRevealDuration() const override{ return kCloudRevealDuration; }
	void Update(float coverRate,bool isRevealing) override;
	void Draw() override;

private:
	// 画面全体を覆うスプライト(SceneTransition が持つ。白にして、雲の隙間を塗るのに使う)
	Sprite* fadeSprite_ = nullptr;
	// CloudRise の雲のかたまり(全面を塗る白は fadeSprite_ に色を掛けて使う)
	std::vector<std::unique_ptr<Sprite>> cloudSprites_;

	// CloudRise の調整項目
	// 画面を覆いきる・戻しきるまでの時間(秒)
	static constexpr float kCloudCoverDuration = 1.0f;
	static constexpr float kCloudRevealDuration = 0.7f;
	// 雲を並べる段の数と、1段あたりの数
	static constexpr int kCloudRowCount = 7;
	static constexpr int kCloudColumnCount = 9;
	// 一番下の段と一番上の段の中心の高さ(画面の高さに対する割合。画面の上下の外まで並べる)
	static constexpr float kCloudBottomRowHeight = 1.15f;
	static constexpr float kCloudTopRowHeight = -0.1f;
	// 段どうしが湧き上がり始めるタイミングのずれ(覆い具合に対する割合)
	static constexpr float kCloudStagger = 0.08f;
	// 湧き上がり始めの大きさの割合(ここから元の大きさまでふくらむ)
	static constexpr float kCloudStartSizeRate = 0.6f;
	// 隙間を埋めるため、画面全体を白で塗り始める覆い具合
	static constexpr float kCloudFillStart = 0.85f;
	// 画面を戻すときに左右へ割れて動く距離(画面の幅に対する割合)
	static constexpr float kCloudSplitDistance = 0.8f;
	// 雲の色(白と、影になったところの薄い水色。数個に1つを影の色にする)
	static constexpr Vector4 kCloudColor = {1.0f, 1.0f, 1.0f, 1.0f};
	static constexpr Vector4 kCloudShadeColor = {0.82f, 0.9f, 0.95f, 1.0f};
	static constexpr int kCloudShadeInterval = 3;
};
