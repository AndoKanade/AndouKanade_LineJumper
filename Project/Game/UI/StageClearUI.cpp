#include "StageClearUI.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include "Easing.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace{
	// tools/generate_stage_clear_assets.py で生成した画像
	const std::string kStageClearTexture = "resource/UI/stageClear.png";
	// 白(画像の色をそのまま出す)。α値だけ変えて使う
	constexpr Vector4 kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
}

StageClearUI::StageClearUI() = default;
StageClearUI::~StageClearUI() = default;

// テクスチャの読み込みとスプライトの生成
// 位置・大きさ・角度・濃さは毎フレームのUpdate()で決めるため、ここでは生成と基準点の設定だけ行う
void StageClearUI::Initialize(SpriteCommon* spriteCommon){
	spriteCommon_ = spriteCommon;

	TextureManager::GetInstance()->LoadTexture(kStageClearTexture);
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(spriteCommon,kStageClearTexture);
	sprite_->SetSize(kSize);
	sprite_->SetAnchorPoint(kCenterAnchor);
}

// 何も表示していない状態に戻す
void StageClearUI::Reset(){
	timer_ = 0.0f;
	isActive_ = false;
}

// 「STAGE CLEAR!」を出す
void StageClearUI::Begin(){
	timer_ = 0.0f;
	isActive_ = true;
}

// 表示の動きを進める
void StageClearUI::Update(float deltaTime){
	if(!isActive_){
		return;
	}
	timer_ += deltaTime;

	const float fadeStartTime = kStampDuration + kWobbleDuration + kHoldTime;
	if(timer_ >= fadeStartTime + kFadeDuration){
		isActive_ = false;
		return;
	}

	float scale = 1.0f;
	float alpha = 1.0f;
	float angle = 0.0f;
	float rise = 0.0f;
	if(timer_ < kStampDuration){
		// 大きい状態から押しつけるように縮んで収まる(押しつけている間に濃くなる)
		const float progress = timer_ / kStampDuration;
		scale = Lerp(kStampStartScale,1.0f,Easing::EaseInCubic(progress));
		alpha = progress;
	} else if(timer_ < kStampDuration + kWobbleDuration){
		// 押しつけた衝撃で左右に揺れ、だんだん小さくなって止まる
		const float wobbleTime = timer_ - kStampDuration;
		const float damping = 1.0f - wobbleTime / kWobbleDuration;
		angle = std::sin(wobbleTime * kWobbleSpeed) * kWobbleAngle * damping;
	} else if(timer_ >= fadeStartTime){
		// 上へ抜けながら薄くなって消える
		const float fadeProgress = std::clamp((timer_ - fadeStartTime) / kFadeDuration,0.0f,1.0f);
		rise = kFadeRiseDistance * Easing::EaseInCubic(fadeProgress);
		alpha = 1.0f - fadeProgress;
	}

	const float screenWidth = static_cast<float>(WinAPI::kClientWidth);
	const float screenHeight = static_cast<float>(WinAPI::kClientHeight);
	sprite_->SetPosition({screenWidth * kCenterXRate, screenHeight * kHeightRate - rise});
	sprite_->SetSize({kSize.x * scale, kSize.y * scale});
	sprite_->SetRotation(angle);
	sprite_->SetColor({kWhite.x, kWhite.y, kWhite.z, alpha});
	sprite_->Update();
}

// 描画処理(表示していないときは何もしない)
void StageClearUI::Draw(){
	if(!spriteCommon_ || !isActive_){
		return;
	}
	spriteCommon_->Draw(); // 描画前処理
	sprite_->Draw();
}
