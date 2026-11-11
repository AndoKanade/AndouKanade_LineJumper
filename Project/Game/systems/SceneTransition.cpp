#include "SceneTransition.h"
#include "systems/transition/ITransitionEffect.h"
#include "systems/transition/TransitionEffectUtil.h"
#include "systems/transition/FadeTransition.h"
#include "systems/transition/RailStripesTransition.h"
#include "systems/transition/FallAndLiftTransition.h"
#include "systems/transition/RetryCutInTransition.h"
#include "systems/transition/InkBurstTransition.h"
#include "systems/transition/InkDripTransition.h"
#include "systems/transition/RailPaintTransition.h"
#include "systems/transition/GoalStampTransition.h"
#include "systems/transition/CloudRiseTransition.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include <utility>

using namespace TransitionEffectUtil;

// 演出を種類ごとに1つずつ生成しておき、切り替えるたびに作り直さないようにする
// (スプライトは Initialize() で作る。演出の時間は、生成した時点から取得できる)
SceneTransition::SceneTransition(){
	effects_[static_cast<size_t>(Style::Fade)] = std::make_unique<FadeTransition>();
	effects_[static_cast<size_t>(Style::RailStripes)] = std::make_unique<RailStripesTransition>();
	effects_[static_cast<size_t>(Style::FallAndLift)] = std::make_unique<FallAndLiftTransition>();
	effects_[static_cast<size_t>(Style::RetryCutIn)] = std::make_unique<RetryCutInTransition>();
	effects_[static_cast<size_t>(Style::InkBurst)] = std::make_unique<InkBurstTransition>();
	effects_[static_cast<size_t>(Style::InkDrip)] = std::make_unique<InkDripTransition>();
	effects_[static_cast<size_t>(Style::RailPaint)] = std::make_unique<RailPaintTransition>();
	effects_[static_cast<size_t>(Style::GoalStamp)] = std::make_unique<GoalStampTransition>();
	effects_[static_cast<size_t>(Style::CloudRise)] = std::make_unique<CloudRiseTransition>();
}

SceneTransition::~SceneTransition() = default;

// 画面全体を覆うスプライトを生成する
void SceneTransition::Initialize(SpriteCommon* spriteCommon){
	spriteCommon_ = spriteCommon;

	// 画面全体を覆う黒のスプライト(左上から画面サイズに引き伸ばす)
	TextureManager::GetInstance()->LoadTexture(kFadeTexture);
	fadeSprite_ = std::make_unique<Sprite>();
	fadeSprite_->Initialize(spriteCommon_,kFadeTexture);
	fadeSprite_->SetSize({kScreenWidth, kScreenHeight});
	fadeSprite_->SetAnchorPoint(kTopLeftAnchor);
	fadeSprite_->SetPosition(kTopLeftAnchor);

	// 演出ごとのスプライトを、Style の番号順に生成する
	for(const std::unique_ptr<ITransitionEffect>& effect : effects_){
		effect->Initialize(spriteCommon_,fadeSprite_.get());
	}
}

// スプライトを解放する
void SceneTransition::Finalize(){
	fadeSprite_.reset();
	// 演出ごとのスプライトを、Style の番号順に解放する
	for(const std::unique_ptr<ITransitionEffect>& effect : effects_){
		effect->Finalize();
	}
	onCovered_ = nullptr;
	phase_ = Phase::None;
	coverRate_ = kNotCovered;
}

// 暗転を始める
bool SceneTransition::Start(std::function<void()> onCovered,Style style){
	if(IsActive()){
		return false;
	}

	onCovered_ = std::move(onCovered);
	style_ = style;
	coverRate_ = kNotCovered;
	phase_ = Phase::Covering;
	return true;
}

// 暗転を進める
void SceneTransition::Update(float deltaTime){
	switch(phase_){
	case Phase::Covering:
		coverRate_ += deltaTime / GetCurrentEffect()->GetCoverDuration();
		if(coverRate_ >= kFullyCovered){
			coverRate_ = kFullyCovered;
			phase_ = Phase::Covered;
		}
		break;

	case Phase::Covered:
		// 覆い終わったフレームで真っ暗な画面が描画された後に呼ぶため、次のフレームのここで処理する
		// 呼んだ処理の中で次の暗転が予約されても上書きしないよう、先に取り出してから呼ぶ
		if(onCovered_){
			std::function<void()> onCovered = std::move(onCovered_);
			onCovered_ = nullptr;
			onCovered();
		}
		phase_ = Phase::Revealing;
		break;

	case Phase::Revealing:
		coverRate_ -= deltaTime / GetCurrentEffect()->GetRevealDuration();
		if(coverRate_ <= kNotCovered){
			coverRate_ = kNotCovered;
			phase_ = Phase::None;
		}
		break;

	case Phase::None:
	default:
		break;
	}

	// 暗転していないとき・スプライトを解放した後は見た目を更新しない
	if(!IsActive() || !fadeSprite_){
		return;
	}

	// 今の演出の見た目だけを更新する
	GetCurrentEffect()->Update(coverRate_,phase_ == Phase::Revealing);
}

// 画面を覆うスプライトを描画する
void SceneTransition::Draw(){
	if(!IsActive() || !spriteCommon_ || !fadeSprite_){
		return;
	}

	spriteCommon_->Draw(); // 描画前処理

	// 今の演出のスプライトを、演出ごとに決めた順で重ねて描く
	GetCurrentEffect()->Draw();
}

// 今の演出の見た目を取得する
ITransitionEffect* SceneTransition::GetCurrentEffect() const{
	// 範囲外の番号(保存済みの JSON を書き換えた場合など)は、基本の暗転(Fade)にする
	const size_t index = static_cast<size_t>(style_);
	return effects_[index < effects_.size()?index:static_cast<size_t>(Style::Fade)].get();
}
