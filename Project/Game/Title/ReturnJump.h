#pragma once

#include "MyMath.h"
#include "Camera/CameraPose.h"
#include "Title/Departure/DepartureContext.h"

/// <summary>
/// ゴールの的を壊したあと、大ジャンプでスタート台へ戻る演出
/// 壊れた的の破片を少し見せてから、その場で身をかがめて溜め、スタート台の出発位置へ向かって高く跳ぶ。
/// 打ち上がった瞬間に ConsumeLaunched() で GameScene に知らせ、プレイヤーをスタート台の上へ移してもらう
/// (プレイ用カメラが着地後の画へ動き始め、最後にその画へ戻せる)。
/// 着地した瞬間に ConsumeLanded() で知らせ、GameScene がステージを始め直して着地の演出を出す。
/// </summary>
class ReturnJump{
public:
	/// <summary>
	/// 演出を始める
	/// </summary>
	/// <param name="context">演出が動かす・参照するもの(出発の演出と同じものを使う)</param>
	void Begin(const DepartureContext& context);

	/// <summary>
	/// 演出を進める(プレイヤーの表示を上書きし、カメラの位置と向きを決める)
	/// プレイヤーの行列更新(Player::UpdateTransform())の後に呼ぶ
	/// </summary>
	/// <param name="deltaTime">経過時間(秒。ヒットストップ中は0)</param>
	/// <param name="gameplayCamera">このフレームのプレイ用カメラの位置と向き(最後はここへ戻す)</param>
	void Update(float deltaTime,const CameraPose& gameplayCamera);

	// 途中で止める(Editモードへ戻ったとき・やり直したときに呼ぶ)
	void Reset();

	// 演出の最中か(最中はプレイヤーの操作・射撃・勝敗判定を止める)
	bool IsRunning() const{ return state_ != State::Finished; }

	// 壊れた的の破片を見せるために、その場で待っている最中か(周りをスローにするのに使う)
	bool IsWaiting() const{ return state_ == State::Wait; }

	/// <summary>
	/// 演出のカメラの位置と向きを取得する
	/// </summary>
	/// <param name="outPose">演出のカメラの位置と向き</param>
	/// <returns>カメラを演出側で動かしている最中なら true(false ならプレイ用カメラのまま)</returns>
	bool GetCameraPose(CameraPose& outPose) const;

	// このフレームで打ち上がったかを受け取る(受け取ったら false に戻る)
	bool ConsumeLaunched();

	/// <summary>
	/// このフレームでスタート台に着地したかを受け取る(受け取ったら false に戻る)
	/// </summary>
	/// <param name="outPosition">着地した位置</param>
	bool ConsumeLanded(Vector3& outPosition);

private:
	// 演出の段階
	enum class State{
		Wait,     // 壊れた的の破片を見せる間、その場で待つ
		Charge,   // その場で身をかがめて溜める
		Flight,   // スタート台へ向かって跳んでいる
		Finished, // 着地して終わった(演出をしていない)
	};

	/// <summary>
	/// 値が start から end へ進む間を 0〜1 に直す(範囲の外は 0 か 1 に収める。演出の段階ごとの進み具合に使う)
	/// </summary>
	static float StageRate(float value,float start,float end);

	/// <summary>
	/// 跳んでいる途中の位置を求める(跳んだ位置からスタート台へ水平に進みながら、放物線で上下する)
	/// </summary>
	/// <param name="progress">跳び始めてからの進み具合(0〜1)</param>
	Vector3 GetFlightPosition(float progress) const;

	DepartureContext context_;
	State state_ = State::Finished;
	// 今の段階に入ってからの経過時間(秒)
	float timer_ = 0.0f;

	// 跳ぶ位置(ゴールの的を壊したときに立っていた位置)と、着地するスタート台の出発位置
	Vector3 launchPosition_ = {0.0f, 0.0f, 0.0f};
	Vector3 landPosition_ = {0.0f, 0.0f, 0.0f};
	// 跳ぶ向き(水平)と、モデルをその向きへ向ける Y 回転
	Vector3 flightDirection_ = {0.0f, 0.0f, 1.0f};
	float yaw_ = 0.0f;

	// 打ち上がった瞬間に映っていたカメラ(ここから追いかけるカメラへ動かす)
	CameraPose launchCamera_;
	// このフレームの演出のカメラ
	CameraPose cameraPose_;
	bool isControllingCamera_ = false;

	// 打ち上がった・着地した知らせをまだ受け取られていないか
	bool isLaunchedPending_ = false;
	bool isLandedPending_ = false;

	// --- 待つ ---
	// ゴールの的が壊れてから溜め始めるまでの時間(秒。壊れた的の破片を見せる)
	static constexpr float kWaitDuration = 1.2f;

	// --- 溜め ---
	// 溜める時間(秒)
	static constexpr float kChargeDuration = 0.5f;
	// 溜めきったときに縦に縮める割合と、縮めた分だけ横に広げる割合
	static constexpr float kChargeSquash = 0.35f;
	static constexpr float kSquashWidenRate = 0.5f;
	// 打ち上がる瞬間に足元から飛び散らせるしぶきの数と、画面の揺れの幅・収まるまでの時間(秒)
	static constexpr int kLaunchBurstCount = 16;
	static constexpr float kLaunchShakeAmplitude = 0.12f;
	static constexpr float kLaunchShakeDuration = 0.3f;

	// --- 跳ぶ ---
	// 跳んでいる時間(秒)と、跳んだ位置とスタート台を結ぶ線から最も高いところまでの高さ
	static constexpr float kFlightDuration = 3.0f;
	static constexpr float kApexHeight = 35.0f;
	// 進み具合0〜1の放物線 s*(1-s) の頂点(s=0.5で0.25)を1にそろえるための係数
	static constexpr float kParabolaPeakScale = 4.0f;
	// 上下の速さに応じて縦に伸ばす割合(速く上下しているときほど細長くする)
	static constexpr float kFlightStretch = 0.6f;
	// 跳んでいる間に1フレームごとに落とすしぶきの数
	static constexpr int kTrailSplashCount = 1;
	// 跳ぶ向きを求められないほど、跳ぶ位置とスタート台が水平に近いときの距離
	static constexpr float kMinFlightHorizontalLength = 0.001f;

	// --- カメラ ---
	// 跳んでいるプレイヤーを追いかけるカメラの、プレイヤーからの位置(跳ぶ向きの後ろ・上)
	static constexpr float kChaseBack = 9.0f;
	static constexpr float kChaseHeight = 4.0f;
	// 跳んでいる進み具合のうち、打ち上がった瞬間の画から追いかける画へ移る範囲と、追いかける画からプレイ用カメラへ戻る範囲
	static constexpr float kToChaseEnd = 0.2f;
	static constexpr float kToGameplayStart = 0.7f;
	static constexpr float kToGameplayEnd = 1.0f;
};
