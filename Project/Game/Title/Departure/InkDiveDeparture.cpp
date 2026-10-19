#include "InkDiveDeparture.h"
#include "Easing.h"
#include "SceneManager.h"
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
	// プレイヤーを隠すときの大きさの倍率
	constexpr Vector3 kHiddenScale = {0.0f, 0.0f, 0.0f};
}

// 出発ゲートが開いた瞬間に、出発台にインク溜まりを塗っておく(ここへ飛び込めばよいことを伝える)
void InkDiveDeparture::Prepare(const DepartureContext& context){
	context.startPlatform->PaintInk(context.startPlatform->GetDeparturePadPosition(),kPoolRadius);
}

// 出発の演出を始める
void InkDiveDeparture::Begin(const DepartureContext& context,const CameraPose& startCamera){
	context_ = context;
	startCamera_ = startCamera;
	cameraPose_ = startCamera;
	isControllingCamera_ = true;
	state_ = State::Dive;
	timer_ = 0.0f;
	isBoardedPending_ = false;

	startPosition_ = context_.player->GetPosition();
	poolPosition_ = context_.startPlatform->GetDeparturePadPosition();
	landPosition_ = context_.railEditor->GetPositionOnRail(kRailStartT);

	Vector3 railForward = context_.railEditor->GetForwardOnRail(kRailStartT);
	forward_ = Normalize(Vector3{railForward.x, 0.0f, railForward.z});
	yaw_ = std::atan2(forward_.x,forward_.z);
}

// 画面が覆われた瞬間の処理
// 途中で止められた後に切り替えの演出から呼ばれても、プレイヤーを動かさないようにする
void InkDiveDeparture::OnCovered(){
	if(state_ != State::Covering){
		return;
	}
	context_.player->BoardRailStart();
	// 覆いが晴れたときにはプレイ用カメラ(レールの始点の画)が映っているよう、カメラを演出側で動かすのをやめる
	isControllingCamera_ = false;
	state_ = State::Hidden;
	timer_ = 0.0f;
}

// 演出を進める
void InkDiveDeparture::Update(float deltaTime,const CameraPose& gameplayCamera){
	(void)gameplayCamera;
	timer_ += deltaTime;

	switch(state_){
	case State::Dive:
	{
		// インク溜まりへ山なりに跳ね、頭から前へ倒れ込みながら、後半で縦に縮んでインクへ沈む
		const float progress = StageRate(timer_,0.0f,kDiveDuration);
		const float sinkRate = StageRate(progress,kSinkStartRate,1.0f);
		Vector3 position = Lerp(startPosition_,poolPosition_,progress);
		position.y += kDiveHopHeight * kParabolaPeakScale * progress * (1.0f - progress) - kSinkDepth * sinkRate;
		const float widen = 1.0f + sinkRate * kSinkWidenRate;
		context_.player->SetDirectedPose(position,{kDivePitch * Easing::EaseInCubic(progress), yaw_, 0.0f},{widen, 1.0f - sinkRate, widen});

		// カメラは始めた瞬間の画から、インク溜まりを後ろ上から見下ろす画へ寄せる
		const CameraPose divePose = CameraPoseMath::LookAt(poolPosition_ - forward_ * kDiveCameraBack + kWorldUp * kDiveCameraHeight,poolPosition_);
		cameraPose_ = CameraPoseMath::Lerp(startCamera_,divePose,Easing::EaseInOutCubic(progress));

		if(progress >= 1.0f){
			// 沈みきったら、しぶきを跳ねさせて画面をインクで覆い始める
			context_.inkEffect->EmitInkSplash(poolPosition_,kWorldUp,kDiveSplashCount);
			context_.screenImpact->Trigger(0.0f,kDiveShakeAmplitude,kDiveShakeDuration);
			state_ = State::Covering;
			timer_ = 0.0f;
			// 別の切り替えの演出の途中で始められなかったときは、その場でレールの始点へ移す
			if(!context_.sceneManager->StartTransition([this](){ OnCovered(); },kTransitionStyle)){
				OnCovered();
			}
		}
		break;
	}

	case State::Covering:
		// 覆われるまでは沈んだまま隠しておく
		context_.player->SetDirectedPose(poolPosition_,{0.0f, yaw_, 0.0f},kHiddenScale);
		break;

	case State::Hidden:
		// 覆いが晴れきるまではレールの始点で隠しておき、晴れたらインクから飛び出させて「GO!」を出す
		context_.player->SetDirectedPose(landPosition_,{0.0f, yaw_, 0.0f},kHiddenScale);
		if(!context_.sceneManager->IsTransitioning()){
			NotifyBoarded(landPosition_);
			context_.inkEffect->EmitInkSplash(landPosition_,kWorldUp,kDiveSplashCount);
			state_ = State::PopOut;
			timer_ = 0.0f;
		}
		break;

	case State::PopOut:
	{
		// 0 の大きさから少し行き過ぎて元の大きさに収まり、山なりに跳ねて飛び出したように見せる
		const float progress = StageRate(timer_,0.0f,kPopDuration);
		const float scale = Easing::EaseOutBack(progress);
		Vector3 position = landPosition_;
		position.y += kPopHeight * kParabolaPeakScale * progress * (1.0f - progress);
		context_.player->SetDirectedPose(position,{0.0f, yaw_, 0.0f},{scale, scale, scale});
		if(progress >= 1.0f){
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
bool InkDiveDeparture::GetCameraPose(CameraPose& outPose) const{
	if(!isControllingCamera_){
		return false;
	}
	outPose = cameraPose_;
	return true;
}

// 途中で止める
void InkDiveDeparture::Reset(){
	state_ = State::Finished;
	isControllingCamera_ = false;
	isBoardedPending_ = false;
}
