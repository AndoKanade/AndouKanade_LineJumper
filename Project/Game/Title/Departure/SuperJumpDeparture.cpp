#include "SuperJumpDeparture.h"
#include "Application.h"
#include "Camera.h"
#include "Easing.h"
#include "Camera/ScreenImpact.h"
#include "Editor/RailEditor.h"
#include "objects/InkEffectManager.h"
#include "objects/Player.h"
#include "objects/StartPlatform.h"
#include <cmath>

namespace{
	// 上方向
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};
	// レールの始点の進行度
	constexpr float kRailStartT = 0.0f;
	// 通常の大きさの倍率
	constexpr Vector3 kNormalScale = {1.0f, 1.0f, 1.0f};
}

// 出発の演出を始める
void SuperJumpDeparture::Begin(const DepartureContext& context,const CameraPose& startCamera){
	context_ = context;
	startCamera_ = startCamera;
	cameraPose_ = startCamera;
	isControllingCamera_ = true;
	state_ = State::Charge;
	timer_ = 0.0f;
	isBoardedPending_ = false;

	startPosition_ = context_.player->GetPosition();
	padPosition_ = context_.startPlatform->GetDeparturePadPosition();
	landPosition_ = context_.railEditor->GetPositionOnRail(kRailStartT);

	// 向きはレールの始点の進行方向(上下の成分を除く)にそろえる
	Vector3 railForward = context_.railEditor->GetForwardOnRail(kRailStartT);
	forward_ = Normalize(Vector3{railForward.x, 0.0f, railForward.z});
	right_ = {forward_.z, 0.0f, -forward_.x};
	yaw_ = std::atan2(forward_.x,forward_.z);

	// 打ち上がる様子は、出発台の右斜め後ろの低い位置から見上げる
	watchPosition_ = padPosition_ - forward_ * kWatchBack + right_ * kWatchSide + kWorldUp * kWatchHeight;
}

// 飛んでいる途中の位置を求める
Vector3 SuperJumpDeparture::GetFlightPosition(float progress) const{
	return Lerp(padPosition_,landPosition_,progress) + kWorldUp * (kApexHeight * kParabolaPeakScale * progress * (1.0f - progress));
}

// 演出を進める
void SuperJumpDeparture::Update(float deltaTime,const CameraPose& gameplayCamera){
	timer_ += deltaTime;

	switch(state_){
	case State::Charge:
	{
		// 出発台の中央へ寄りながら、縦に縮んで横に広がり、力を溜めているように見せる
		const float progress = StageRate(timer_,0.0f,kChargeDuration);
		const float squash = Easing::EaseOutCubic(progress) * kChargeSquash;
		const Vector3 position = Lerp(startPosition_,padPosition_,Easing::EaseOutCubic(progress));
		context_.player->SetDirectedPose(position,{0.0f, yaw_, 0.0f},{1.0f + squash * kSquashWidenRate, 1.0f - squash, 1.0f + squash * kSquashWidenRate});

		// カメラは始めた瞬間の画から、出発台を斜め後ろから見上げる画へ寄せる
		const CameraPose watchPose = CameraPoseMath::LookAt(watchPosition_,position + kWorldUp * kWatchLookHeight);
		cameraPose_ = CameraPoseMath::Lerp(startCamera_,watchPose,Easing::EaseInOutCubic(progress));

		if(progress >= 1.0f){
			// 打ち上がる瞬間にレールの始点へ乗せておく(プレイ用カメラが着地後の画へ動き始め、最後にその画へ戻せる)
			context_.player->BoardRailStart();
			// 足元からしぶきを高く飛び散らせ(台に落ちたところに跡が残る)、画面を少し揺らす
			context_.inkEffect->EmitLaunchBurst(padPosition_,kLaunchBurstCount);
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

		// 飛んでいる向きの逆へインクのしぶきを落とす
		const Vector3 velocity = (landPosition_ - padPosition_) + kWorldUp * (kApexHeight * kParabolaPeakScale * (1.0f - progress - progress));
		context_.inkEffect->EmitInkSplash(position,-Normalize(velocity),kTrailSplashCount);

		// カメラは、見上げる画 → 空からコースを見下ろす画 → プレイ用カメラ(着地後の画)の順に移す
		const CameraPose watchPose = CameraPoseMath::LookAt(watchPosition_,position);
		const CameraPose aerialPose = CameraPoseMath::LookAt(landPosition_ - forward_ * kAerialBack + kWorldUp * kAerialHeight,landPosition_ + forward_ * kAerialLookAhead);
		if(progress < kToGameplayStart){
			cameraPose_ = CameraPoseMath::Lerp(watchPose,aerialPose,Easing::EaseInOutCubic(StageRate(progress,kToAerialStart,kToAerialEnd)));
		} else{
			cameraPose_ = CameraPoseMath::Lerp(aerialPose,gameplayCamera,Easing::EaseInOutCubic(StageRate(progress,kToGameplayStart,kToGameplayEnd)));
		}

		if(progress >= 1.0f){
			// 着地: プレイヤーの表示を通常に戻し、着地点を中心に衝撃を見せて「GO!」を出す
			context_.player->SetDirectedPose(landPosition_,{0.0f, yaw_, 0.0f},kNormalScale);
			context_.screenImpact->Trigger(kLandHitStopDuration,kLandShakeAmplitude,kLandShakeDuration);
			if(context_.camera){
				Application::GetInstance()->TriggerRadialBlur(CameraPoseMath::WorldToScreenUv(landPosition_,context_.camera->GetViewProjectionMatrix()),kLandRadialBlurWidth,kLandRadialBlurDuration);
			}
			NotifyBoarded(landPosition_);
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
bool SuperJumpDeparture::GetCameraPose(CameraPose& outPose) const{
	if(!isControllingCamera_){
		return false;
	}
	outPose = cameraPose_;
	return true;
}
