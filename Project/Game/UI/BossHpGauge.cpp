#include "BossHpGauge.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include <algorithm>

namespace{
	// 中心から端までの長さを求めるときの割合(半分)
	constexpr float kHalf = 0.5f;
	// 縁の太さを両側の分にする倍率
	constexpr float kBothSides = 2.0f;
}

BossHpGauge::BossHpGauge() = default;
BossHpGauge::~BossHpGauge() = default;

// テクスチャの読み込みとスプライトの生成
// 位置は変わらないためここで決め、帯の長さ・色・濃さは毎フレームのUpdate()で決める
void BossHpGauge::Initialize(SpriteCommon* spriteCommon){
	spriteCommon_ = spriteCommon;
	TextureManager::GetInstance()->LoadTexture(kWhiteTexturePath);

	const float screenWidth = static_cast<float>(WinAPI::kClientWidth);
	const float screenHeight = static_cast<float>(WinAPI::kClientHeight);
	const Vector2 center = {screenWidth * kCenterXRate, screenHeight * kHeightRate};
	// 帯の左端(下地の左端から縁の太さだけ内側)
	const Vector2 barLeft = {center.x - kFrameSize.x * kHalf + kFramePadding, center.y};

	frameSprite_ = std::make_unique<Sprite>();
	frameSprite_->Initialize(spriteCommon,kWhiteTexturePath);
	frameSprite_->SetAnchorPoint(kCenterAnchor);
	frameSprite_->SetPosition(center);
	frameSprite_->SetSize(kFrameSize);

	// 帯は左端を基準にして、長さを変えても左端が動かないようにする
	delaySprite_ = std::make_unique<Sprite>();
	delaySprite_->Initialize(spriteCommon,kWhiteTexturePath);
	delaySprite_->SetAnchorPoint(kLeftCenterAnchor);
	delaySprite_->SetPosition(barLeft);

	fillSprite_ = std::make_unique<Sprite>();
	fillSprite_->Initialize(spriteCommon,kWhiteTexturePath);
	fillSprite_->SetAnchorPoint(kLeftCenterAnchor);
	fillSprite_->SetPosition(barLeft);

	// 形態の印は、ゲージの下に左端をそろえて並べる
	const float markTop = center.y + kFrameSize.y * kHalf + kPhaseMarkGap;
	for(int i = 0; i < kPhaseCount; ++i){
		auto mark = std::make_unique<Sprite>();
		mark->Initialize(spriteCommon,kWhiteTexturePath);
		mark->SetAnchorPoint(kLeftCenterAnchor);
		mark->SetPosition({center.x - kFrameSize.x * kHalf + static_cast<float>(i) * (kPhaseMarkSize.x + kPhaseMarkSpacing), markTop + kPhaseMarkSize.y * kHalf});
		mark->SetSize(kPhaseMarkSize);
		phaseSprites_.push_back(std::move(mark));
	}

	Reset();
}

// 消した状態に戻す
void BossHpGauge::Reset(){
	alpha_ = 0.0f;
	displayedRate_ = 1.0f;
	delayRate_ = 1.0f;
	previousPhase_ = 0;
}

// 表示内容の更新
void BossHpGauge::Update(bool isShown,int hp,int maxHp,int phase,float deltaTime){
	// 出すときはふわっと濃くし、消すときは薄くする
	if(isShown){
		alpha_ = (std::min)(alpha_ + kFadeInSpeed * deltaTime,1.0f);
	} else{
		alpha_ = (std::max)(alpha_ - kFadeOutSpeed * deltaTime,0.0f);
	}
	if(alpha_ <= 0.0f){
		// 見えていない間に体力・形態が変わっても、次に出したときは今の値から始める
		previousPhase_ = 0;
		return;
	}

	const float actualRate = (maxHp > 0)?std::clamp(static_cast<float>(hp) / static_cast<float>(maxHp),0.0f,1.0f):0.0f;

	if(previousPhase_ == 0){
		// 出し始めは、今の体力をそのまま表示する
		displayedRate_ = actualRate;
		delayRate_ = actualRate;
	} else if(phase != previousPhase_){
		// 形態が変わったら、空の状態から満タンまで伸ばして、体力が戻ったことを見せる
		displayedRate_ = 0.0f;
		delayRate_ = 0.0f;
	}
	previousPhase_ = phase;

	// 減るときはすぐに、増えるとき(形態が変わったとき)は決めた速さで伸ばす
	if(actualRate < displayedRate_){
		displayedRate_ = actualRate;
	} else{
		displayedRate_ = (std::min)(displayedRate_ + kRefillSpeed * deltaTime,actualRate);
	}
	// 白い帯は体力の帯より長い間だけ見え、少し遅れて縮んで追いつく
	delayRate_ = (std::max)(delayRate_ - kDelayShrinkSpeed * deltaTime,displayedRate_);

	// 帯の長さ(下地の内側の幅に対する割合)
	const Vector2 barSize = {kFrameSize.x - kFramePadding * kBothSides, kFrameSize.y - kFramePadding * kBothSides};
	const int phaseIndex = std::clamp(phase - 1,0,kPhaseCount - 1);
	const Vector4& phaseColor = kPhaseColors[phaseIndex];

	frameSprite_->SetColor({kFrameColor.x, kFrameColor.y, kFrameColor.z, kFrameColor.w * alpha_});
	frameSprite_->Update();

	delaySprite_->SetSize({barSize.x * delayRate_, barSize.y});
	delaySprite_->SetColor({kDelayColor.x, kDelayColor.y, kDelayColor.z, kDelayColor.w * alpha_});
	delaySprite_->Update();

	fillSprite_->SetSize({barSize.x * displayedRate_, barSize.y});
	fillSprite_->SetColor({phaseColor.x, phaseColor.y, phaseColor.z, phaseColor.w * alpha_});
	fillSprite_->Update();

	// 形態の印は、今の形態とこれからの形態をその形態の色で、使い切った形態を暗い色で出す
	for(int i = 0; i < kPhaseCount; ++i){
		const Vector4& markColor = (i >= phaseIndex)?kPhaseColors[i]:kUsedPhaseColor;
		phaseSprites_[i]->SetColor({markColor.x, markColor.y, markColor.z, markColor.w * alpha_});
		phaseSprites_[i]->Update();
	}
}

// 描画処理(見えていないときは何もしない)
void BossHpGauge::Draw(){
	if(!spriteCommon_ || alpha_ <= 0.0f){
		return;
	}
	spriteCommon_->Draw(); // 描画前処理
	frameSprite_->Draw();
	delaySprite_->Draw();
	fillSprite_->Draw();
	for(auto& mark : phaseSprites_){
		mark->Draw();
	}
}
