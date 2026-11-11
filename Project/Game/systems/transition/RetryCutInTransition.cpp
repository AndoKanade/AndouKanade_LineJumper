#include "RetryCutInTransition.h"
#include "systems/transition/TransitionEffectUtil.h"
#include "TextureManager.h"
#include "Easing.h"
#include <string>

using namespace TransitionEffectUtil;

namespace{
	// RetryCutIn で差し込む「RETRY」の文字(白で描いた画像に色を掛けて使う。仮素材)
	const std::string kRetryTexture = "resource/UI/retry.png";
}

RetryCutInTransition::RetryCutInTransition() = default;
RetryCutInTransition::~RetryCutInTransition() = default;

// 黒の帯・色付きの帯・文字のスプライトを生成する
void RetryCutInTransition::Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite){
	fadeSprite_ = fadeSprite;
	// 使うテクスチャを読み込む(読み込み済みのものは何もしない)
	TextureManager::GetInstance()->LoadTexture(kFadeTexture);

	// RetryCutIn の黒の帯・色付きの帯・文字
	TextureManager::GetInstance()->LoadTexture(kRetryTexture);
	cutInCoverSprite_ = CreateBandSprite(spriteCommon,kFadeTexture);
	cutInBandSprite_ = CreateBandSprite(spriteCommon,kFadeTexture);
	cutInTextSprite_ = CreateBandSprite(spriteCommon,kRetryTexture);
	// 文字は画像の大きさ(Initialize で設定される)を基準に拡大して、演出中は変えない
	const Vector2& textureSize = cutInTextSprite_->GetSize();
	cutInTextSprite_->SetSize({textureSize.x * kCutInTextScale, textureSize.y * kCutInTextScale});
}

// スプライトを解放する(画面全体を覆うスプライトは SceneTransition が解放するため、手放すだけにする)
void RetryCutInTransition::Finalize(){
	cutInCoverSprite_.reset();
	cutInBandSprite_.reset();
	cutInTextSprite_.reset();
	fadeSprite_ = nullptr;
}

// RetryCutIn: 画面を暗くして斜めの帯と「RETRY」の文字を差し込み、黒の帯を広げて覆う
// 戻すときは帯と文字が右へ抜け、黒の帯が細くなって画面が戻る
void RetryCutInTransition::Update(float coverRate,bool isRevealing){
	const Vector2 lengthAxis = GetLengthAxis(kCutInAngle);
	// 帯の長さを画面の対角線にして、傾けても画面の端から端まで届くようにする
	const float length = kScreenDiagonal;

	// 画面を暗くする濃さ・帯と文字を動かす距離・黒の帯の太さ
	float tintAlpha = 0.0f;
	float bandSlide = 0.0f;
	float textSlide = 0.0f;
	float coverThickness = 0.0f;

	if(isRevealing){
		// 戻し始めの前半で、帯と文字が加速しながら右へ抜けきる
		float exit = GetRangeProgress(kFullyCovered - coverRate,kProgressMin,kCutInExitEnd);
		bandSlide = kFromRight * length * Easing::EaseInCubic(exit);
		// 文字は覆っている間に流れた位置から抜けていく
		textSlide = bandSlide - kCutInTextDrift;
		tintAlpha = kCutInTintAlpha * coverRate;
		coverThickness = length * Easing::EaseInOutCubic(coverRate);
	} else{
		// 帯は左から、文字は右から勢いよく差し込み、同時に画面を暗くする
		float slide = Easing::EaseOutCubic(GetRangeProgress(coverRate,kProgressMin,kCutInSlideEnd));
		bandSlide = kFromLeft * length * (kProgressMax - slide);
		// 差し込んだ後も文字を左へゆっくり流し続け、止まって見えないようにする
		float drift = GetRangeProgress(coverRate,kCutInSlideEnd,kFullyCovered);
		textSlide = kFromRight * length * (kProgressMax - slide) - kCutInTextDrift * drift;
		tintAlpha = kCutInTintAlpha * slide;
		// 文字を読ませた後で、黒の帯を帯の太さの向きに広げて画面全体を覆う
		float cover = GetRangeProgress(coverRate,kCutInCoverStart,kFullyCovered);
		coverThickness = length * Easing::EaseInOutCubic(cover);
	}

	fadeSprite_->SetColor({kFadeColor, kFadeColor, kFadeColor, tintAlpha});
	fadeSprite_->Update();

	// 黒の帯は帯の長さと太さを対角線にすると、傾けていても画面全体を覆える
	PlaceSprite(cutInCoverSprite_.get(),kScreenCenter,kCutInAngle,{length, coverThickness},kBaseColor);
	PlaceSprite(cutInBandSprite_.get(),MoveAlong(kScreenCenter,lengthAxis,bandSlide),kCutInAngle,{length, kCutInBandThickness},kAccentColor);
	PlaceSprite(cutInTextSprite_.get(),MoveAlong(kScreenCenter,lengthAxis,textSlide),kCutInAngle,cutInTextSprite_->GetSize(),kBaseColor);
}

// スプライトを描画する
void RetryCutInTransition::Draw(){
	// 暗くした画面 → 広がる黒の帯 → 色付きの帯 → 文字 の順に重ねる
	fadeSprite_->Draw();
	cutInCoverSprite_->Draw();
	cutInBandSprite_->Draw();
	cutInTextSprite_->Draw();
}
