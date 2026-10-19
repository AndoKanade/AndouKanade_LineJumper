#pragma once

#include "Title/Departure/BaseDeparture.h"

/// <summary>
/// 出発の演出: カウントダウンから宙返りで飛び乗る
/// ゲートの前の目印で出発ゲートを開け始めた瞬間に始め、ゲートの 3・2・1 の合図(ランプが1段ずつ点く)に合わせて、
/// 低い位置のカメラで背中に寄りながら、合図のたびに身を沈めて力を溜める。
/// 0 の合図で遮断棒が跳ね上がると同時に、前へ跳びながら宙返りを1回してレールの始点へ飛び乗る。
/// 飛び乗った瞬間に「GO!」と同時に、レールを進む速さを大きく上げ、視野角を一瞬広げて放射ブラーをかけ、勢いを見せる。
/// </summary>
class CountdownFlipDeparture : public BaseDeparture{
public:
	bool StartsWithGate() const override{ return true; }
	void Begin(const DepartureContext& context,const CameraPose& startCamera) override;
	void Update(float deltaTime,const CameraPose& gameplayCamera) override;
	bool IsFinished() const override{ return state_ == State::Finished; }
	bool GetCameraPose(CameraPose& outPose) const override;
	void Reset() override;

private:
	// 演出の段階
	enum class State{
		Countdown, // ゲートの合図に合わせて身を沈めて溜める
		Flip,      // 宙返りしながらレールの始点へ跳ぶ
		Launch,    // 飛び乗った後(広げた視野角を元に戻す)
		Finished,  // 終わった
	};

	/// <summary>
	/// 溜めている間のカメラの位置と向きを求める(溜めが進むほど背中へ寄せる)
	/// </summary>
	/// <param name="pushRate">背中へ寄せる割合(0〜1)</param>
	CameraPose GetCountdownCameraPose(float pushRate) const;

	// 溜めている間のカメラが注目する点(溜める位置の前の、ゲートの高さの中ほど)
	Vector3 GetCountdownLookTarget() const;

	// 視野角を元に戻す
	void RestoreFov();

	DepartureContext context_;
	State state_ = State::Finished;
	// 今の段階に入ってからの経過時間(秒)
	float timer_ = 0.0f;
	// 受け取った合図の回数と、最後の合図からの経過時間(秒)
	int beatCount_ = 0;
	float beatTimer_ = 0.0f;
	// 前のフレームでゲートが出していたカウントダウンの数(数が変わった瞬間を合図にする)
	int lastCountdownNumber_ = 0;
	// カメラを背中へ寄せている割合(合図のたびに寄せる先が近づき、なめらかに追いかける)
	float cameraPushRate_ = 0.0f;

	// 溜める位置(ゲートを開け始めたときに立っていた位置)と、飛び乗るレールの始点
	Vector3 startPosition_ = {0.0f, 0.0f, 0.0f};
	Vector3 landPosition_ = {0.0f, 0.0f, 0.0f};
	// レールの始点での水平な前方向・右方向と、モデルを前へ向ける Y 回転
	Vector3 forward_ = {0.0f, 0.0f, 1.0f};
	Vector3 right_ = {1.0f, 0.0f, 0.0f};
	float yaw_ = 0.0f;

	// 始めた瞬間に映っていたカメラと、このフレームの演出のカメラ
	CameraPose startCamera_;
	CameraPose cameraPose_;
	bool isControllingCamera_ = false;
	// 演出を始める前の視野角(飛び乗った後に戻す)と、視野角を変えているか
	float baseFov_ = 0.0f;
	bool isFovChanged_ = false;

	// --- 溜める ---
	// すべての合図を受け取ったときに縦に縮める割合と、合図のたびに一瞬だけ余分に縮める割合・元に戻るまでの時間(秒)
	static constexpr float kChargeSquash = 0.3f;
	static constexpr float kBeatPulseSquash = 0.12f;
	static constexpr float kBeatPulseDuration = 0.4f;
	// 縮めた分だけ横に広げる割合
	static constexpr float kSquashWidenRate = 0.5f;
	// 合図のたびの画面の揺れの幅(回数に比例して強くする)・収まるまでの時間(秒)と、足元から跳ねるしぶきの数
	static constexpr float kBeatShakeAmplitude = 0.04f;
	static constexpr float kBeatShakeDuration = 0.18f;
	static constexpr int kBeatSplashCount = 5;
	// 始めた瞬間の画から、溜めている間の画へ移すのにかける時間(秒)
	static constexpr float kCameraBlendDuration = 0.5f;
	// 溜めている間のカメラの、溜める位置からの位置(後ろ・右・上)と、注目する点(前・上)
	// 合図を受け取るほど後ろの距離を縮めて背中へ寄せる(ゲートとランプが画面に入る距離にする)
	static constexpr float kCountdownCameraBackStart = 3.2f;
	static constexpr float kCountdownCameraBackEnd = 2.2f;
	static constexpr float kCountdownCameraSide = 0.6f;
	static constexpr float kCountdownCameraHeight = 0.9f;
	static constexpr float kCountdownLookAhead = 2.5f;
	static constexpr float kCountdownLookHeight = 0.6f;
	// カメラを寄せる先へ追いかける速さ(1秒あたりに残りの差を縮める割合の指数)
	static constexpr float kCameraPushSharpness = 10.0f;

	// --- 宙返りして跳ぶ ---
	// 跳んでいる時間(秒)と、跳ぶ高さ(ゲートの遮断棒と梁にぶつからない高さにする)
	static constexpr float kFlipDuration = 0.7f;
	static constexpr float kFlipJumpHeight = 1.1f;
	// 宙返りの回転の中心の、足元からの高さ(モデルの身長の約半分)
	static constexpr float kFlipPivotHeight = 0.25f;
	// 進み具合0〜1の放物線 s*(1-s) の頂点(s=0.5で0.25)を1にそろえるための係数
	static constexpr float kParabolaPeakScale = 4.0f;
	// 跳んでいる間、カメラの注目する点を溜めていたときの点から体へ移し終える進み具合(0〜1)
	static constexpr float kFlipLookTurnEnd = 0.3f;

	// --- 飛び乗る ---
	// レールを進む速さの倍率(飛び乗った瞬間)と、元の速さへ戻りきるまでの時間(秒)
	static constexpr float kBoostRate = 3.0f;
	static constexpr float kBoostDuration = 1.8f;
	// 飛び乗った瞬間のヒットストップ・画面の揺れの幅・収まるまでの時間(秒)
	static constexpr float kLandHitStopDuration = 0.06f;
	static constexpr float kLandShakeAmplitude = 0.15f;
	static constexpr float kLandShakeDuration = 0.3f;
	// 飛び乗った瞬間に画面の中央にかける放射ブラーの強さと、消えるまでの時間(秒)
	static constexpr float kLandRadialBlurWidth = 0.025f;
	static constexpr float kLandRadialBlurDuration = 0.5f;
	// 飛び乗った瞬間に広げる視野角(ラジアン)と、元の視野角へ戻るまでの時間(秒)
	static constexpr float kFovKick = 0.35f;
	static constexpr float kFovReturnDuration = 1.2f;
};
