#include "CountdownFlipDeparture.h"
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

// 出発の演出を始める(ゲートを開け始めた瞬間に呼ばれる)
void CountdownFlipDeparture::Begin(const DepartureContext& context,const CameraPose& startCamera){
	context_ = context;
	startCamera_ = startCamera;
	cameraPose_ = startCamera;
	isControllingCamera_ = true;
	state_ = State::Countdown;
	timer_ = 0.0f;
	beatCount_ = 0;
	beatTimer_ = 0.0f;
	lastCountdownNumber_ = 0;
	cameraPushRate_ = 0.0f;
	isBoardedPending_ = false;

	startPosition_ = context_.player->GetPosition();
	landPosition_ = context_.railEditor->GetPositionOnRail(kRailStartT);

	// 向きはレールの始点の進行方向(上下の成分を除く)にそろえる
	Vector3 railForward = context_.railEditor->GetForwardOnRail(kRailStartT);
	forward_ = Normalize(Vector3{railForward.x, 0.0f, railForward.z});
	right_ = {forward_.z, 0.0f, -forward_.x};
	yaw_ = std::atan2(forward_.x,forward_.z);

	if(context_.camera){
		baseFov_ = context_.camera->GetFov();
	}
}

// 溜めている間のカメラの位置と向きを求める
CameraPose CountdownFlipDeparture::GetCountdownCameraPose(float pushRate) const{
	const float back = Lerp(kCountdownCameraBackStart,kCountdownCameraBackEnd,pushRate);
	const Vector3 position = startPosition_ - forward_ * back + right_ * kCountdownCameraSide + kWorldUp * kCountdownCameraHeight;
	return CameraPoseMath::LookAt(position,GetCountdownLookTarget());
}

// 溜めている間のカメラが注目する点
Vector3 CountdownFlipDeparture::GetCountdownLookTarget() const{
	return startPosition_ + forward_ * kCountdownLookAhead + kWorldUp * kCountdownLookHeight;
}

// 演出を進める
void CountdownFlipDeparture::Update(float deltaTime,const CameraPose& gameplayCamera){
	timer_ += deltaTime;

	switch(state_){
	case State::Countdown:
	{
		// ゲートのカウントダウンの数が変わった瞬間(ランプが1段点いた瞬間)を合図にし、
		// 回数を重ねるほど深く沈んで、揺れも強くする
		const int countdownNumber = context_.startPlatform->GetCountdownNumber();
		if(countdownNumber > 0 && countdownNumber != lastCountdownNumber_){
			beatCount_ = StartPlatform::GetCountdownCount() + 1 - countdownNumber;
			beatTimer_ = 0.0f;
			context_.screenImpact->Trigger(0.0f,kBeatShakeAmplitude * static_cast<float>(beatCount_),kBeatShakeDuration);
			context_.inkEffect->EmitInkSplash(startPosition_,kWorldUp,kBeatSplashCount);
		}
		lastCountdownNumber_ = countdownNumber;
		beatTimer_ += deltaTime;

		// 合図の直後は一瞬だけ余分に縮め、次の合図までに戻して、ばねを引き絞っているように見せる
		const float chargeRate = static_cast<float>(beatCount_) / static_cast<float>(StartPlatform::GetCountdownCount());
		const float pulseRate = beatCount_ > 0?1.0f - Easing::EaseOutCubic(StageRate(beatTimer_,0.0f,kBeatPulseDuration)):0.0f;
		const float squash = kChargeSquash * chargeRate + kBeatPulseSquash * pulseRate;
		context_.player->SetDirectedPose(startPosition_,{0.0f, yaw_, 0.0f},{1.0f + squash * kSquashWidenRate, 1.0f - squash, 1.0f + squash * kSquashWidenRate});

		// カメラは始めた瞬間の画から、背中を低い位置から映す画へ移し、合図のたびに背中へ寄せる
		cameraPushRate_ += (chargeRate - cameraPushRate_) * (1.0f - std::exp(-kCameraPushSharpness * deltaTime));
		cameraPose_ = CameraPoseMath::Lerp(startCamera_,GetCountdownCameraPose(cameraPushRate_),Easing::EaseInOutCubic(StageRate(timer_,0.0f,kCameraBlendDuration)));

		// 0 の合図(遮断棒が跳ね上がり始めた瞬間)に跳ぶ
		// 跳ぶ瞬間にレールの始点へ乗せておき、プレイ用カメラが飛び乗った後の画へ動き始め、最後にその画へ戻せるようにする
		if(context_.startPlatform->HasBarStarted()){
			context_.player->BoardRailStart();
			state_ = State::Flip;
			timer_ = 0.0f;
		}
		break;
	}

	case State::Flip:
	{
		// 足元は溜める位置からレールの始点へ山なりに進める
		const float progress = StageRate(timer_,0.0f,kFlipDuration);
		const Vector3 feetPosition = Lerp(startPosition_,landPosition_,progress) + kWorldUp * (kFlipJumpHeight * kParabolaPeakScale * progress * (1.0f - progress));

		// 前へ1回転する(X 回転を正にすると頭が前へ倒れる)。踏み切りと着地の前後はゆっくり、真ん中で素早く回す
		// モデルの原点は足元にあるため、体の中心を軸に回るよう、回した体の上方向に合わせて原点の位置をずらす
		const float flipAngle = CameraPoseMath::kFullTurn * Easing::EaseInOutCubic(progress);
		const Vector3 bodyUp = kWorldUp * std::cos(flipAngle) + forward_ * std::sin(flipAngle);
		const Vector3 pivotPosition = feetPosition + kWorldUp * kFlipPivotHeight;
		context_.player->SetDirectedPose(pivotPosition - bodyUp * kFlipPivotHeight,{flipAngle, yaw_, 0.0f},kNormalScale);

		// カメラは前半は溜めていた位置に残したまま、注目する点を体へ移して宙返りを追いかけ、
		// 後半で加速しながらプレイ用カメラ(飛び乗った後の画)へ追いつかせる
		const Vector3 lookTarget = Lerp(GetCountdownLookTarget(),pivotPosition,Easing::EaseOutCubic(StageRate(progress,0.0f,kFlipLookTurnEnd)));
		const CameraPose watchPose = CameraPoseMath::LookAt(GetCountdownCameraPose(cameraPushRate_).position,lookTarget);
		cameraPose_ = CameraPoseMath::Lerp(watchPose,gameplayCamera,Easing::EaseInCubic(progress));

		if(progress >= 1.0f){
			// 飛び乗る: 表示を通常に戻して「GO!」を出し、レールを進む速さを上げ、衝撃と放射ブラーと視野角で勢いを見せる
			context_.player->SetDirectedPose(landPosition_,{0.0f, yaw_, 0.0f},kNormalScale);
			NotifyBoarded(landPosition_);
			context_.player->StartRailBoost(kBoostRate,kBoostDuration);
			context_.screenImpact->Trigger(kLandHitStopDuration,kLandShakeAmplitude,kLandShakeDuration);
			Application::GetInstance()->TriggerRadialBlur(CameraPoseMath::kScreenCenterUv,kLandRadialBlurWidth,kLandRadialBlurDuration);
			isFovChanged_ = context_.camera != nullptr;
			isControllingCamera_ = false;
			state_ = State::Launch;
			timer_ = 0.0f;
		}
		break;
	}

	case State::Launch:
	{
		// 視野角は飛び乗った瞬間に広げ、少しずつ元に戻す(速さが上がって景色が流れていくように見せる)
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
bool CountdownFlipDeparture::GetCameraPose(CameraPose& outPose) const{
	if(!isControllingCamera_){
		return false;
	}
	outPose = cameraPose_;
	return true;
}

// 視野角を元に戻す
void CountdownFlipDeparture::RestoreFov(){
	if(isFovChanged_){
		context_.camera->SetFov(baseFov_);
		isFovChanged_ = false;
	}
}

// 途中で止める(広げた視野角も元に戻す)
void CountdownFlipDeparture::Reset(){
	RestoreFov();
	state_ = State::Finished;
	isControllingCamera_ = false;
	isBoardedPending_ = false;
}
