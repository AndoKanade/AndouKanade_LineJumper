#include "FadeTransition.h"
#include "systems/transition/TransitionEffectUtil.h"
#include "Easing.h"

using namespace TransitionEffectUtil;

// 画面全体を覆うスプライトを受け取る(Fade はこのスプライトだけを使うため、自分のスプライトは作らない)
void FadeTransition::Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite){
	(void)spriteCommon;
	fadeSprite_ = fadeSprite;
}

// 受け取ったスプライトを手放す(解放は SceneTransition が行う)
void FadeTransition::Finalize(){
	fadeSprite_ = nullptr;
}

// Fade: ゆっくり動き出してゆっくり止まるように黒の濃さを変える
void FadeTransition::Update(float coverRate,bool isRevealing){
	(void)isRevealing;
	fadeSprite_->SetColor({kFadeColor, kFadeColor, kFadeColor, Easing::EaseInOutCubic(coverRate)});
	fadeSprite_->Update();
}

// 画面全体を覆うスプライトを描画する
void FadeTransition::Draw(){
	fadeSprite_->Draw();
}
