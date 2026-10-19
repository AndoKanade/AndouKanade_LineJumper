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
	// カウントダウンの数字(1・2・3 の順)
	const std::string kCountTextures[] = {"resource/UI/count1.png", "resource/UI/count2.png", "resource/UI/count3.png"};

	// 白(画像の色をそのまま出す)。α値だけ変えて使う
	constexpr Vector4 kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
	// 画面の横方向の中心(画面幅に対する割合)
	constexpr float kScreenCenterRate = 0.5f;

	// 白にα値を付けた色を返す
	Vector4 WhiteWithAlpha(float alpha){
		return {kWhite.x, kWhite.y, kWhite.z, alpha};
	}

	/// <summary>
	/// 大きい状態から飛び出すように縮んで収まり、少し表示した後に大きさを変えながら消える表示の、大きさの倍率と濃さを求める
	/// </summary>
	/// <param name="timer">出してからの経過時間(秒)</param>
	/// <param name="popDuration">飛び出してくる時間(秒)</param>
	/// <param name="popStartScale">飛び出し始めの大きさの倍率</param>
	/// <param name="holdTime">飛び出してから消え始めるまでの時間(秒)</param>
	/// <param name="fadeDuration">消える時間(秒)</param>
	/// <param name="fadeEndScale">消え終わりの大きさの倍率</param>
	/// <param name="outScale">大きさの倍率</param>
	/// <param name="outAlpha">濃さ</param>
	void CalculatePopScaleAlpha(float timer,float popDuration,float popStartScale,float holdTime,float fadeDuration,float fadeEndScale,float& outScale,float& outAlpha){
		outScale = 1.0f;
		outAlpha = 1.0f;
		const float fadeStartTime = popDuration + holdTime;
		if(timer < popDuration){
			float popProgress = timer / popDuration;
			outScale = Lerp(popStartScale,1.0f,Easing::EaseOutBack(popProgress));
			outAlpha = popProgress;
		} else if(timer >= fadeStartTime){
			float fadeProgress = std::clamp((timer - fadeStartTime) / fadeDuration,0.0f,1.0f);
			outScale = Lerp(1.0f,fadeEndScale,Easing::EaseOutCubic(fadeProgress));
			outAlpha = 1.0f - fadeProgress;
		}
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
	static_assert(std::size(kCountTextures) == kCountdownMaxNumber,"カウントダウンの数字の画像と数が合っていない");
	for(size_t i = 0; i < countSprites_.size(); ++i){
		countSprites_[i] = createSprite(kCountTextures[i],kCountSize);
	}
}

// 何も表示していない状態に戻す
void StageIntroUI::Reset(){
	isPromptVisible_ = false;
	promptAlpha_ = 0.0f;
	promptTime_ = 0.0f;
	goTimer_ = 0.0f;
	isGoActive_ = false;
	countdownNumber_ = 0;
	countTimer_ = 0.0f;
	isCountActive_ = false;
}

// 「GO!」を出す
void StageIntroUI::BeginGo(){
	goTimer_ = 0.0f;
	isGoActive_ = true;
}

// カウントダウンの数を伝える(数が変わった瞬間に、その数字を最初から飛び出させる)
void StageIntroUI::SetCountdown(int number){
	if(number == countdownNumber_){
		return;
	}
	countdownNumber_ = number;
	if(number < 1 || number > kCountdownMaxNumber){
		return;
	}
	shownCountIndex_ = number - 1;
	countTimer_ = 0.0f;
	isCountActive_ = true;
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

	// カウントダウンの数字は大きい状態から飛び出すように縮んで収まり、少し表示した後に縮みながら消える
	if(isCountActive_){
		countTimer_ += deltaTime;
		float countScale = 1.0f;
		float countAlpha = 1.0f;
		CalculatePopScaleAlpha(countTimer_,kCountPopDuration,kCountPopStartScale,kCountHoldTime,kCountFadeDuration,kCountFadeEndScale,countScale,countAlpha);
		Sprite& countSprite = *countSprites_[shownCountIndex_];
		countSprite.SetPosition({centerX, screenHeight * kCountHeightRate});
		countSprite.SetSize({kCountSize.x * countScale, kCountSize.y * countScale});
		countSprite.SetColor(WhiteWithAlpha(countAlpha));
		countSprite.Update();
		if(countTimer_ >= kCountPopDuration + kCountHoldTime + kCountFadeDuration){
			isCountActive_ = false;
		}
	}

	if(!isGoActive_){
		return;
	}
	goTimer_ += deltaTime;

	// GO! は大きい状態から飛び出すように縮んで収まり、少し表示した後に広がりながら消える
	float goScale = 1.0f;
	float goAlpha = 1.0f;
	const float goFadeStartTime = kGoPopDuration + kGoHoldTime;
	CalculatePopScaleAlpha(goTimer_,kGoPopDuration,kGoPopStartScale,kGoHoldTime,kGoFadeDuration,kGoFadeEndScale,goScale,goAlpha);
	goSprite_->SetPosition({centerX, screenHeight * kGoHeightRate});
	goSprite_->SetSize({kGoSize.x * goScale, kGoSize.y * goScale});
	goSprite_->SetColor(WhiteWithAlpha(goAlpha));
	goSprite_->Update();

	// GO! が消え終わったら、すべての表示が終わる
	if(goTimer_ >= goFadeStartTime + kGoFadeDuration){
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
	if(isCountActive_){
		countSprites_[shownCountIndex_]->Draw();
	}
	if(isGoActive_){
		goSprite_->Draw();
	}
}
