#include "StageLockedNotice.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include "Easing.h"
#include <algorithm>
#include <string>

namespace{
	// tools/generate_stage_locked_notice.py で生成した画像
	const std::string kStageLockedTexture = "resource/UI/stageLocked.png";
	// 白(画像の色をそのまま出す)。α値だけ変えて使う
	constexpr Vector4 kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
}

StageLockedNotice::StageLockedNotice() = default;
StageLockedNotice::~StageLockedNotice() = default;

// テクスチャの読み込みとスプライトの生成
// 位置・大きさ・濃さは毎フレームのUpdate()で決めるため、ここでは生成と基準点の設定だけ行う
void StageLockedNotice::Initialize(SpriteCommon* spriteCommon){
	spriteCommon_ = spriteCommon;

	TextureManager::GetInstance()->LoadTexture(kStageLockedTexture);
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(spriteCommon,kStageLockedTexture);
	sprite_->SetSize(kSize);
	sprite_->SetAnchorPoint(kCenterAnchor);
}

// 何も表示していない状態に戻す
void StageLockedNotice::Reset(){
	popTimer_ = 0.0f;
	alpha_ = 0.0f;
	wasShown_ = false;
}

// 表示の動きを進める
// 出し始めた瞬間は大きい状態から弾むように縮め、出している間はすぐ濃くし、離れたら少しかけて薄くする
void StageLockedNotice::Update(bool isShown,float deltaTime){
	if(isShown && !wasShown_){
		popTimer_ = 0.0f;
	}
	wasShown_ = isShown;

	if(isShown){
		popTimer_ += deltaTime;
		alpha_ = (std::min)(alpha_ + deltaTime / kFadeInDuration,1.0f);
	} else{
		alpha_ = (std::max)(alpha_ - deltaTime / kFadeOutDuration,0.0f);
	}
	if(alpha_ <= 0.0f){
		return;
	}

	const float popProgress = std::clamp(popTimer_ / kPopDuration,0.0f,1.0f);
	const float scale = Lerp(kPopStartScale,1.0f,Easing::EaseOutBack(popProgress));
	const float screenWidth = static_cast<float>(WinAPI::kClientWidth);
	const float screenHeight = static_cast<float>(WinAPI::kClientHeight);
	sprite_->SetPosition({screenWidth * kCenterXRate, screenHeight * kHeightRate});
	sprite_->SetSize({kSize.x * scale, kSize.y * scale});
	sprite_->SetColor({kWhite.x, kWhite.y, kWhite.z, alpha_});
	sprite_->Update();
}

// 描画処理(表示していないときは何もしない)
void StageLockedNotice::Draw(){
	if(!spriteCommon_ || alpha_ <= 0.0f){
		return;
	}
	spriteCommon_->Draw(); // 描画前処理
	sprite_->Draw();
}
