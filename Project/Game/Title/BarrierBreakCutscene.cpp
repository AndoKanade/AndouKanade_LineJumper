#include "BarrierBreakCutscene.h"
#include "Easing.h"
#include "GlobalVariables.h"
#include "objects/StartPlatform.h"
#include <algorithm>

// 初期化処理
void BarrierBreakCutscene::Initialize(const std::string& paramGroup){
	paramGroup_ = paramGroup;

	// 第3引数はデフォルト値。保存済みJSONがあればそちらが優先される(AddItemは未登録キーのみ追加)
	GlobalVariables* gv = GlobalVariables::GetInstance();
	gv->AddItem(paramGroup_,kTurnDurationKey,kDefaultTurnDuration);     // 壁を映す画へ回す時間
	gv->AddItem(paramGroup_,kBreakDurationKey,kDefaultBreakDuration);   // 壁を映したまま壊して見せる時間
	gv->AddItem(paramGroup_,kReturnDurationKey,kDefaultReturnDuration); // プレイ用カメラへ戻す時間
}

// 演出を始める
// 壁を映す画は、台の上の決まった位置から、壊す壁の中心を見る画にする
void BarrierBreakCutscene::Begin(const StartPlatform& startPlatform,int stageIndex){
	isRunning_ = true;
	timer_ = 0.0f;
	stageIndex_ = stageIndex;
	hasBroken_ = false;
	isBrokenPending_ = false;
	shotPose_ = CameraPoseMath::LookAt(startPlatform.PlatformLocalToWorld(kShotLocalPosition),startPlatform.GetBarrierCenter(stageIndex));
}

// 演出を進める
void BarrierBreakCutscene::Update(float deltaTime,bool isSkipRequested){
	if(!isRunning_){
		return;
	}

	// 飛ばしたときは、まだ壊していなければ壊したことにして、すぐプレイ用カメラへ戻す
	if(isSkipRequested){
		if(!hasBroken_){
			hasBroken_ = true;
			isBrokenPending_ = true;
		}
		isRunning_ = false;
		return;
	}

	timer_ += deltaTime;

	GlobalVariables* gv = GlobalVariables::GetInstance();
	const float turnDuration = gv->GetFloatValue(paramGroup_,kTurnDurationKey);
	const float breakDuration = gv->GetFloatValue(paramGroup_,kBreakDurationKey);
	const float returnDuration = gv->GetFloatValue(paramGroup_,kReturnDurationKey);

	// 回しきって少し間を置いたら、壁を壊す(調整項目が負の値でも、時間が戻らないよう0以上として扱う)
	const float breakTime = (std::max)(turnDuration,0.0f) + std::clamp(kBreakDelay,0.0f,(std::max)(breakDuration,0.0f));
	if(!hasBroken_ && timer_ >= breakTime){
		hasBroken_ = true;
		isBrokenPending_ = true;
	}

	// 戻しきったら終わる
	const float totalDuration = (std::max)(turnDuration,0.0f) + (std::max)(breakDuration,0.0f) + (std::max)(returnDuration,0.0f);
	if(timer_ >= totalDuration){
		isRunning_ = false;
	}
}

// 途中で止める
void BarrierBreakCutscene::Reset(){
	isRunning_ = false;
	timer_ = 0.0f;
	stageIndex_ = -1;
	hasBroken_ = false;
	isBrokenPending_ = false;
}

// 演出のカメラの位置と向きを取得する
// 壁を映す画とプレイ用カメラを割合で混ぜるため、回し始めと戻しきったときにプレイ用カメラと完全に一致する
bool BarrierBreakCutscene::GetCameraPose(const CameraPose& gameplayCamera,CameraPose& outPose) const{
	if(!isRunning_){
		return false;
	}
	outPose = CameraPoseMath::Lerp(gameplayCamera,shotPose_,GetShotWeight());
	return true;
}

// このフレームで壁が壊れたかを受け取る
bool BarrierBreakCutscene::ConsumeBroken(){
	const bool isBroken = isBrokenPending_;
	isBrokenPending_ = false;
	return isBroken;
}

// 壁を映す画を混ぜる割合
// 回す間は 0 から 1 へなめらかに上げ、壊す間は 1 のまま、戻す間は 1 から 0 へなめらかに下げる
float BarrierBreakCutscene::GetShotWeight() const{
	GlobalVariables* gv = GlobalVariables::GetInstance();
	const float turnDuration = (std::max)(gv->GetFloatValue(paramGroup_,kTurnDurationKey),0.0f);
	const float breakDuration = (std::max)(gv->GetFloatValue(paramGroup_,kBreakDurationKey),0.0f);
	const float returnDuration = (std::max)(gv->GetFloatValue(paramGroup_,kReturnDurationKey),0.0f);

	if(timer_ < turnDuration){
		return Easing::EaseInOutCubic(StageRate(timer_,turnDuration));
	}
	const float returnStartTime = turnDuration + breakDuration;
	if(timer_ < returnStartTime){
		return 1.0f;
	}
	return 1.0f - Easing::EaseInOutCubic(StageRate(timer_ - returnStartTime,returnDuration));
}

// 経過時間を、段階の進み具合(0〜1)に直す
float BarrierBreakCutscene::StageRate(float elapsed,float duration){
	if(duration <= 0.0f){
		return 1.0f;
	}
	return std::clamp(elapsed / duration,0.0f,1.0f);
}
