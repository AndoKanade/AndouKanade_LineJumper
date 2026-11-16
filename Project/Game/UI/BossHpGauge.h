#pragma once

#include "MyMath.h"
#include <memory>
#include <vector>

// 前方宣言
class Sprite;
class SpriteCommon;

/// <summary>
/// ボスの体力ゲージ
/// 画面の上に横長のゲージを出し、今の形態の体力を表す。色は形態ごとに変え、残っている形態の数をゲージの下の小さな印で表す。
/// 減った分は白い帯が少し遅れて追いかけ、どれだけ減らしたかが分かるようにする。形態が変わったときは、ゲージが満タンまで伸びて戻る。
/// ImGui を使わずスプライトで描くため、Release 構成でも表示される。
/// </summary>
class BossHpGauge{
public:
	BossHpGauge();
	~BossHpGauge();

	/// <summary>
	/// テクスチャの読み込みとスプライトの生成
	/// </summary>
	/// <param name="spriteCommon">スプライト共通設定</param>
	void Initialize(SpriteCommon* spriteCommon);

	// 消した状態に戻す
	void Reset();

	/// <summary>
	/// 表示内容の更新
	/// </summary>
	/// <param name="isShown">ゲージを出すか(ボスと戦っている間だけ true)</param>
	/// <param name="hp">今の形態の体力</param>
	/// <param name="maxHp">今の形態の体力の最大値</param>
	/// <param name="phase">今の形態(1 始まり)</param>
	/// <param name="deltaTime">経過時間(秒)</param>
	void Update(bool isShown,int hp,int maxHp,int phase,float deltaTime);

	// 描画処理(スプライト共通の描画前処理も含む。見えていないときは何もしない)
	void Draw();

	// ボスの形態の数(ゲージの下に並べる印の数)
	static constexpr int kPhaseCount = 2;

private:
	SpriteCommon* spriteCommon_ = nullptr;

	// 下地(暗い帯)・遅れて追いかける白い帯・体力の帯
	std::unique_ptr<Sprite> frameSprite_;
	std::unique_ptr<Sprite> delaySprite_;
	std::unique_ptr<Sprite> fillSprite_;
	// 残っている形態の数を表す印(形態の数だけ並べる)
	std::vector<std::unique_ptr<Sprite>> phaseSprites_;

	// 表示の濃さ(0: 見えない 〜 1: 見えている)
	float alpha_ = 0.0f;
	// 表示している体力の割合(0〜1)と、遅れて追いかける白い帯の割合
	float displayedRate_ = 1.0f;
	float delayRate_ = 1.0f;
	// 前のフレームの形態(変わったら、ゲージを空から満タンまで伸ばす)
	int previousPhase_ = 0;

	// ゲージの下地の大きさ(ピクセル)と、画面上の位置(画面の幅・高さに対する割合。下地の中心)
	static constexpr Vector2 kFrameSize = {720.0f, 28.0f};
	static constexpr float kCenterXRate = 0.5f;
	static constexpr float kHeightRate = 0.045f;
	// 下地の縁の太さ(体力の帯は、下地よりこの分だけ内側に描く)
	static constexpr float kFramePadding = 4.0f;
	// 形態の印の大きさ・印どうしの間隔・ゲージの下の端から印の上の端までのすき間(ピクセル)
	static constexpr Vector2 kPhaseMarkSize = {28.0f, 10.0f};
	static constexpr float kPhaseMarkSpacing = 8.0f;
	static constexpr float kPhaseMarkGap = 6.0f;

	// 出すとき・消すときに濃さを変える速さ(1秒あたりの変化量)
	static constexpr float kFadeInSpeed = 3.0f;
	static constexpr float kFadeOutSpeed = 2.0f;
	// 形態が変わってゲージを伸ばすときの速さ(1秒あたりに伸びる割合)
	static constexpr float kRefillSpeed = 0.7f;
	// 白い帯が体力の帯を追いかける速さ(1秒あたりに縮む割合)
	static constexpr float kDelayShrinkSpeed = 0.35f;

	// 帯に使うテクスチャ(白一色。色はスプライトの色で付ける)
	static inline const char* kWhiteTexturePath = "resource/RailTube/white.png";
	// 左端を基準にする帯の基準点(左端の中央)と、中央を基準にする下地の基準点
	static constexpr Vector2 kLeftCenterAnchor = {0.0f, 0.5f};
	static constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
	// 下地・白い帯・使い切った形態の印の色
	static constexpr Vector4 kFrameColor = {0.05f, 0.07f, 0.12f, 0.85f};
	static constexpr Vector4 kDelayColor = {1.0f, 1.0f, 1.0f, 1.0f};
	static constexpr Vector4 kUsedPhaseColor = {0.3f, 0.3f, 0.35f, 1.0f};
	// 形態ごとの体力の帯の色(1形態目はボスの体と同じ黄色、2形態目は紫)
	static constexpr Vector4 kPhaseColors[kPhaseCount] = {
		{1.0f, 0.85f, 0.2f, 1.0f},
		{0.7f, 0.45f, 1.0f, 1.0f}
	};
};
