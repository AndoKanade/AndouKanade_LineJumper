#pragma once

#include "Title/Departure/BaseDeparture.h"

/// <summary>
/// 出発の演出: スーパージャンプ
/// 出発台の上で身をかがめて溜め、真上へ高く打ち上がる。空の上からこれから走るコースを見下ろす画を見せてから、
/// レールの始点へ急降下して着地し、ヒットストップ・画面の揺れ・放射ブラーと一緒に「GO!」を出す。
/// 飛んでいる間は空中からインクのしぶきを落とし、台に落ちたところにインクの跡を残す。
/// </summary>
class SuperJumpDeparture : public BaseDeparture{
public:
	void Begin(const DepartureContext& context,const CameraPose& startCamera) override;
	void Update(float deltaTime,const CameraPose& gameplayCamera) override;
	bool IsFinished() const override{ return state_ == State::Finished; }
	bool GetCameraPose(CameraPose& outPose) const override;
	void Reset() override{
		state_ = State::Finished;
		isControllingCamera_ = false;
		isBoardedPending_ = false;
	}

private:
	// 演出の段階
	enum class State{
		Charge,   // 出発台の中央へ寄りながら身をかがめて溜める
		Flight,   // 打ち上がってレールの始点へ落ちていく
		Finished, // 着地して終わった
	};

	/// <summary>
	/// 飛んでいる途中の位置を求める(出発台からレールの始点へ水平に進みながら、放物線で上下する)
	/// </summary>
	/// <param name="progress">飛び始めてからの進み具合(0〜1)</param>
	Vector3 GetFlightPosition(float progress) const;

	DepartureContext context_;
	State state_ = State::Finished;
	// 今の段階に入ってからの経過時間(秒)
	float timer_ = 0.0f;

	// 線を越えた位置・出発台の中央・着地するレールの始点
	Vector3 startPosition_ = {0.0f, 0.0f, 0.0f};
	Vector3 padPosition_ = {0.0f, 0.0f, 0.0f};
	Vector3 landPosition_ = {0.0f, 0.0f, 0.0f};
	// レールの始点での水平な前方向・右方向と、モデルを前へ向ける Y 回転
	Vector3 forward_ = {0.0f, 0.0f, 1.0f};
	Vector3 right_ = {1.0f, 0.0f, 0.0f};
	float yaw_ = 0.0f;

	// 始めた瞬間に映っていたカメラ・打ち上がる様子を見上げるカメラの位置
	CameraPose startCamera_;
	Vector3 watchPosition_ = {0.0f, 0.0f, 0.0f};
	// このフレームの演出のカメラ
	CameraPose cameraPose_;
	bool isControllingCamera_ = false;

	// --- 溜め ---
	// 溜める時間(秒)
	static constexpr float kChargeDuration = 0.5f;
	// 溜めきったときに縦に縮める割合と、縮めた分だけ横に広げる割合
	static constexpr float kChargeSquash = 0.35f;
	static constexpr float kSquashWidenRate = 0.5f;
	// 打ち上がる瞬間に足元から飛び散らせるしぶきの数と、画面の揺れの幅・収まるまでの時間(秒)
	static constexpr int kLaunchBurstCount = 12;
	static constexpr float kLaunchShakeAmplitude = 0.08f;
	static constexpr float kLaunchShakeDuration = 0.25f;

	// --- 飛ぶ ---
	// 飛んでいる時間(秒)と、最も高いところの高さ(出発台とレールの始点を結ぶ線から)
	static constexpr float kFlightDuration = 2.2f;
	static constexpr float kApexHeight = 18.0f;
	// 進み具合0〜1の放物線 s*(1-s) の頂点(s=0.5で0.25)を1にそろえるための係数
	static constexpr float kParabolaPeakScale = 4.0f;
	// 上下の速さに応じて縦に伸ばす割合(速く上下しているときほど細長くする)
	static constexpr float kFlightStretch = 0.6f;
	// 飛んでいる間に1フレームごとに落とすしぶきの数
	static constexpr int kTrailSplashCount = 1;

	// --- カメラ ---
	// 打ち上がる様子を見上げるカメラの、出発台からの位置(後ろ・右・上)と、見上げる注目点の足元からの高さ
	static constexpr float kWatchBack = 3.5f;
	static constexpr float kWatchSide = 2.0f;
	static constexpr float kWatchHeight = 0.6f;
	static constexpr float kWatchLookHeight = 0.5f;
	// 空からコースを見下ろすカメラの、着地点からの位置(後ろ・上)と、注目する点(着地点から前)の距離
	static constexpr float kAerialBack = 10.0f;
	static constexpr float kAerialHeight = 28.0f;
	static constexpr float kAerialLookAhead = 25.0f;
	// 飛んでいる進み具合のうち、見上げる画から空の画へ移る範囲と、空の画からプレイ用カメラへ戻る範囲
	static constexpr float kToAerialStart = 0.15f;
	static constexpr float kToAerialEnd = 0.5f;
	static constexpr float kToGameplayStart = 0.6f;
	static constexpr float kToGameplayEnd = 1.0f;

	// --- 着地 ---
	// 着地の瞬間のヒットストップ・画面の揺れの幅・収まるまでの時間(秒)
	static constexpr float kLandHitStopDuration = 0.08f;
	static constexpr float kLandShakeAmplitude = 0.2f;
	static constexpr float kLandShakeDuration = 0.45f;
	// 着地の瞬間に着地点を中心にかける放射ブラーの強さと、消えるまでの時間(秒)
	static constexpr float kLandRadialBlurWidth = 0.02f;
	static constexpr float kLandRadialBlurDuration = 0.35f;
};
