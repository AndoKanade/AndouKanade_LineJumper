#pragma once

#include "Title/Departure/BaseDeparture.h"

/// <summary>
/// 出発の演出: 発射台(カタパルト)
/// 線を越えるとレールの始点へ移って乗り、低い位置のカメラで背中に寄りながら、3回に分けて身を沈めて力を溜める。
/// 溜めきったら「GO!」と同時に、レールを進む速さを大きく上げて飛び出し、視野角を一瞬広げて放射ブラーをかけ、勢いを見せる。
/// 速さは少しずつ元に戻るため、勢いのままステージへ入っていく。
/// </summary>
class CatapultDeparture : public BaseDeparture{
public:
	void Begin(const DepartureContext& context,const CameraPose& startCamera) override;
	void Update(float deltaTime,const CameraPose& gameplayCamera) override;
	bool IsFinished() const override{ return state_ == State::Finished; }
	bool GetCameraPose(CameraPose& outPose) const override;
	void Reset() override;

private:
	// 演出の段階
	enum class State{
		Approach, // レールの始点へ跳ねて移る
		Charge,   // 3回に分けて身を沈めて溜める
		Launch,   // 飛び出す(カメラをプレイ用カメラへ戻し、広げた視野角を元に戻す)
		Finished, // 終わった
	};

	/// <summary>
	/// 溜めている間のカメラの位置と向きを求める(溜めが進むほど背中へ寄せる)
	/// </summary>
	/// <param name="chargeProgress">溜めの進み具合(0〜1)</param>
	CameraPose GetChargeCameraPose(float chargeProgress) const;

	// 視野角を元に戻す
	void RestoreFov();

	DepartureContext context_;
	State state_ = State::Finished;
	// 今の段階に入ってからの経過時間(秒)
	float timer_ = 0.0f;
	// 最後に溜めの合図(揺れ・しぶき)を出した回数
	int chargeBeatCount_ = 0;

	// 線を越えた位置と、飛び出すレールの始点
	Vector3 startPosition_ = {0.0f, 0.0f, 0.0f};
	Vector3 launchPosition_ = {0.0f, 0.0f, 0.0f};
	// レールの始点での水平な前方向・右方向と、モデルを前へ向ける Y 回転
	Vector3 forward_ = {0.0f, 0.0f, 1.0f};
	Vector3 right_ = {1.0f, 0.0f, 0.0f};
	float yaw_ = 0.0f;

	// 始めた瞬間に映っていたカメラと、このフレームの演出のカメラ
	CameraPose startCamera_;
	CameraPose cameraPose_;
	bool isControllingCamera_ = false;
	// 演出を始める前の視野角(飛び出した後に戻す)と、視野角を変えているか
	float baseFov_ = 0.0f;
	bool isFovChanged_ = false;

	// --- レールの始点へ移る ---
	// 移るのにかける時間(秒)と、跳ねる高さ
	static constexpr float kApproachDuration = 0.35f;
	static constexpr float kApproachHopHeight = 0.4f;
	// 進み具合0〜1の放物線 s*(1-s) の頂点(s=0.5で0.25)を1にそろえるための係数
	static constexpr float kParabolaPeakScale = 4.0f;

	// --- 溜める ---
	// 溜めの合図の回数と間隔(秒)
	static constexpr int kChargeBeatCount = 3;
	static constexpr float kChargeBeatInterval = 0.45f;
	// 溜めきったときに縦に縮める割合と、合図のたびに一瞬だけ余分に縮める割合
	static constexpr float kChargeSquash = 0.3f;
	static constexpr float kBeatPulseSquash = 0.12f;
	// 縮めた分だけ横に広げる割合
	static constexpr float kSquashWidenRate = 0.5f;
	// 合図のたびの画面の揺れの幅(回数に比例して強くする)・収まるまでの時間(秒)と、足元から跳ねるしぶきの数
	static constexpr float kBeatShakeAmplitude = 0.04f;
	static constexpr float kBeatShakeDuration = 0.18f;
	static constexpr int kBeatSplashCount = 5;
	// 溜めている間のカメラの、レールの始点からの位置(後ろ・右・上)と、注目する点(前・上)
	// 溜めが進むほど後ろの距離を縮めて背中へ寄せる
	static constexpr float kChargeCameraBackStart = 3.0f;
	static constexpr float kChargeCameraBackEnd = 1.8f;
	static constexpr float kChargeCameraSide = 0.5f;
	static constexpr float kChargeCameraHeight = 0.6f;
	static constexpr float kChargeLookAhead = 6.0f;
	static constexpr float kChargeLookHeight = 0.4f;

	// --- 飛び出す ---
	// レールを進む速さの倍率(飛び出した瞬間)と、元の速さへ戻りきるまでの時間(秒)
	static constexpr float kBoostRate = 3.0f;
	static constexpr float kBoostDuration = 1.8f;
	// 飛び出す瞬間のヒットストップ・画面の揺れの幅・収まるまでの時間(秒)
	static constexpr float kLaunchHitStopDuration = 0.06f;
	static constexpr float kLaunchShakeAmplitude = 0.15f;
	static constexpr float kLaunchShakeDuration = 0.3f;
	// 飛び出す瞬間に画面の中央にかける放射ブラーの強さと、消えるまでの時間(秒)
	static constexpr float kLaunchRadialBlurWidth = 0.025f;
	static constexpr float kLaunchRadialBlurDuration = 0.5f;
	// カメラをプレイ用カメラへ戻すのにかける時間(秒)
	static constexpr float kLaunchCameraDuration = 0.35f;
	// 飛び出した瞬間に広げる視野角(ラジアン)と、元の視野角へ戻るまでの時間(秒)
	static constexpr float kFovKick = 0.35f;
	static constexpr float kFovReturnDuration = 1.2f;
};
