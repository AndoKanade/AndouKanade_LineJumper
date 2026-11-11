#include "RailStripesTransition.h"
#include "systems/transition/TransitionEffectUtil.h"
#include "TextureManager.h"
#include "Easing.h"

using namespace TransitionEffectUtil;

namespace{
	// 帯を左右交互にするための割る数
	constexpr int kAlternateCount = 2;
}

RailStripesTransition::RailStripesTransition() = default;
RailStripesTransition::~RailStripesTransition() = default;

// 帯と線のスプライトを生成する
void RailStripesTransition::Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite){
	(void)fadeSprite;
	// 使うテクスチャを読み込む(読み込み済みのものは何もしない)
	TextureManager::GetInstance()->LoadTexture(kFadeTexture);

	// RailStripes の帯と線
	for(int i = 0; i < kStripeCount; ++i){
		stripeSprites_.push_back(CreateBandSprite(spriteCommon,kFadeTexture));
		stripeLineSprites_.push_back(CreateBandSprite(spriteCommon,kFadeTexture));
	}
}

// スプライトを解放する
void RailStripesTransition::Finalize(){
	stripeSprites_.clear();
	stripeLineSprites_.clear();
}

// RailStripes: レールを模した斜めの帯が左右から交互に走り込んで画面を覆い、始点の方向(左)へ抜けていく
void RailStripesTransition::Update(float coverRate,bool isRevealing){
	const Vector2 lengthAxis = GetLengthAxis(kStripeAngle);
	const Vector2 thicknessAxis = GetThicknessAxis(kStripeAngle);
	// 帯の長さと、帯を並べる幅の合計を画面の対角線にして、傾けても画面全体を覆えるようにする
	const float length = kScreenDiagonal;
	const float spacing = kScreenDiagonal / static_cast<float>(kStripeCount);
	const Vector2 bodySize = {length, spacing * kStripeOverlapScale};
	const Vector2 lineSize = {length, kStripeLineThickness};
	// 帯を画面の中心を挟んで上下対称に並べるため、真ん中の帯の番号を求めておく
	const float centerIndex = static_cast<float>(kStripeCount - 1) * kHalf;

	for(int i = 0; i < kStripeCount; ++i){
		float progress = GetStaggeredProgress(coverRate,i,kStripeCount,kStripeStagger);

		// 帯の長さの向きに動かす距離(帯の長さだけ動かすと画面の外に出きる)
		float slide = 0.0f;
		if(isRevealing){
			// 戻すときはすべての帯がレールの始点の方向(左)へ、加速しながら抜けていく
			slide = kFromLeft * length * Easing::EaseInCubic(kProgressMax - progress);
		} else{
			// 覆うときは左右から交互に、勢いよく走り込んで中央で止まる
			float side = (i % kAlternateCount == 0) ? kFromLeft : kFromRight;
			slide = side * length * (kProgressMax - Easing::EaseOutCubic(progress));
		}

		float across = (static_cast<float>(i) - centerIndex) * spacing;
		Vector2 center = MoveAlong(MoveAlong(kScreenCenter,lengthAxis,slide),thicknessAxis,across);
		PlaceSprite(stripeSprites_[i].get(),center,kStripeAngle,bodySize,kBaseColor);

		// レールの線は帯の上の縁に引く
		Vector2 lineCenter = MoveAlong(center,thicknessAxis,-spacing * kHalf);
		PlaceSprite(stripeLineSprites_[i].get(),lineCenter,kStripeAngle,lineSize,kAccentColor);
	}
}

// 帯と線を描画する
void RailStripesTransition::Draw(){
	// 帯をすべて描いてから線を描き、重なった帯に線が隠れないようにする
	for(const std::unique_ptr<Sprite>& sprite : stripeSprites_){
		sprite->Draw();
	}
	for(const std::unique_ptr<Sprite>& sprite : stripeLineSprites_){
		sprite->Draw();
	}
}
