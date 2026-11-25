#include "InkGauge.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include <algorithm>
#include <cmath>

namespace{
	// 中心から端までの長さを求めるときの割合(半分)
	constexpr float kHalf = 0.5f;
	// 縁の太さを両側の分にする倍率
	constexpr float kBothSides = 2.0f;
	// 点滅で切り替える色の数(赤と元の色の2つを交互に出す)
	constexpr int kBlinkColorCount = 2;
}

InkGauge::InkGauge() = default;
InkGauge::~InkGauge() = default;

// テクスチャの読み込みとスプライトの生成
// 位置は変わらないためここで決め、帯の長さと色は毎フレームのUpdate()で決める
void InkGauge::Initialize(SpriteCommon* spriteCommon){
	spriteCommon_ = spriteCommon;
	TextureManager::GetInstance()->LoadTexture(kWhiteTexturePath);

	// 下地はレティクル(画面中央)の右横に置く
	const Vector2 center = {
		static_cast<float>(WinAPI::kClientWidth) * kScreenCenterRate + kOffsetFromReticleX,
		static_cast<float>(WinAPI::kClientHeight) * kScreenCenterRate
	};

	frameSprite_ = std::make_unique<Sprite>();
	frameSprite_->Initialize(spriteCommon,kWhiteTexturePath);
	frameSprite_->SetAnchorPoint(kCenterAnchor);
	frameSprite_->SetPosition(center);
	frameSprite_->SetSize(kFrameSize);
	frameSprite_->SetColor(kFrameColor);

	// インクの帯は下端を基準にして、長さを変えても下端が動かず上へ伸び縮みするようにする
	fillSprite_ = std::make_unique<Sprite>();
	fillSprite_->Initialize(spriteCommon,kWhiteTexturePath);
	fillSprite_->SetAnchorPoint(kBottomCenterAnchor);
	fillSprite_->SetPosition({center.x, center.y + kFrameSize.y * kHalf - kFramePadding});

	Reset();
}

// 点滅を止める
void InkGauge::Reset(){
	shortageTimer_ = 0.0f;
	blinkTimer_ = 0.0f;
}

// インクが足りなくて撃てなかったときに、赤い点滅を始める
// 左クリックを押しっぱなしで撃てない間は毎フレーム呼ばれるため、点滅している最中は残り時間だけを延ばし、点滅の進み方は最初に戻さない
void InkGauge::NotifyShortage(){
	if(shortageTimer_ <= 0.0f){
		blinkTimer_ = 0.0f;
	}
	shortageTimer_ = kShortageBlinkDuration;
}

// 表示内容の更新
void InkGauge::Update(float rate,float deltaTime){
	if(!frameSprite_ || !fillSprite_){
		return;
	}

	// 帯の長さ(下地の内側の高さに対する割合)
	const Vector2 innerSize = {kFrameSize.x - kFramePadding * kBothSides, kFrameSize.y - kFramePadding * kBothSides};
	fillSprite_->SetSize({innerSize.x, innerSize.y * std::clamp(rate,0.0f,1.0f)});

	// 足りなくて撃てなかった直後は、決めた間隔で赤と元の色を切り替えて点滅させる(点滅し始めは赤から)
	bool isShortageColor = false;
	if(shortageTimer_ > 0.0f){
		const int blinkCount = static_cast<int>(std::floor(blinkTimer_ / kShortageBlinkInterval));
		isShortageColor = (blinkCount % kBlinkColorCount) == 0;
		blinkTimer_ += deltaTime;
		shortageTimer_ = (std::max)(shortageTimer_ - deltaTime,0.0f);
	}
	// 下地も赤くして、インクが空で帯が見えないときでも点滅が分かるようにする
	fillSprite_->SetColor(isShortageColor?kShortageColor:kFillColor);
	frameSprite_->SetColor(isShortageColor?Vector4{kShortageColor.x, kShortageColor.y, kShortageColor.z, kFrameColor.w}:kFrameColor);

	frameSprite_->Update();
	fillSprite_->Update();
}

// 描画処理(2D描画のため、SpriteCommonの描画前処理を先に呼ぶ)
void InkGauge::Draw(){
	if(!spriteCommon_ || !frameSprite_ || !fillSprite_){
		return;
	}
	spriteCommon_->Draw(); // 描画前処理
	frameSprite_->Draw();
	fillSprite_->Draw();
}
