#pragma once

#include "MyMath.h"

/// <summary>
/// 衝撃の瞬間の画面演出(ヒットストップと画面の揺れ)
/// ヒットストップ中は、呼び出し側が演出の時間を止める(IsHitStopping() を見て経過時間を0にする)。
/// 画面の揺れは、カメラの位置に足すずらし量として返し、時間とともに弱めて消す。
/// </summary>
class ScreenImpact{
public:
	/// <summary>
	/// 衝撃を起こす
	/// </summary>
	/// <param name="hitStopDuration">演出の時間を止める長さ(秒)</param>
	/// <param name="shakeAmplitude">画面の揺れの最大の幅</param>
	/// <param name="shakeDuration">画面の揺れが収まるまでの時間(秒)</param>
	void Trigger(float hitStopDuration,float shakeAmplitude,float shakeDuration);

	// 衝撃を止めて、何もしていない状態に戻す
	void Reset();

	// ヒットストップと揺れの残り時間を進める
	void Update(float deltaTime);

	// ヒットストップ中か
	bool IsHitStopping() const{ return hitStopTimer_ > 0.0f; }

	// 画面が揺れている最中か
	bool IsShaking() const{ return shakeTimer_ > 0.0f; }

	/// <summary>
	/// 画面の揺れによるカメラのずらし量を求める
	/// </summary>
	/// <param name="right">カメラの右方向(正規化済み)</param>
	/// <param name="up">カメラの上方向(正規化済み)</param>
	Vector3 GetShakeOffset(const Vector3& right,const Vector3& up) const;

private:
	// 揺れの速さ(1秒あたりの位相の進み、ラジアン)
	// 右・上で割り切れない速さにして、同じ形を繰り返さない不規則な揺れにする
	static constexpr float kShakeSpeedRight = 47.0f;
	static constexpr float kShakeSpeedUp = 61.0f;

	// ヒットストップの残り時間(秒)
	float hitStopTimer_ = 0.0f;
	// 揺れの残り時間と全体の長さ(秒)
	float shakeTimer_ = 0.0f;
	float shakeDuration_ = 0.0f;
	// 揺れの最大の幅
	float shakeAmplitude_ = 0.0f;
	// 揺れ始めてからの経過時間(揺れの位相に使う)
	float shakeElapsed_ = 0.0f;
};
