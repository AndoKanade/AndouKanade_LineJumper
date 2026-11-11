#pragma once

#include "systems/transition/ITransitionEffect.h"
#include <memory>

/// <summary>
/// 画面を覆う演出: 画面を暗くして斜めの帯と「RETRY」の文字を差し込み、黒の帯を広げて覆う
/// </summary>
class RetryCutInTransition : public ITransitionEffect{
public:
	RetryCutInTransition();
	~RetryCutInTransition() override;

	void Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite) override;
	void Finalize() override;
	float GetCoverDuration() const override{ return kCutInCoverDuration; }
	float GetRevealDuration() const override{ return kCutInRevealDuration; }
	void Update(float coverRate,bool isRevealing) override;
	void Draw() override;

private:
	// 画面全体を覆うスプライト(SceneTransition が持つ。黒にして画面を暗くするのに使う)
	Sprite* fadeSprite_ = nullptr;
	// RetryCutIn の、広がって画面を覆う黒の帯・差し込む色付きの帯・「RETRY」の文字
	std::unique_ptr<Sprite> cutInCoverSprite_;
	std::unique_ptr<Sprite> cutInBandSprite_;
	std::unique_ptr<Sprite> cutInTextSprite_;

	// RetryCutIn の調整項目
	// 画面を覆いきる・戻しきるまでの時間(秒。文字を読ませるため覆う側を長めにする)
	static constexpr float kCutInCoverDuration = 1.0f;
	static constexpr float kCutInRevealDuration = 0.45f;
	// 帯と文字の傾き(ラジアン。マイナスで右上がり)
	static constexpr float kCutInAngle = -0.2f;
	// 色付きの帯の太さ(ピクセル)
	static constexpr float kCutInBandThickness = 220.0f;
	// 画面を暗くする濃さ
	static constexpr float kCutInTintAlpha = 0.5f;
	// 帯と文字が差し込み終わる覆い具合
	static constexpr float kCutInSlideEnd = 0.25f;
	// 黒の帯が広がり始める覆い具合
	static constexpr float kCutInCoverStart = 0.6f;
	// 帯が止まっている間に文字をゆっくり流す距離(ピクセル)
	static constexpr float kCutInTextDrift = 40.0f;
	// 文字の画像を拡大する倍率
	static constexpr float kCutInTextScale = 1.5f;
	// 画面を戻すときに、帯と文字が抜けきる進み具合
	static constexpr float kCutInExitEnd = 0.5f;
};
