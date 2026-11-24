#include "HelpOverlay.h"
#include "Input.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"

namespace{
	// 説明の本体(操作方法と画面の流れ)
	const std::string kHelpTexture = "resource/UI/help.png";
	// 閉じている間に出す「H あそびかた」の案内
	const std::string kHintTexture = "resource/UI/helpHint.png";
	// 説明の後ろの幕(色を付けて画面全体に引き伸ばす)
	const std::string kDimTexture = "resource/white16x16.png";

	// 説明を開閉するキー
	constexpr BYTE kToggleKey = DIK_H;
}

HelpOverlay::HelpOverlay() = default;
HelpOverlay::~HelpOverlay() = default;

// テクスチャの読み込みとスプライトの生成
void HelpOverlay::Initialize(SpriteCommon* spriteCommon){
	spriteCommon_ = spriteCommon;

	TextureManager* textureManager = TextureManager::GetInstance();
	textureManager->LoadTexture(kHelpTexture);
	textureManager->LoadTexture(kHintTexture);
	textureManager->LoadTexture(kDimTexture);

	const float screenWidth = static_cast<float>(WinAPI::kClientWidth);
	const float screenHeight = static_cast<float>(WinAPI::kClientHeight);

	// 表示位置は変わらないため、ここで位置と大きさまで決めておく
	auto createSprite = [spriteCommon](const std::string& texture,const Vector2& position,const Vector2& size,const Vector2& anchor,const Vector4& color){
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon,texture);
		sprite->SetPosition(position);
		sprite->SetSize(size);
		sprite->SetAnchorPoint(anchor);
		sprite->SetColor(color);
		sprite->Update();
		return sprite;
	};

	const Vector2 screenCenter = {screenWidth * kCenterAnchor.x, screenHeight * kCenterAnchor.y};
	const Vector4 white = {1.0f, 1.0f, 1.0f, 1.0f};
	dimSprite_ = createSprite(kDimTexture,screenCenter,{screenWidth, screenHeight},kCenterAnchor,kDimColor);
	helpSprite_ = createSprite(kHelpTexture,screenCenter,kHelpSize,kCenterAnchor,white);
	hintSprite_ = createSprite(kHintTexture,{kScreenMargin, screenHeight - kScreenMargin},kHintSize,kBottomLeftAnchor,kHintColor);
}

// H キーによる開閉と、表示の更新
void HelpOverlay::Update(Input* input,bool isEnabled){
	isEnabled_ = isEnabled;

	// 開けない状態になったら閉じておく(次に開けるようになったとき、開いたままで始まらないようにする)
	if(!isEnabled_){
		isOpen_ = false;
		return;
	}

	if(input && input->TriggerKey(kToggleKey)){
		isOpen_ = !isOpen_;
	}
}

// 描画処理
void HelpOverlay::Draw(){
	if(!spriteCommon_ || !isEnabled_){
		return;
	}

	spriteCommon_->Draw(); // 描画前処理

	// 開いている間は説明だけを出し、閉じている間は案内だけを出す
	if(isOpen_){
		dimSprite_->Draw();
		helpSprite_->Draw();
	} else{
		hintSprite_->Draw();
	}
}
