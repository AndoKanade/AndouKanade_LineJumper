#include "StageIntroUI.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include "Easing.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace{
	// tools/generate_start_stage_assets.py で生成した画像
	// 操作案内(SPACE 出発する)
	const std::string kPromptTexture = "resource/UI/startPrompt.png";
	// GO!
	const std::string kGoTexture = "resource/UI/go.png";
	// 目標の文言
	const std::string kMissionTexture = "resource/UI/missionText.png";

	// 白(画像の色をそのまま出す)。α値だけ変えて使う
	constexpr Vector4 kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
	// 画面の横方向の中心(画面幅に対する割合)
	constexpr float kScreenCenterRate = 0.5f;

	// 白にα値を付けた色を返す
	Vector4 WhiteWithAlpha(float alpha){
		return {kWhite.x, kWhite.y, kWhite.z, alpha};
	}
}

StageIntroUI::StageIntroUI() = default;
StageIntroUI::~StageIntroUI() = default;

// テクスチャの読み込みとスプライトの生成
void StageIntroUI::Initialize(SpriteCommon* spriteCommon){
	spriteCommon_ = spriteCommon;

	// 位置・大きさ・濃さは毎フレームのUpdate()で決めるため、ここでは生成と基準点の設定だけ行う
	auto createSprite = [spriteCommon](const std::string& texture,const Vector2& size){
		TextureManager::GetInstance()->LoadTexture(texture);
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon,texture);
		sprite->SetSize(size);
		sprite->SetAnchorPoint(kCenterAnchor);
		return sprite;
	};

	promptSprite_ = createSprite(kPromptTexture,kPromptSize);
	goSprite_ = createSprite(kGoTexture,kGoSize);
	missionSprite_ = createSprite(kMissionTexture,kMissionSize);
}

// 何も表示していない状態に戻す
void StageIntroUI::Reset(){
	isPromptVisible_ = false;
	promptAlpha_ = 0.0f;
	promptTime_ = 0.0f;
	goTimer_ = 0.0f;
	isGoActive_ = false;
}

// 「GO!」を出し、続けて目標の文言を出す
void StageIntroUI::BeginGo(){
	goTimer_ = 0.0f;
	isGoActive_ = true;
}

// 表示の動きを進める
void StageIntroUI::Update(float deltaTime){
	const float screenWidth = static_cast<float>(WinAPI::kClientWidth);
	const float screenHeight = static_cast<float>(WinAPI::kClientHeight);
	const float centerX = screenWidth * kScreenCenterRate;

	// 操作案内は出す・消すの切り替えに合わせてなめらかに濃さを変え、出ている間は小さく上下に揺らす
	promptTime_ += deltaTime;
	float promptTarget = isPromptVisible_?1.0f:0.0f;
	promptAlpha_ += (promptTarget - promptAlpha_) * (1.0f - std::exp(-kPromptFadeSharpness * deltaTime));
	promptSprite_->SetPosition({centerX, screenHeight * kPromptHeightRate + kPromptBobHeight * std::sin(promptTime_ * kPromptBobSpeed)});
	promptSprite_->SetColor(WhiteWithAlpha(promptAlpha_));
	promptSprite_->Update();

	if(!isGoActive_){
		return;
	}
	goTimer_ += deltaTime;

	// GO! は大きい状態から飛び出すように縮んで収まり、少し表示した後に広がりながら消える
	float goScale = 1.0f;
	float goAlpha = 1.0f;
	const float goFadeStartTime = kGoPopDuration + kGoHoldTime;
	if(goTimer_ < kGoPopDuration){
		float popProgress = goTimer_ / kGoPopDuration;
		goScale = Lerp(kGoPopStartScale,1.0f,Easing::EaseOutBack(popProgress));
		goAlpha = popProgress;
	} else if(goTimer_ >= goFadeStartTime){
		float fadeProgress = std::clamp((goTimer_ - goFadeStartTime) / kGoFadeDuration,0.0f,1.0f);
		goScale = Lerp(1.0f,kGoFadeEndScale,Easing::EaseOutCubic(fadeProgress));
		goAlpha = 1.0f - fadeProgress;
	}
	goSprite_->SetPosition({centerX, screenHeight * kGoHeightRate});
	goSprite_->SetSize({kGoSize.x * goScale, kGoSize.y * goScale});
	goSprite_->SetColor(WhiteWithAlpha(goAlpha));
	goSprite_->Update();

	// 目標の文言は GO! が消え始めると同時に、少し上から下りてきて表示し、しばらくしたら消える
	float missionTime = goTimer_ - kMissionStartTime;
	float missionAlpha = 0.0f;
	float missionDrop = 0.0f;
	if(missionTime >= 0.0f){
		float appearProgress = std::clamp(missionTime / kMissionAppearDuration,0.0f,1.0f);
		float fadeProgress = std::clamp((missionTime - kMissionAppearDuration - kMissionHoldTime) / kMissionFadeDuration,0.0f,1.0f);
		missionAlpha = appearProgress * (1.0f - fadeProgress);
		missionDrop = kMissionDropDistance * (1.0f - Easing::EaseOutCubic(appearProgress));
	}
	missionSprite_->SetPosition({centerX, screenHeight * kMissionHeightRate - missionDrop});
	missionSprite_->SetColor(WhiteWithAlpha(missionAlpha));
	missionSprite_->Update();

	// 目標の文言を消し終えたら、すべての表示が終わる
	if(missionTime >= kMissionAppearDuration + kMissionHoldTime + kMissionFadeDuration){
		isGoActive_ = false;
	}
}

// 描画処理(見えていないものは描かない)
void StageIntroUI::Draw(){
	if(!spriteCommon_){
		return;
	}

	spriteCommon_->Draw(); // 描画前処理

	if(promptAlpha_ > kPromptMinVisibleAlpha){
		promptSprite_->Draw();
	}
	if(isGoActive_){
		missionSprite_->Draw();
		goSprite_->Draw();
	}
}
