#include "GateCameraDirector.h"
#include "Easing.h"
#include <algorithm>

// 何もしていない状態に戻す
void GateCameraDirector::Reset(){
	state_ = State::None;
	timer_ = 0.0f;
}

// ゲートを映す画へ寄せ始める
void GateCameraDirector::BeginShot(){
	state_ = State::Shot;
	timer_ = 0.0f;
}

// プレイ用カメラへ戻し始める
void GateCameraDirector::BeginReturn(){
	if(state_ != State::Shot){
		return;
	}
	// 寄せきった画で少し止めてから戻すよう、止める時間の分だけマイナスから数え始める
	state_ = State::Return;
	timer_ = -kReturnHoldDuration;
}

// 寄せる・戻す動きを進める
void GateCameraDirector::Update(float deltaTime){
	switch(state_){
	case State::Shot:
		timer_ = (std::min)(timer_ + deltaTime,kShotInDuration);
		break;
	case State::Return:
		timer_ += deltaTime;
		// 戻りきったら、プレイ用カメラを上書きするのをやめる
		if(timer_ >= kReturnDuration){
			Reset();
		}
		break;
	case State::None:
	default:
		break;
	}
}

// ゲートを映す画を混ぜる割合
float GateCameraDirector::GetShotWeight() const{
	switch(state_){
	case State::Shot:
		return Easing::EaseInOutCubic(timer_ / kShotInDuration);
	case State::Return:
		// 止めている間(タイマーがマイナスの間)は寄せきった画のまま
		return 1.0f - Easing::EaseInOutCubic(std::clamp(timer_ / kReturnDuration,0.0f,1.0f));
	case State::None:
	default:
		return 0.0f;
	}
}

// 演出中のカメラ座標・向きを求める
// ゲートを映す画(注目する座標を向いた画)とプレイ用カメラを、割合で混ぜる
CameraPose GateCameraDirector::Calculate(const Vector3& shotPosition,const Vector3& shotTarget,const CameraPose& gameplayCamera) const{
	return CameraPoseMath::Lerp(gameplayCamera,CameraPoseMath::LookAt(shotPosition,shotTarget),GetShotWeight());
}
