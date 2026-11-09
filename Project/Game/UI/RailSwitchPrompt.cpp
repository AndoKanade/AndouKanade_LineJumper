#include "RailSwitchPrompt.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace{
	// tools/generate_rail_switch_prompt.py で生成した画像(左: A + SPACE、右: D + SPACE)
	const std::string kPromptTextures[] = {
		"resource/UI/railSwitchLeft.png",
		"resource/UI/railSwitchRight.png",
	};
	// 白(画像の色をそのまま出す)。α値だけ変えて使う
	constexpr Vector4 kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
}

RailSwitchPrompt::RailSwitchPrompt() = default;
RailSwitchPrompt::~RailSwitchPrompt() = default;

// テクスチャの読み込みとスプライトの生成
// 位置・濃さは毎フレームのUpdate()で決めるため、ここでは生成と大きさ・基準点の設定だけ行う
void RailSwitchPrompt::Initialize(SpriteCommon* spriteCommon){
	static_assert(std::size(kPromptTextures) == kSideCount,"案内の画像の数と左右の数が合っていない");
	spriteCommon_ = spriteCommon;

	for(int i = 0; i < kSideCount; ++i){
		TextureManager::GetInstance()->LoadTexture(kPromptTextures[i]);
		sprites_[i] = std::make_unique<Sprite>();
		sprites_[i]->Initialize(spriteCommon,kPromptTextures[i]);
		sprites_[i]->SetSize(kSize);
		sprites_[i]->SetAnchorPoint(kCenterAnchor);
	}
}

// 何も表示していない状態に戻す
void RailSwitchPrompt::Reset(){
	alpha_ = 0.0f;
	blinkTimer_ = 0.0f;
}

// 表示の動きを進める
// 飛び移れるレールがあればその側の案内を濃くし、無ければ最後に出していた側の案内を薄くして消す
void RailSwitchPrompt::Update(int side,float deltaTime){
	if(side != 0){
		const int index = (side > 0)?kRightIndex:kLeftIndex;
		// 反対の側に変わったときは、薄い所から出し直して、どちらに出たか分かるようにする
		if(index != shownIndex_){
			shownIndex_ = index;
			alpha_ = 0.0f;
		}
		alpha_ = (std::min)(alpha_ + deltaTime / kFadeInDuration,1.0f);
	} else{
		alpha_ = (std::max)(alpha_ - deltaTime / kFadeOutDuration,0.0f);
	}
	if(alpha_ <= 0.0f){
		blinkTimer_ = 0.0f;
		return;
	}
	blinkTimer_ += deltaTime;

	// 点滅させながら、飛び移る向きへ揺らす(左の案内は左へ、右の案内は右へ)
	const float wave = (std::sin(blinkTimer_ * kBlinkSpeed) + 1.0f) * kWaveToRateScale;
	const float blinkAlpha = kBlinkMinAlpha + (1.0f - kBlinkMinAlpha) * wave;
	const float direction = (shownIndex_ == kRightIndex)?1.0f:-1.0f;
	const float screenWidth = static_cast<float>(WinAPI::kClientWidth);
	const float screenHeight = static_cast<float>(WinAPI::kClientHeight);
	const float centerX = (shownIndex_ == kRightIndex)?screenWidth * (1.0f - kSideCenterXRate):screenWidth * kSideCenterXRate;

	Sprite& sprite = *sprites_[shownIndex_];
	sprite.SetPosition({centerX + direction * kNudgeDistance * wave, screenHeight * kHeightRate});
	sprite.SetColor({kWhite.x, kWhite.y, kWhite.z, alpha_ * blinkAlpha});
	sprite.Update();
}

// 描画処理(表示していないときは何もしない)
void RailSwitchPrompt::Draw(){
	if(!spriteCommon_ || alpha_ <= 0.0f){
		return;
	}
	spriteCommon_->Draw(); // 描画前処理
	sprites_[shownIndex_]->Draw();
}
