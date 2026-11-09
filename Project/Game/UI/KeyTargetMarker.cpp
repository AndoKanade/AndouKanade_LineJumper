#include "KeyTargetMarker.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include "Camera/CameraPose.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace{
	// tools/generate_key_target_arrow.py で生成した画像(下向きの三角)
	const std::string kArrowTexture = "resource/UI/keyTargetArrow.png";
	// 白(画像の色をそのまま出す)。α値だけ変えて使う
	constexpr Vector4 kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
	// 上向き(ワールド座標)
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};
}

KeyTargetMarker::KeyTargetMarker() = default;
KeyTargetMarker::~KeyTargetMarker() = default;

// テクスチャの読み込みとスプライトの生成
void KeyTargetMarker::Initialize(SpriteCommon* spriteCommon){
	spriteCommon_ = spriteCommon;

	TextureManager::GetInstance()->LoadTexture(kArrowTexture);
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(spriteCommon,kArrowTexture);
	sprite_->SetSize(kSize);
	sprite_->SetAnchorPoint(kCenterAnchor);
}

// 何も表示していない状態に戻す
void KeyTargetMarker::Reset(){
	alpha_ = 0.0f;
	bobTimer_ = 0.0f;
}

// 表示の動きを進める
// 的の少し上の点を画面上の位置に直し、画面に映っていればそこで跳ねさせ、映っていなければ画面の端に、的のある向きへ回して出す
void KeyTargetMarker::Update(bool isShown,const Vector3& targetPosition,const Vector3& cameraPosition,const Vector3& cameraForward,const Matrix4x4& viewProjection,float deltaTime){
	const Vector3 markerWorld = targetPosition + kWorldUp * kWorldHeightOffset;
	// カメラの後ろにあるときは、画面上の位置が求まらないため出さない
	const bool isInFront = Dot(markerWorld - cameraPosition,cameraForward) > kMinForwardDistance;

	if(isShown && isInFront){
		alpha_ = (std::min)(alpha_ + deltaTime / kFadeInDuration,1.0f);
	} else{
		alpha_ = (std::max)(alpha_ - deltaTime / kFadeOutDuration,0.0f);
	}
	if(alpha_ <= 0.0f || !isInFront){
		return;
	}
	bobTimer_ += deltaTime;

	const float screenWidth = static_cast<float>(WinAPI::kClientWidth);
	const float screenHeight = static_cast<float>(WinAPI::kClientHeight);
	const Vector2 uv = CameraPoseMath::WorldToScreenUv(markerWorld,viewProjection);
	const Vector2 screenPosition = {uv.x * screenWidth, uv.y * screenHeight};

	// 画面の端から余白を除いた範囲に入っていれば、的の上で跳ねさせる(下向きのまま)
	const float minX = kEdgeMargin;
	const float maxX = screenWidth - kEdgeMargin;
	const float minY = kEdgeMargin;
	const float maxY = screenHeight - kEdgeMargin;
	const float bob = kBobHeight * (std::sin(bobTimer_ * kBobSpeed) + 1.0f) * kWaveToRateScale;
	if(screenPosition.x >= minX && screenPosition.x <= maxX && screenPosition.y >= minY && screenPosition.y <= maxY){
		sprite_->SetPosition({screenPosition.x, screenPosition.y - bob});
		sprite_->SetRotation(0.0f);
	} else{
		// 画面の外にあるときは、画面の真ん中から的への向きの先にある、画面の端の位置に出し、三角の先を的へ向ける
		// 三角は下向き(0, 1)が回していない向きなので、向けたい向き(dx, dy)にするには atan2(-dx, dy) だけ回す
		const Vector2 center = {screenWidth * CameraPoseMath::kScreenCenterUv.x, screenHeight * CameraPoseMath::kScreenCenterUv.y};
		const float dx = screenPosition.x - center.x;
		const float dy = screenPosition.y - center.y;
		const Vector2 clamped = {std::clamp(screenPosition.x,minX,maxX), std::clamp(screenPosition.y,minY,maxY)};
		sprite_->SetPosition(clamped);
		sprite_->SetRotation(std::atan2(-dx,dy));
	}
	sprite_->SetColor({kWhite.x, kWhite.y, kWhite.z, alpha_});
	sprite_->Update();
}

// 描画処理(表示していないときは何もしない)
void KeyTargetMarker::Draw(){
	if(!spriteCommon_ || alpha_ <= 0.0f){
		return;
	}
	spriteCommon_->Draw(); // 描画前処理
	sprite_->Draw();
}
