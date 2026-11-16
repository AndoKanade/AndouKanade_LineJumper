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
	previousHp_ = -1;
	Application::GetInstance()->SetDamageEffect(0.0f,0.0f);
}

// 体力からインクの広がり・色づきを求め、ポストプロセスに渡す
// recoverProgress は、自然回復で次に体力が1戻るまでの進み具合(0〜1)。その分だけ体力が戻りかけているものとして、インクを少しずつ引かせる
void DamageInkEffect::Update(int hp,int maxHp,float recoverProgress,bool isVisible,float deltaTime){
	if(!isVisible){
		Reset();
		return;
	}

	// 体力が前のフレームより減った瞬間を被弾として、画面全体を一瞬インクの色に寄せる
	if(previousHp_ >= 0 && hp < previousHp_){
		flash_ = 1.0f;
	}
	previousHp_ = hp;
	flash_ = (std::max)(0.0f,flash_ - deltaTime / kFlashDuration);

	// 体力の減り具合(0: 満タン 〜 1: 尽きた)へ、インクの広がりを少しずつ寄せる
	// やり直しで体力が満タンに戻ったときも、インクが一瞬で消えずに引いていくように見える
	// 自然回復の進み具合を体力に足して、体力が戻る瞬間にまとめて引くのではなく、待っている間に少しずつ引くようにする
	const float recoveringHp = static_cast<float>(hp) + std::clamp(recoverProgress,0.0f,1.0f);
	const float targetAmount = (maxHp > 0)?std::clamp(1.0f - recoveringHp / static_cast<float>(maxHp),0.0f,1.0f):0.0f;
	displayedAmount_ += (targetAmount - displayedAmount_) * (std::min)(1.0f,kFollowSpeed * deltaTime);

	// 体力が残り少ない間は、鼓動のようにインクを脈打たせる
	float pulse = 0.0f;
	if(hp > 0 && hp <= kDangerHp){
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
