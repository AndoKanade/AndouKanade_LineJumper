#include "SceneTransition.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include "Easing.h"
#include <string>
#include <utility>

namespace{
	// 画面を覆う白一色のテクスチャ(色を掛けて黒にする)
	// 読み込み時にミップマップを生成するため、1x1 では失敗する。縮小できる大きさにしている
	const std::string kFadeTexture = "resource/white16x16.png";
	// スプライトを左上基準で配置するためのアンカー
	constexpr Vector2 kTopLeftAnchor = {0.0f, 0.0f};
	// 画面を覆う色(黒)
	constexpr float kFadeColor = 0.0f;
}

SceneTransition::SceneTransition() = default;
SceneTransition::~SceneTransition() = default;

// 画面全体を覆うスプライトを生成する
void SceneTransition::Initialize(SpriteCommon* spriteCommon){
	spriteCommon_ = spriteCommon;

	// 画面全体を覆う黒のスプライト(左上から画面サイズに引き伸ばす)
	TextureManager::GetInstance()->LoadTexture(kFadeTexture);
	fadeSprite_ = std::make_unique<Sprite>();
	fadeSprite_->Initialize(spriteCommon_,kFadeTexture);
	fadeSprite_->SetSize({static_cast<float>(WinAPI::kClientWidth), static_cast<float>(WinAPI::kClientHeight)});
	fadeSprite_->SetAnchorPoint(kTopLeftAnchor);
	fadeSprite_->SetPosition(kTopLeftAnchor);
}

// スプライトを解放する
void SceneTransition::Finalize(){
	fadeSprite_.reset();
	onCovered_ = nullptr;
	phase_ = Phase::None;
	coverRate_ = kNotCovered;
}

// 暗転を始める
bool SceneTransition::Start(std::function<void()> onCovered){
	if(IsActive()){
		return false;
	}

	onCovered_ = std::move(onCovered);
	coverRate_ = kNotCovered;
	phase_ = Phase::Covering;
	return true;
}

// 暗転を進める
void SceneTransition::Update(float deltaTime){
	switch(phase_){
	case Phase::Covering:
		coverRate_ += deltaTime / kCoverDuration;
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
		coverRate_ -= deltaTime / kRevealDuration;
		if(coverRate_ <= kNotCovered){
			coverRate_ = kNotCovered;
			phase_ = Phase::None;
		}
		break;

	case Phase::None:
	default:
		break;
	}

	// ゆっくり動き出してゆっくり止まるように黒の濃さを変える
	if(fadeSprite_ && IsActive()){
		fadeSprite_->SetColor({kFadeColor, kFadeColor, kFadeColor, Easing::EaseInOutCubic(coverRate_)});
		fadeSprite_->Update();
	}
}

// 画面を覆うスプライトを描画する
void SceneTransition::Draw(){
	if(!IsActive() || !spriteCommon_ || !fadeSprite_){
		return;
	}

	spriteCommon_->Draw(); // 描画前処理
	fadeSprite_->Draw();
}
