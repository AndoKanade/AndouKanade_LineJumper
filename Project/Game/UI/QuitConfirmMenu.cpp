#include "QuitConfirmMenu.h"
#include "Input.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "SoundManager.h"
#include "WinAPI.h"
#include "Easing.h"
#include "Title/StartSequence.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace{
	// tools/generate_quit_confirm_assets.py で生成した画像
	const std::string kPanelTexture = "resource/UI/quitConfirm/panel.png";
	const std::string kOptionTextures[] = {
		"resource/UI/quitConfirm/yesButton.png",
		"resource/UI/quitConfirm/noButton.png",
	};
	// 画面を暗くする幕(白い画像に色を掛けて使う)
	const std::string kDimTexture = "resource/white16x16.png";
	// 「はい」を選んだときに広がるインクのしみ(tools/generate_transition_assets.py で生成。白い画像に色を掛けて使う)
	const std::string kQuitSplatTexture = "resource/UI/transition/inkSplat0.png";

	// 効果音(選び直す・はい・いいえ)
	const char* const kMoveSePath = "resource/music/se/move.mp3";
	const char* const kDecideSePath = "resource/music/se/decide.mp3";
	const char* const kCancelSePath = "resource/music/se/cancel.mp3";

	// 白(画像の色をそのまま出す)
	constexpr Vector4 kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
}

QuitConfirmMenu::QuitConfirmMenu() = default;
QuitConfirmMenu::~QuitConfirmMenu() = default;

// テクスチャ・効果音の読み込みとスプライトの生成
// 位置・大きさ・色は開いている間の ApplyLook() で毎フレーム決めるため、ここでは生成と基準点の設定だけ行う
void QuitConfirmMenu::Initialize(SpriteCommon* spriteCommon){
	spriteCommon_ = spriteCommon;
	TextureManager* textureManager = TextureManager::GetInstance();

	// 幕は画面全体を覆う
	textureManager->LoadTexture(kDimTexture);
	dimSprite_ = std::make_unique<Sprite>();
	dimSprite_->Initialize(spriteCommon,kDimTexture);
	dimSprite_->SetAnchorPoint(kTopLeftAnchor);
	dimSprite_->SetPosition({0.0f, 0.0f});
	dimSprite_->SetSize({static_cast<float>(WinAPI::kClientWidth), static_cast<float>(WinAPI::kClientHeight)});

	textureManager->LoadTexture(kPanelTexture);
	panelSprite_ = std::make_unique<Sprite>();
	panelSprite_->Initialize(spriteCommon,kPanelTexture);
	panelSprite_->SetAnchorPoint(kCenterAnchor);

	for(int i = 0; i < kOptionCount; ++i){
		textureManager->LoadTexture(kOptionTextures[i]);
		optionSprites_[i] = std::make_unique<Sprite>();
		optionSprites_[i]->Initialize(spriteCommon,kOptionTextures[i]);
		optionSprites_[i]->SetAnchorPoint(kCenterAnchor);
	}

	textureManager->LoadTexture(kQuitSplatTexture);
	quitSplatSprite_ = std::make_unique<Sprite>();
	quitSplatSprite_->Initialize(spriteCommon,kQuitSplatTexture);
	quitSplatSprite_->SetAnchorPoint(kCenterAnchor);

	SoundManager* soundManager = SoundManager::GetInstance();
	soundManager->SoundLoadFile(kMoveSePath);
	soundManager->SoundLoadFile(kDecideSePath);
	soundManager->SoundLoadFile(kCancelSePath);
}

// 開閉と選択の更新
void QuitConfirmMenu::Update(Input* input){
	stateTimer_ += kFrameDeltaTime;
	openTimer_ += kFrameDeltaTime;
	selectTimer_ += kFrameDeltaTime;

	switch(state_){
	case State::Closed:
		// 閉じている間は ESC で開くだけ(ゲームの更新は止めない)
		if(input->TriggerKey(DIK_ESCAPE)){
			Open();
		}
		return;
	case State::Opening:
		// 開いている途中でも選び直せるようにし、開ききったら選べる状態にする
		UpdateSelection(input);
		if(state_ == State::Opening && stateTimer_ >= kOpenDuration){
			state_ = State::Open;
		}
		break;
	case State::Open:
		UpdateSelection(input);
		break;
	case State::Closing:
		if(stateTimer_ >= kCloseDuration){
			state_ = State::Closed;
			return;
		}
		break;
	case State::Quitting:
		if(stateTimer_ >= kQuitSplatDuration){
			state_ = State::Quitted;
		}
		break;
	case State::Quitted:
		break;
	}

	ApplyLook();
}

// 開き始める
void QuitConfirmMenu::Open(){
	state_ = State::Opening;
	stateTimer_ = 0.0f;
	openTimer_ = 0.0f;
	selectTimer_ = kSelectPopDuration;
	selectedOption_ = kOptionNo;
	ApplyLook();
}

// 開いている間の選択と決定
// A / D(← / →)で選び、SPACE(Enter)で決める。ESC は「いいえ」を選んだのと同じく閉じる
void QuitConfirmMenu::UpdateSelection(Input* input){
	const bool isLeft = input->TriggerKey(DIK_A) || input->TriggerKey(DIK_LEFT);
	const bool isRight = input->TriggerKey(DIK_D) || input->TriggerKey(DIK_RIGHT);
	if(isLeft || isRight){
		// 項目は2つなので、左右どちらを押しても端で止めずに反対側へ回り込む
		const int step = isRight?1:kOptionCount - 1;
		selectedOption_ = (selectedOption_ + step) % kOptionCount;
		selectTimer_ = 0.0f;
		PlaySe(kMoveSePath);
	}

	const bool isDecided = input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN);
	const bool isCanceled = input->TriggerKey(DIK_ESCAPE);
	if(isDecided && selectedOption_ == kOptionYes){
		state_ = State::Quitting;
		stateTimer_ = 0.0f;
		PlaySe(kDecideSePath);
	} else if((isDecided && selectedOption_ == kOptionNo) || isCanceled){
		state_ = State::Closing;
		stateTimer_ = 0.0f;
		PlaySe(kCancelSePath);
	}
}

// 今の状態と経過時間から、スプライトの位置・大きさ・色を決める
void QuitConfirmMenu::ApplyLook(){
	const float screenWidth = static_cast<float>(WinAPI::kClientWidth);
	const float screenHeight = static_cast<float>(WinAPI::kClientHeight);

	// 板とボタン全体に掛ける大きさの倍率と濃さ
	// 開くときは小さい状態から弾むように広げ、閉じるときは縮めながら薄くする
	float panelScale = 1.0f;
	float optionPopScale = 1.0f;
	float visibility = 1.0f;
	if(state_ == State::Opening){
		const float panelProgress = std::clamp(stateTimer_ / kOpenDuration,0.0f,1.0f);
		panelScale = Lerp(kOpenStartScale,1.0f,Easing::EaseOutBack(panelProgress));
		// ボタンは板より少し遅れて飛び出させる
		const float optionProgress = std::clamp((stateTimer_ - kOptionPopDelay) / (kOpenDuration - kOptionPopDelay),0.0f,1.0f);
		optionPopScale = Easing::EaseOutBack(optionProgress);
		visibility = Easing::EaseOutCubic(panelProgress);
	} else if(state_ == State::Closing){
		const float closeProgress = std::clamp(stateTimer_ / kCloseDuration,0.0f,1.0f);
		const float eased = Easing::EaseInCubic(closeProgress);
		panelScale = Lerp(1.0f,kCloseEndScale,eased);
		optionPopScale = panelScale;
		visibility = 1.0f - eased;
	}

	dimSprite_->SetColor({kDimColor.x, kDimColor.y, kDimColor.z, kDimAlpha * visibility});
	dimSprite_->Update();

	const Vector2 panelCenter = {screenWidth * 0.5f, screenHeight * kPanelCenterYRate};
	panelSprite_->SetPosition(panelCenter);
	panelSprite_->SetSize({kPanelSize.x * panelScale, kPanelSize.y * panelScale});
	panelSprite_->SetColor({kWhite.x, kWhite.y, kWhite.z, visibility});
	panelSprite_->Update();

	// ボタンは板と一緒に大きさが変わるよう、板の中心からの距離にも板の倍率を掛ける
	// 選んでいる方は色を付けて大きくし、脈打たせて左右に揺らす。選び直した瞬間は大きく弾ませる
	const float pulse = std::sin(openTimer_ * kPulseSpeed) * kPulseAmplitude;
	const float wobble = std::sin(openTimer_ * kWobbleSpeed) * kWobbleAngle;
	const float selectProgress = std::clamp(selectTimer_ / kSelectPopDuration,0.0f,1.0f);
	const float selectPop = Lerp(kSelectPopScale,1.0f,Easing::EaseOutBack(selectProgress));
	const float optionCenterY = panelCenter.y + (screenHeight * kOptionCenterYRate - panelCenter.y) * panelScale;
	for(int i = 0; i < kOptionCount; ++i){
		const bool isSelected = (i == selectedOption_);
		// 左のボタン(はい)は中央から左へ、右のボタン(いいえ)は右へずらす
		const float side = (i == kOptionYes)?-1.0f:1.0f;
		optionSprites_[i]->SetPosition({panelCenter.x + side * kOptionOffsetX * panelScale, optionCenterY});

		const float lookScale = isSelected?(kSelectedOptionScale + pulse) * selectPop:kUnselectedOptionScale;
		const float scale = lookScale * optionPopScale * panelScale;
		optionSprites_[i]->SetSize({kOptionSize.x * scale, kOptionSize.y * scale});
		optionSprites_[i]->SetRotation(isSelected?wobble:0.0f);

		const Vector4& color = isSelected?kSelectedOptionColors[i]:kUnselectedOptionColor;
		optionSprites_[i]->SetColor({color.x, color.y, color.z, color.w * visibility});
		optionSprites_[i]->Update();
	}

	// 「はい」を選んだら、そのボタンからピンクのインクを回しながら画面いっぱいに広げる
	if(state_ == State::Quitting || state_ == State::Quitted){
		const float splatProgress = std::clamp(stateTimer_ / kQuitSplatDuration,0.0f,1.0f);
		const float eased = Easing::EaseInCubic(splatProgress);
		const float splatSize = Lerp(kQuitSplatStartSize,kQuitSplatEndSize,eased);
		const Vector4& splatColor = kSelectedOptionColors[kOptionYes];
		quitSplatSprite_->SetPosition({panelCenter.x - kOptionOffsetX, optionCenterY});
		quitSplatSprite_->SetSize({splatSize, splatSize});
		quitSplatSprite_->SetRotation(kQuitSplatSpin * eased);
		quitSplatSprite_->SetColor(splatColor);
		quitSplatSprite_->Update();
	}
}

// 効果音を鳴らす(音量はタイトルの効果音と同じ調整項目を使う)
void QuitConfirmMenu::PlaySe(const char* path) const{
	SoundManager::GetInstance()->PlayAudio(path,StartSequence::GetSeVolume());
}

// 描画処理(閉じているときは何もしない)
void QuitConfirmMenu::Draw(){
	if(!spriteCommon_ || state_ == State::Closed){
		return;
	}
	spriteCommon_->Draw(); // 描画前処理
	dimSprite_->Draw();
	panelSprite_->Draw();
	for(const auto& optionSprite : optionSprites_){
		optionSprite->Draw();
	}
	if(state_ == State::Quitting || state_ == State::Quitted){
		quitSplatSprite_->Draw();
	}
}
