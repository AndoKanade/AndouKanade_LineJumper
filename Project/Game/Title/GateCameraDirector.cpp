#include "GateCameraDirector.h"
#include "Easing.h"
#include <algorithm>
#include <cmath>

namespace{
	// 角度を、遠回りしない向きで補間する(差を -π〜π に収めてから補間し、1周回ってしまわないようにする)
	float LerpAngle(float from,float to,float t,float fullTurn){
		return from + std::remainder(to - from,fullTurn) * t;
	}
}

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
void GateCameraDirector::Calculate(const Vector3& shotPosition,const Vector3& shotTarget,const Vector3& gameplayPosition,const Vector3& gameplayRotate,Vector3& outPosition,Vector3& outRotate) const{
	// ゲートを映す画の向きを、注目する座標への向きから求める(X回転は下向きが正、Y回転は +Z から +X へ回る向きが正)
	Vector3 direction = Normalize(shotTarget - shotPosition);
	float horizontalLength = std::sqrt(direction.x * direction.x + direction.z * direction.z);
	Vector3 shotRotate = {std::atan2(-direction.y,horizontalLength), std::atan2(direction.x,direction.z), 0.0f};

	// プレイ用カメラとゲートを映す画を、割合で混ぜる
	float weight = GetShotWeight();
	outPosition = Lerp(gameplayPosition,shotPosition,weight);
	outRotate = {
		LerpAngle(gameplayRotate.x,shotRotate.x,weight,kFullTurn),
		LerpAngle(gameplayRotate.y,shotRotate.y,weight,kFullTurn),
		LerpAngle(gameplayRotate.z,shotRotate.z,weight,kFullTurn)
	};
}
