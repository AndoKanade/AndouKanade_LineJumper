#include "ScreenImpact.h"
#include <algorithm>
#include <cmath>

// 衝撃を起こす
void ScreenImpact::Trigger(float hitStopDuration,float shakeAmplitude,float shakeDuration){
	hitStopTimer_ = hitStopDuration;
	shakeTimer_ = shakeDuration;
	shakeDuration_ = shakeDuration;
	shakeAmplitude_ = shakeAmplitude;
	shakeElapsed_ = 0.0f;
}

// 衝撃を止めて、何もしていない状態に戻す
void ScreenImpact::Reset(){
	hitStopTimer_ = 0.0f;
	shakeTimer_ = 0.0f;
	shakeElapsed_ = 0.0f;
}

// ヒットストップと揺れの残り時間を進める
// 揺れはヒットストップ中も進め、止まった画が揺れることで衝撃の強さを伝える
void ScreenImpact::Update(float deltaTime){
	hitStopTimer_ = (std::max)(hitStopTimer_ - deltaTime,0.0f);
	if(shakeTimer_ > 0.0f){
		shakeTimer_ = (std::max)(shakeTimer_ - deltaTime,0.0f);
		shakeElapsed_ += deltaTime;
	}
}

// 画面の揺れによるカメラのずらし量を求める
// 残り時間の割合の2乗で幅を小さくし、最初は強く揺れて素早く収まるようにする
Vector3 ScreenImpact::GetShakeOffset(const Vector3& right,const Vector3& up) const{
	if(shakeTimer_ <= 0.0f || shakeDuration_ <= 0.0f){
		return {0.0f, 0.0f, 0.0f};
	}
	float remainRate = shakeTimer_ / shakeDuration_;
	float amplitude = shakeAmplitude_ * remainRate * remainRate;
	return right * (std::sin(shakeElapsed_ * kShakeSpeedRight) * amplitude) + up * (std::sin(shakeElapsed_ * kShakeSpeedUp) * amplitude);
}
