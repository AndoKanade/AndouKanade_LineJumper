#pragma once

#include "systems/transition/ITransitionEffect.h"
#include <memory>
#include <vector>

/// <summary>
/// 画面を覆う演出: 暗いインクが画面の上から不揃いに垂れてきて覆い、そのまま下へ沈んで抜けていく(ゲームオーバー)
/// </summary>
class InkDripTransition : public ITransitionEffect{
public:
	InkDripTransition();
	~InkDripTransition() override;

	void Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite) override;
	void Finalize() override;
	float GetCoverDuration() const override{ return kDripCoverDuration; }
	float GetRevealDuration() const override{ return kDripRevealDuration; }
	void Update(float coverRate,bool isRevealing) override;
	void Draw() override;

private:
	// 画面全体を覆うスプライト(SceneTransition が持つ。暗い色にして画面を暗くするのに使う)
	Sprite* fadeSprite_ = nullptr;
	// InkDrip の、垂れるインクの本体と先の丸み(差し色の縁取り用と、本体の色用の2層)
	std::vector<std::unique_ptr<Sprite>> dripRimSprites_;
	std::vector<std::unique_ptr<Sprite>> dripRimTipSprites_;
	std::vector<std::unique_ptr<Sprite>> dripSprites_;
	std::vector<std::unique_ptr<Sprite>> dripTipSprites_;

	// InkDrip の調整項目
	// 画面を覆いきる・戻しきるまでの時間(秒。重さを出すため覆う側を長めにする)
	static constexpr float kDripCoverDuration = 1.1f;
	static constexpr float kDripRevealDuration = 0.7f;
	// 垂れるインクの本数
	static constexpr int kDripCount = 16;
	// 垂れ始める順番をばらけさせるため、番号にこの数を掛けて本数で割った余りを順番にする(本数と互いに素な数)
	static constexpr int kDripOrderStep = 5;
	// 垂れるインクどうしが垂れ始めるタイミングのずれ(覆い具合に対する割合)
	static constexpr float kDripStagger = 0.03f;
	// 最初に画面の上の縁を埋める帯の長さ(画面の高さに対する割合)と、埋め終わる覆い具合
	static constexpr float kDripBandRate = 0.12f;
	static constexpr float kDripBandEnd = 0.25f;
	// 本数で分けた幅に対する、垂れるインクの太さの倍率(不揃いに見せるため順に使う)
	// 隣り合う2本の倍率の和が2以上になるようにして、垂れきったときに隙間ができないようにする
	static constexpr float kDripWidthScales[] = {1.3f, 0.9f, 1.5f, 1.0f, 1.2f, 0.85f, 1.4f, 1.1f};
	// 差し色の縁取りを本体より先に出す長さ(ピクセル)
	static constexpr float kDripRimLead = 8.0f;
	// 垂れている間に画面を暗くする濃さ
	static constexpr float kDripTintAlpha = 0.45f;
	// 隙間が残っても画面を覆いきれるよう、暗くする濃さを1まで上げ始める覆い具合
	static constexpr float kDripFillStart = 0.92f;
	// 画面を戻すときに沈んでいく距離(画面の高さに対する倍率)
	static constexpr float kDripSinkScale = 1.6f;
};
