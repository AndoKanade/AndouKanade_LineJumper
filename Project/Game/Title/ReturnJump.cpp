#include "ReturnJump.h"
#include "Easing.h"
#include "Camera/ScreenImpact.h"
#include "objects/InkEffectManager.h"
#include "objects/Player.h"
#include "objects/StartPlatform.h"
#include <cmath>

namespace{
	// 上方向
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};
	// 通常の大きさの倍率
	constexpr Vector3 kNormalScale = {1.0f, 1.0f, 1.0f};
}

// 演出を始める
void ReturnJump::Begin(const DepartureContext& context){
	context_ = context;
	state_ = State::Wait;
	timer_ = 0.0f;
	isControllingCamera_ = false;
	isLaunchedPending_ = false;
	isLandedPending_ = false;

	launchPosition_ = context_.player->GetPosition();
	landPosition_ = context_.startPlatform->GetSpawnPosition();

	// 跳ぶ向きは、跳ぶ位置からスタート台への水平な向きにする(真上に近いときは今の向きのまま +Z を使う)
	const Vector3 toLand = landPosition_ - launchPosition_;
	const Vector3 toLandXZ = {toLand.x, 0.0f, toLand.z};
	if(Length(toLandXZ) >= kMinFlightHorizontalLength){
		flightDirection_ = Normalize(toLandXZ);
	}
	yaw_ = std::atan2(flightDirection_.x,flightDirection_.z);
}

// 途中で止める
void ReturnJump::Reset(){
	state_ = State::Finished;
	timer_ = 0.0f;
	isControllingCamera_ = false;
	isLaunchedPending_ = false;
	isLandedPending_ = false;
}

// 値が start から end へ進む間を 0〜1 に直す
float ReturnJump::StageRate(float value,float start,float end){
	float rate = (value - start) / (end - start);
	return rate < 0.0f?0.0f:(rate > 1.0f?1.0f:rate);
}

// 跳んでいる途中の位置を求める
Vector3 ReturnJump::GetFlightPosition(float progress) const{
	return Lerp(launchPosition_,landPosition_,progress) + kWorldUp * (kApexHeight * kParabolaPeakScale * progress * (1.0f - progress));
}

// 演出を進める
void ReturnJump::Update(float deltaTime,const CameraPose& gameplayCamera){
	timer_ += deltaTime;

	switch(state_){
	case State::Wait:
		// 壊れた的の破片を見せる間は、プレイ用カメラのまま、その場で待つ
		if(timer_ >= kWaitDuration){
			state_ = State::Charge;
			timer_ = 0.0f;
		}
		break;

	case State::Charge:
	{
		// スタート台の方を向き、縦に縮んで横に広がり、力を溜めているように見せる
		const float progress = StageRate(timer_,0.0f,kChargeDuration);
		const float squash = Easing::EaseOutCubic(progress) * kChargeSquash;
		context_.player->SetDirectedPose(launchPosition_,{0.0f, yaw_, 0.0f},{1.0f + squash * kSquashWidenRate, 1.0f - squash, 1.0f + squash * kSquashWidenRate});

		if(progress >= 1.0f){
			// 打ち上がった瞬間の画から追いかけるカメラへ動かし始める
			launchCamera_ = gameplayCamera;
			cameraPose_ = gameplayCamera;
			isControllingCamera_ = true;
			// GameScene にプレイヤーをスタート台の上へ移してもらう
			isLaunchedPending_ = true;
			// 足元からしぶきを高く飛び散らせ、画面を少し揺らす
			context_.inkEffect->EmitLaunchBurst(launchPosition_,kLaunchBurstCount);
			context_.screenImpact->Trigger(0.0f,kLaunchShakeAmplitude,kLaunchShakeDuration);
			state_ = State::Flight;
			timer_ = 0.0f;
		}
		break;
	}

	case State::Flight:
	{
		const float progress = StageRate(timer_,0.0f,kFlightDuration);
		const Vector3 position = GetFlightPosition(progress);

		// 上下の速さ(放物線の傾き。打ち上がった直後と落ちる直前で最も速い)に応じて縦に伸ばす
		// 体積が変わって見えないよう、伸ばした分だけ横を細くする
		const float verticalSpeedRate = std::abs(1.0f - progress - progress);
		const float stretch = 1.0f + kFlightStretch * verticalSpeedRate;
		const float thin = 1.0f / std::sqrt(stretch);
		context_.player->SetDirectedPose(position,{0.0f, yaw_, 0.0f},{thin, stretch, thin});

		// 跳んでいる向きの逆へインクのしぶきを落とす
		const Vector3 velocity = (landPosition_ - launchPosition_) + kWorldUp * (kApexHeight * kParabolaPeakScale * (1.0f - progress - progress));
		context_.inkEffect->EmitInkSplash(position,-Normalize(velocity),kTrailSplashCount);

		// カメラは、打ち上がった瞬間の画 → 後ろ上から追いかける画 → プレイ用カメラ(スタート台の上の画)の順に移す
		const CameraPose chasePose = CameraPoseMath::LookAt(position - flightDirection_ * kChaseBack + kWorldUp * kChaseHeight,position);
		if(progress < kToGameplayStart){
			cameraPose_ = CameraPoseMath::Lerp(launchCamera_,chasePose,Easing::EaseInOutCubic(StageRate(progress,0.0f,kToChaseEnd)));
		} else{
			cameraPose_ = CameraPoseMath::Lerp(chasePose,gameplayCamera,Easing::EaseInOutCubic(StageRate(progress,kToGameplayStart,kToGameplayEnd)));
		}

		if(progress >= 1.0f){
			// 着地: プレイヤーの表示を通常に戻し、GameScene にステージを始め直してもらう
			context_.player->SetDirectedPose(landPosition_,{0.0f, yaw_, 0.0f},kNormalScale);
			isLandedPending_ = true;
			isControllingCamera_ = false;
			state_ = State::Finished;
		}
		break;
	}

	case State::Finished:
	default:
		break;
	}
}

// 演出のカメラの位置と向きを取得する
bool ReturnJump::GetCameraPose(CameraPose& outPose) const{
	if(!isControllingCamera_){
		return false;
	}
	outPose = cameraPose_;
	return true;
}

// このフレームで打ち上がったかを受け取る
bool ReturnJump::ConsumeLaunched(){
	if(!isLaunchedPending_){
		return false;
	}
	isLaunchedPending_ = false;
	return true;
}

// このフレームでスタート台に着地したかを受け取る
bool ReturnJump::ConsumeLanded(Vector3& outPosition){
	if(!isLandedPending_){
		return false;
	}
	isLandedPending_ = false;
	outPosition = landPosition_;
	return true;
}
