#include "GoalStampTransition.h"
#include "systems/transition/TransitionEffectUtil.h"
#include "TextureManager.h"
#include "Easing.h"
#include <cmath>
#include <iterator>
#include <string>

using namespace TransitionEffectUtil;

namespace{
	// GoalStamp で押す「GOAL!」の文字(色付きの画像をそのまま使う。tools/generate_transition_assets.py で生成する)
	const std::string kGoalTexture = "resource/UI/transition/goal.png";
	// GoalStamp で文字を押した衝撃で周りに飛ぶしみ(位置・大きさ・傾き・画像の番号。InkBurst と同じ並べ方)
	constexpr InkSplatLayout kStampSplashLayouts[] = {
		{{0.3f, 0.3f}, 0.5f, 1.0f, 0},
		{{0.72f, 0.28f}, 0.45f, 2.0f, 1},
		{{0.25f, 0.68f}, 0.5f, 3.0f, 2},
		{{0.75f, 0.7f}, 0.55f, 4.0f, 0},
		{{0.5f, 0.78f}, 0.4f, 5.0f, 1},
	};
	constexpr int kStampSplashCount = static_cast<int>(std::size(kStampSplashLayouts));
	// GoalStamp で中央から広がるしみに使う画像の番号
	constexpr int kStampSpreadTextureIndex = 0;
}

GoalStampTransition::GoalStampTransition() = default;
GoalStampTransition::~GoalStampTransition() = default;

// 暗くする全面・飛ぶしみ・広がるしみ・文字のスプライトを生成する
void GoalStampTransition::Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite){
	fadeSprite_ = fadeSprite;
	// 使うテクスチャを読み込む(読み込み済みのものは何もしない)
	TextureManager::GetInstance()->LoadTexture(kFadeTexture);
	for(const std::string& texture : kInkSplatTextures){
		TextureManager::GetInstance()->LoadTexture(texture);
	}

	// GoalStamp の暗くする全面・飛ぶしみ・広がるしみ・文字
	TextureManager::GetInstance()->LoadTexture(kGoalTexture);
	stampDimSprite_ = CreateBandSprite(spriteCommon,kFadeTexture);
	stampDimSprite_->SetAnchorPoint(kTopLeftAnchor);
	stampDimSprite_->SetPosition(kTopLeftAnchor);
	stampDimSprite_->SetSize({kScreenWidth, kScreenHeight});
	for(const InkSplatLayout& layout : kStampSplashLayouts){
		stampSplashSprites_.push_back(CreateBandSprite(spriteCommon,kInkSplatTextures[layout.textureIndex]));
	}
	stampSpreadSprite_ = CreateBandSprite(spriteCommon,kInkSplatTextures[kStampSpreadTextureIndex]);
	stampTextSprite_ = CreateBandSprite(spriteCommon,kGoalTexture);
	// 文字は画像の大きさ(Initialize で設定される)を基準に、押す勢いに合わせて大きさを変える
	const Vector2& goalTextureSize = stampTextSprite_->GetSize();
	stampTextSprite_->SetSize({goalTextureSize.x * kStampTextScale, goalTextureSize.y * kStampTextScale});
	stampTextBaseSize_ = stampTextSprite_->GetSize();
}

// スプライトを解放する(画面全体を覆うスプライトは SceneTransition が解放するため、手放すだけにする)
void GoalStampTransition::Finalize(){
	stampDimSprite_.reset();
	stampSplashSprites_.clear();
	stampSpreadSprite_.reset();
	stampTextSprite_.reset();
	fadeSprite_ = nullptr;
}

// GoalStamp: 画面を少し暗くし、「GOAL!」の文字を大きい状態から押しつけて揺らす。押した衝撃でインクのしみが周りに飛び、
// 中央から黄色いインクが広がって画面を覆う。戻すときは、広がったインクが縮みながら下へ落ちていく
void GoalStampTransition::Update(float coverRate,bool isRevealing){
	const Vector2 stampCenter = {kScreenCenter.x, kScreenHeight * kStampHeightRate};

	if(isRevealing){
		float progress = Easing::EaseInCubic(kFullyCovered - coverRate);
		float spreadSize = kScreenHeight * kStampSpreadScale * (kProgressMax - progress);
		Vector2 spreadCenter = {stampCenter.x, stampCenter.y + kScreenHeight * kStampSpreadFallScale * progress};
		PlaceSprite(stampSpreadSprite_.get(),spreadCenter,kStampSpreadAngle,{spreadSize, spreadSize},kInkColor);

		// 覆っていた全体の塗りはすぐに消し、広がったインクが落ちていく様子を見せる
		float fillAlpha = kProgressMax - GetRangeProgress(kFullyCovered - coverRate,kProgressMin,kStampFillRevealEnd);
		fadeSprite_->SetColor({kInkColor.x, kInkColor.y, kInkColor.z, fillAlpha});
		fadeSprite_->Update();

		// 暗くした画面・飛んだしみ・文字は戻すときには出さない
		stampDimSprite_->SetColor({kFadeColor, kFadeColor, kFadeColor, 0.0f});
		stampDimSprite_->Update();
		for(const std::unique_ptr<Sprite>& sprite : stampSplashSprites_){
			sprite->SetSize({0.0f, 0.0f});
			sprite->Update();
		}
		stampTextSprite_->SetSize({0.0f, 0.0f});
		stampTextSprite_->Update();
		return;
	}

	// 画面を少し暗くして、文字を目立たせる
	stampDimSprite_->SetColor({kFadeColor, kFadeColor, kFadeColor, kStampDimAlpha * GetRangeProgress(coverRate,kProgressMin,kStampDimEnd)});
	stampDimSprite_->Update();

	// 文字は大きい状態から加速しながら縮んで押しつけられ、押した後は左右に揺れてから止まる
	float slam = GetRangeProgress(coverRate,kProgressMin,kStampSlamEnd);
	float textScale = Lerp(kStampStartScale,kProgressMax,Easing::EaseInCubic(slam));
	float shake = 0.0f;
	if(slam >= kProgressMax){
		float shakeRemain = kProgressMax - GetRangeProgress(coverRate,kStampSlamEnd,kStampShakeEnd);
		shake = kStampShakeWidth * std::sin(coverRate * kStampShakeSpeed) * shakeRemain;
	}
	float textAlpha = GetRangeProgress(coverRate,kProgressMin,kStampTextAppearEnd);
	PlaceSprite(stampTextSprite_.get(),{stampCenter.x + shake, stampCenter.y},0.0f,
		{stampTextBaseSize_.x * textScale, stampTextBaseSize_.y * textScale},{kProgressMax, kProgressMax, kProgressMax, textAlpha});

	// 押した瞬間に、周りにしみが勢いよく飛ぶ(押す前は大きさを0にして隠す)
	float splash = slam >= kProgressMax ? Easing::EaseOutBack(GetRangeProgress(coverRate,kStampSlamEnd,kStampSplashEnd)) : 0.0f;
	for(int i = 0; i < kStampSplashCount; ++i){
		const InkSplatLayout& layout = kStampSplashLayouts[i];
		float size = kScreenHeight * layout.sizeScale * splash;
		Vector2 center = {kScreenWidth * layout.position.x, kScreenHeight * layout.position.y};
		const Vector4& color = (i % kInkColorAlternateCount == 0) ? kInkShadeColor : kInkColor;
		PlaceSprite(stampSplashSprites_[i].get(),center,layout.angle,{size, size},color);
	}

	// 文字を読ませた後で、中央から黄色いインクが加速しながら広がる
	float spreadSize = kScreenHeight * kStampSpreadScale * Easing::EaseInCubic(GetRangeProgress(coverRate,kStampSpreadStart,kFullyCovered));
	PlaceSprite(stampSpreadSprite_.get(),stampCenter,kStampSpreadAngle,{spreadSize, spreadSize},kInkColor);

	// しみの隙間に見えている画面を、覆い終わる直前に全体を塗って隠す
	fadeSprite_->SetColor({kInkColor.x, kInkColor.y, kInkColor.z, GetRangeProgress(coverRate,kStampFillStart,kFullyCovered)});
	fadeSprite_->Update();
}

// スプライトを描画する
void GoalStampTransition::Draw(){
	// 暗くした画面 → 飛んだしみ → 広がるしみ → 文字 → 全体の塗り の順に重ねる
	stampDimSprite_->Draw();
	for(const std::unique_ptr<Sprite>& sprite : stampSplashSprites_){
		sprite->Draw();
	}
	stampSpreadSprite_->Draw();
	stampTextSprite_->Draw();
	fadeSprite_->Draw();
}
