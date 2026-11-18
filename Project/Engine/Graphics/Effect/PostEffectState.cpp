#include "PostEffectState.h"

// -------------------------------------------------
// 指定した種類をかけ続ける状態
// -------------------------------------------------

// 時間では終わらないため、ほかの状態へ切り替えられるまでこの状態を続ける
std::unique_ptr<PostEffectState> SelectedPostEffectState::Update(PostProcess* postProcess,float deltaTime){
	(void)postProcess;
	(void)deltaTime;
	return nullptr;
}

// -------------------------------------------------
// グリッチ
// -------------------------------------------------

// 残り時間を減らし、なくなったら通常の表示へ戻る
std::unique_ptr<PostEffectState> GlitchPostEffectState::Update(PostProcess* postProcess,float deltaTime){
	(void)postProcess;
	remainTime_ -= deltaTime;
	if(remainTime_ <= 0.0f){
		return std::make_unique<SelectedPostEffectState>(PostProcess::Type::PostProcess);
	}
	return nullptr;
}

// -------------------------------------------------
// 放射ブラー
// -------------------------------------------------

// かけ始めの中心と強さを書き込む
void RadialBlurPostEffectState::Enter(PostProcess* postProcess){
	postProcess->SetRadialBlurCenter(center_);
	postProcess->SetRadialBlurWidth(startWidth_);
}

// 残り時間の割合の2乗で強さを下げ、かけた瞬間に強く、素早く消えるようにする
// 消えきったら通常の表示へ戻る
std::unique_ptr<PostEffectState> RadialBlurPostEffectState::Update(PostProcess* postProcess,float deltaTime){
	elapsedTime_ += deltaTime;
	const float progress = elapsedTime_ / duration_;
	if(progress >= 1.0f){
		return std::make_unique<SelectedPostEffectState>(PostProcess::Type::PostProcess);
	}

	const float remainRate = 1.0f - progress;
	postProcess->SetRadialBlurWidth(startWidth_ * remainRate * remainRate);
	return nullptr;
}

// -------------------------------------------------
// ディゾルブ
// -------------------------------------------------

// 経過時間の割合をそのまま閾値にし、消しきったら消えた画面のままディゾルブをかけ続ける状態へ移る
std::unique_ptr<PostEffectState> DissolvePostEffectState::Update(PostProcess* postProcess,float deltaTime){
	elapsedTime_ += deltaTime;
	threshold_ = elapsedTime_ / kDuration;

	const bool isFinished = threshold_ >= kMaxThreshold;
	if(isFinished){
		threshold_ = kMaxThreshold;
	}
	postProcess->SetDissolveThreshold(threshold_);

	if(isFinished){
		return std::make_unique<SelectedPostEffectState>(PostProcess::Type::Dissolve);
	}
	return nullptr;
}
