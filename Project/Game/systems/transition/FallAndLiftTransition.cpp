#include "FallAndLiftTransition.h"
#include "systems/transition/TransitionEffectUtil.h"
#include "TextureManager.h"
#include "Easing.h"

using namespace TransitionEffectUtil;

FallAndLiftTransition::FallAndLiftTransition() = default;
FallAndLiftTransition::~FallAndLiftTransition() = default;

// 帯と線のスプライトを生成する
void FallAndLiftTransition::Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite){
	(void)fadeSprite;
	// 使うテクスチャを読み込む(読み込み済みのものは何もしない)
	TextureManager::GetInstance()->LoadTexture(kFadeTexture);

	// FallAndLift の帯と線
	for(int i = 0; i < kColumnCount; ++i){
		columnSprites_.push_back(CreateBandSprite(spriteCommon,kFadeTexture));
		columnLineSprites_.push_back(CreateBandSprite(spriteCommon,kFadeTexture));
	}
}

// スプライトを解放する
void FallAndLiftTransition::Finalize(){
	columnSprites_.clear();
	columnLineSprites_.clear();
}

// FallAndLift: 帯が上からばらばらに落ちてきて画面を覆い、少し沈んでから上へ持ち上げるように抜けていく
void FallAndLiftTransition::Update(float coverRate,bool isRevealing){
	const Vector2 thicknessAxis = GetThicknessAxis(kColumnAngle);
	// 画面の外に置く分を除いた本数で画面の横幅を分ける
	const float spacing = kScreenWidth / static_cast<float>(kColumnCount - kColumnMarginCount);
	const Vector2 bodySize = {spacing * kColumnOverlapScale, kScreenHeight * kColumnHeightScale};
	const Vector2 lineSize = {bodySize.x, kColumnLineThickness};
	const float travel = kScreenHeight * kColumnTravelScale;
	// 左端の帯を画面の左外に置くための、並べ始める位置のずらし量(帯の本数で数える)
	const float startOffset = kHalf - static_cast<float>(kColumnMarginCount) * kHalf;

	for(int i = 0; i < kColumnCount; ++i){
		// 左から順に落ちると単調になるため、落ちる順番をばらけさせる
		int order = (i * kColumnOrderStep) % kColumnCount;
		float progress = GetStaggeredProgress(coverRate,order,kColumnCount,kColumnStagger);

		// 画面の中央の高さからどれだけ上にあるか(マイナスで上)
		float drop = 0.0f;
		if(isRevealing){
			// 戻すときは一度少し沈んでから、加速しながら上へ持ち上げられていく
			drop = -travel * Easing::EaseInBack(kProgressMax - progress);
		} else{
			// 覆うときは上から加速しながら落ちてきて、勢いのまま止まる
			drop = -travel * (kProgressMax - Easing::EaseInCubic(progress));
		}

		Vector2 center = {(static_cast<float>(i) + startOffset) * spacing, kScreenCenter.y + drop};
		PlaceSprite(columnSprites_[i].get(),center,kColumnAngle,bodySize,kBaseColor);

		// 線は帯の下の縁(落ちてくる先頭)の内側に引く
		Vector2 lineCenter = MoveAlong(center,thicknessAxis,(bodySize.y - kColumnLineThickness) * kHalf);
		PlaceSprite(columnLineSprites_[i].get(),lineCenter,kColumnAngle,lineSize,kAccentColor);
	}
}

// 帯をすべて描いてから、帯の下の縁の線を描画する
void FallAndLiftTransition::Draw(){
	for(const std::unique_ptr<Sprite>& sprite : columnSprites_){
		sprite->Draw();
	}
	for(const std::unique_ptr<Sprite>& sprite : columnLineSprites_){
		sprite->Draw();
	}
}
