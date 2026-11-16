#include "DamageInkEffect.h"
#include "Application.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace{
	// 脈打つ動きの sin の結果(-1〜1)を 0〜1 にする係数
	constexpr float kHalf = 0.5f;
	// 1周の角度(ラジアン)
	constexpr float kFullTurn = 2.0f * std::numbers::pi_v<float>;
}

// 体力の表現を消し、すぐにポストプロセスからも外す
void DamageInkEffect::Reset(){
	displayedAmount_ = 0.0f;
	flash_ = 0.0f;
	pulseTimer_ = 0.0f;
	previousHpRate_ = kNoPreviousHpRate;
	Application::GetInstance()->SetDamageEffect(0.0f,0.0f);
}

// 体力からインクの広がり・色づきを求め、ポストプロセスに渡す
void DamageInkEffect::Update(float hpRate,bool isVisible,float deltaTime){
	if(!isVisible){
		Reset();
		return;
	}

	// 体力が前のフレームより大きく減った瞬間を被弾として、画面全体を一瞬インクの色に寄せる
	if(previousHpRate_ >= 0.0f && previousHpRate_ - hpRate >= kFlashMinDropRate){
		flash_ = 1.0f;
	}
	previousHpRate_ = hpRate;
	flash_ = (std::max)(0.0f,flash_ - deltaTime / kFlashDuration);

	// 体力の減り具合(0: 満タン 〜 1: 尽きた)へ、インクの広がりを少しずつ寄せる
	// やり直しで体力が満タンに戻ったときも、インクが一瞬で消えずに引いていくように見える
	// 体力は自然回復で少しずつ戻るため、インクも時間とともに少しずつ引いていく
	const float targetAmount = std::clamp(1.0f - hpRate,0.0f,1.0f);
	displayedAmount_ += (targetAmount - displayedAmount_) * (std::min)(1.0f,kFollowSpeed * deltaTime);

	// 体力が残り少ない間は、鼓動のようにインクを脈打たせる
	float pulse = 0.0f;
	if(hpRate > 0.0f && hpRate <= kDangerHpRate){
		pulseTimer_ += deltaTime;
		pulse = kPulseAmplitude * (kHalf + kHalf * std::sin(kFullTurn * kPulseFrequency * pulseTimer_));
	} else{
		pulseTimer_ = 0.0f;
	}

	float amount = displayedAmount_ + pulse;
	if(amount < kMinVisibleAmount){
		amount = 0.0f;
	}
	Application::GetInstance()->SetDamageEffect(amount,flash_);
}
