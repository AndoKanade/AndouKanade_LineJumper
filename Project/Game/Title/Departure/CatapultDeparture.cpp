#include "CatapultDeparture.h"
#include "Application.h"
#include "Camera.h"
#include "Easing.h"
#include "Camera/ScreenImpact.h"
#include "Editor/RailEditor.h"
#include "objects/InkEffectManager.h"
#include "objects/Player.h"
#include "Title/StartSequence.h"
#include "SoundManager.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>

namespace{
	// 上方向
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};
	// レールの始点の進行度
	constexpr float kRailStartT = 0.0f;
	// 溜めの合図の1回目・2回目・3回目に鳴らす音(回数を重ねるほど高くなる)
	const std::string kChargeSePaths[] = {
		"resource/music/se/catapult_charge1.mp3",
		"resource/music/se/catapult_charge2.mp3",
		"resource/music/se/catapult_charge3.mp3",
	};
	// 飛び出す瞬間に鳴らす音
	const std::string kLaunchSePath = "resource/music/se/catapult_launch.mp3";
}

// 溜め・飛び出すときの効果音を読み込む
CatapultDeparture::CatapultDeparture(){
	static_assert(std::size(kChargeSePaths) == kChargeBeatCount,"溜めの合図の音と合図の回数が合っていない");
	for(const std::string& path : kChargeSePaths){
		SoundManager::GetInstance()->SoundLoadFile(path);
	}
	SoundManager::GetInstance()->SoundLoadFile(kLaunchSePath);
}

// 出発の演出を始める
void CatapultDeparture::Begin(const DepartureContext& context,const CameraPose& startCamera){
	context_ = context;
	startCamera_ = startCamera;
	cameraPose_ = startCamera;
	isControllingCamera_ = true;
	state_ = State::Approach;
	timer_ = 0.0f;
	chargeBeatCount_ = 0;
	isBoardedPending_ = false;

	startPosition_ = context_.player->GetPosition();
	launchPosition_ = context_.railEditor->GetPositionOnRail(kRailStartT);

	Vector3 railForward = context_.railEditor->GetForwardOnRail(kRailStartT);
	forward_ = Normalize(Vector3{railForward.x, 0.0f, railForward.z});
	right_ = {forward_.z, 0.0f, -forward_.x};
	yaw_ = std::atan2(forward_.x,forward_.z);

	// 先にレールの始点へ乗せておき、飛び出した瞬間からレールを進めるようにする
	context_.player->BoardRailStart();
	if(context_.camera){
		baseFov_ = context_.camera->GetFov();
	}
}

// 溜めている間のカメラの位置と向きを求める
CameraPose CatapultDeparture::GetChargeCameraPose(float chargeProgress) const{
	const float back = Lerp(kChargeCameraBackStart,kChargeCameraBackEnd,Easing::EaseOutCubic(chargeProgress));
	const Vector3 position = launchPosition_ - forward_ * back + right_ * kChargeCameraSide + kWorldUp * kChargeCameraHeight;
	const Vector3 target = launchPosition_ + forward_ * kChargeLookAhead + kWorldUp * kChargeLookHeight;
	return CameraPoseMath::LookAt(position,target);
}

// 演出を進める
void CatapultDeparture::Update(float deltaTime,const CameraPose& gameplayCamera){
	timer_ += deltaTime;
	const float chargeDuration = kChargeBeatInterval * static_cast<float>(kChargeBeatCount);

	switch(state_){
	case State::Approach:
	{
		// レールの始点へ山なりに跳ねて移り、カメラは低い位置から背中を映す画へ寄せる
		const float progress = StageRate(timer_,0.0f,kApproachDuration);
		Vector3 position = Lerp(startPosition_,launchPosition_,Easing::EaseOutCubic(progress));
		position.y += kApproachHopHeight * kParabolaPeakScale * progress * (1.0f - progress);
		context_.player->SetDirectedPose(position,{0.0f, yaw_, 0.0f},{1.0f, 1.0f, 1.0f});
		cameraPose_ = CameraPoseMath::Lerp(startCamera_,GetChargeCameraPose(0.0f),Easing::EaseInOutCubic(progress));
		if(progress >= 1.0f){
			state_ = State::Charge;
			timer_ = 0.0f;
		}
		break;
	}

	case State::Charge:
	{
		// 一定の間隔で合図を出し、回数を重ねるほど深く沈んで、揺れも強くする
		const int beatIndex = (std::min)(static_cast<int>(timer_ / kChargeBeatInterval),kChargeBeatCount - 1);
		if(beatIndex + 1 > chargeBeatCount_){
			chargeBeatCount_ = beatIndex + 1;
			context_.screenImpact->Trigger(0.0f,kBeatShakeAmplitude * static_cast<float>(chargeBeatCount_),kBeatShakeDuration);
			context_.inkEffect->EmitInkSplash(launchPosition_,kWorldUp,kBeatSplashCount);
			// 合図の回数に応じた音を鳴らす
			SoundManager::GetInstance()->PlayAudio(kChargeSePaths[chargeBeatCount_ - 1],StartSequence::GetSeVolume());
		}
		// 合図の直後は一瞬だけ余分に縮め、次の合図までに戻して、ばねを引き絞っているように見せる
		const float beatProgress = StageRate(timer_ - kChargeBeatInterval * static_cast<float>(beatIndex),0.0f,kChargeBeatInterval);
		const float squash = kChargeSquash * static_cast<float>(chargeBeatCount_) / static_cast<float>(kChargeBeatCount) + kBeatPulseSquash * (1.0f - Easing::EaseOutCubic(beatProgress));
		context_.player->SetDirectedPose(launchPosition_,{0.0f, yaw_, 0.0f},{1.0f + squash * kSquashWidenRate, 1.0f - squash, 1.0f + squash * kSquashWidenRate});

		const float chargeProgress = StageRate(timer_,0.0f,chargeDuration);
		cameraPose_ = GetChargeCameraPose(chargeProgress);

		if(chargeProgress >= 1.0f){
			// 飛び出す: 「GO!」を出し、レールを進む速さを上げ、衝撃と放射ブラーと視野角で勢いを見せる
			NotifyBoarded(launchPosition_);
			context_.player->StartRailBoost(kBoostRate,kBoostDuration);
			context_.screenImpact->Trigger(kLaunchHitStopDuration,kLaunchShakeAmplitude,kLaunchShakeDuration);
			Application::GetInstance()->TriggerRadialBlur(CameraPoseMath::kScreenCenterUv,kLaunchRadialBlurWidth,kLaunchRadialBlurDuration);
			SoundManager::GetInstance()->PlayAudio(kLaunchSePath,StartSequence::GetSeVolume());
			isFovChanged_ = context_.camera != nullptr;
			state_ = State::Launch;
			timer_ = 0.0f;
		}
		break;
	}

	case State::Launch:
	{
		// カメラは溜めきった画からプレイ用カメラへ素早く戻す(プレイヤーの表示はレールを進む通常の表示に任せる)
		const float cameraProgress = StageRate(timer_,0.0f,kLaunchCameraDuration);
		cameraPose_ = CameraPoseMath::Lerp(GetChargeCameraPose(1.0f),gameplayCamera,Easing::EaseOutCubic(cameraProgress));
		isControllingCamera_ = cameraProgress < 1.0f;

		// 視野角は飛び出した瞬間に広げ、少しずつ元に戻す(速さが上がって景色が流れていくように見せる)
		const float fovProgress = StageRate(timer_,0.0f,kFovReturnDuration);
		if(isFovChanged_){
			context_.camera->SetFov(baseFov_ + kFovKick * (1.0f - Easing::EaseOutCubic(fovProgress)));
		}
		if(fovProgress >= 1.0f){
			RestoreFov();
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
bool CatapultDeparture::GetCameraPose(CameraPose& outPose) const{
	if(!isControllingCamera_){
		return false;
	}
	outPose = cameraPose_;
	return true;
}

// 視野角を元に戻す
void CatapultDeparture::RestoreFov(){
	if(isFovChanged_){
		context_.camera->SetFov(baseFov_);
		isFovChanged_ = false;
	}
}

// 途中で止める(広げた視野角も元に戻す)
void CatapultDeparture::Reset(){
	RestoreFov();
	state_ = State::Finished;
	isControllingCamera_ = false;
	isBoardedPending_ = false;
}
