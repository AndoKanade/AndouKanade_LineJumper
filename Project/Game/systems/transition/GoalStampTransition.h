#pragma once

#include "systems/transition/ITransitionEffect.h"
#include "MyMath.h"
#include <memory>
#include <vector>

/// <summary>
/// 画面を覆う演出: 「GOAL!」が上から押されて揺れ、しみが飛んだ後に中央から黄色いインクが広がって覆う(クリアの案)
/// </summary>
class GoalStampTransition : public ITransitionEffect{
public:
	GoalStampTransition();
	~GoalStampTransition() override;

	void Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite) override;
	void Finalize() override;
	float GetCoverDuration() const override{ return kStampCoverDuration; }
	float GetRevealDuration() const override{ return kStampRevealDuration; }
	void Update(float coverRate,bool isRevealing) override;
	void Draw() override;

private:
	// 画面全体を覆うスプライト(SceneTransition が持つ。インクの色にして、最後に全面を塗るのに使う)
	Sprite* fadeSprite_ = nullptr;
	// GoalStamp の、画面を暗くする全面・押した衝撃で飛ぶしみ・中央から広がるしみ・「GOAL!」の文字
	// (最後に全面を塗る色は fadeSprite_ に色を掛けて使う)
	std::unique_ptr<Sprite> stampDimSprite_;
	std::vector<std::unique_ptr<Sprite>> stampSplashSprites_;
	std::unique_ptr<Sprite> stampSpreadSprite_;
	std::unique_ptr<Sprite> stampTextSprite_;
	// 「GOAL!」の文字の、押しきったときの大きさ(画像の大きさに kStampTextScale を掛けたもの)
	Vector2 stampTextBaseSize_ = {0.0f, 0.0f};

	// GoalStamp の調整項目(押した衝撃で飛ぶしみの位置・大きさは GoalStampTransition.cpp の kStampSplashLayouts)
	// 画面を覆いきる・戻しきるまでの時間(秒。文字を読ませるため覆う側を長めにする)
	static constexpr float kStampCoverDuration = 1.4f;
	static constexpr float kStampRevealDuration = 0.6f;
	// 画面を暗くする濃さと、暗くしきる覆い具合
	static constexpr float kStampDimAlpha = 0.35f;
	static constexpr float kStampDimEnd = 0.3f;
	// 文字が押されきる(大きい状態から縮んで止まる)覆い具合と、押し始めの大きさの倍率
	static constexpr float kStampSlamEnd = 0.3f;
	static constexpr float kStampStartScale = 3.0f;
	// 文字がはっきり見えるようになる覆い具合
	static constexpr float kStampTextAppearEnd = 0.1f;
	// 文字の画像を表示する大きさの倍率
	static constexpr float kStampTextScale = 0.9f;
	// 文字の中心の高さ(画面の高さに対する割合)
	static constexpr float kStampHeightRate = 0.45f;
	// 押した後に文字を左右に揺らす幅(ピクセル)・速さ(覆い具合1あたりの位相の進み、ラジアン)・揺れが収まる覆い具合
	static constexpr float kStampShakeWidth = 12.0f;
	static constexpr float kStampShakeSpeed = 120.0f;
	static constexpr float kStampShakeEnd = 0.45f;
	// 押した衝撃で飛ぶしみが広がりきる覆い具合
	static constexpr float kStampSplashEnd = 0.5f;
	// 中央からしみが広がり始める覆い具合と、広がりきったときの大きさ(画面の高さに対する倍率)と傾き
	static constexpr float kStampSpreadStart = 0.55f;
	static constexpr float kStampSpreadScale = 4.5f;
	static constexpr float kStampSpreadAngle = 0.3f;
	// 隙間を埋めるため、画面全体をインクの色で塗り始める覆い具合
	static constexpr float kStampFillStart = 0.85f;
	// 画面を戻すときに、全体の塗りが消えきる進み具合と、広がったしみが落ちていく距離(画面の高さに対する倍率)
	static constexpr float kStampFillRevealEnd = 0.25f;
	static constexpr float kStampSpreadFallScale = 1.5f;
};
