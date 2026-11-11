#include "RailPaintTransition.h"
#include "systems/transition/TransitionEffectUtil.h"
#include "TextureManager.h"
#include "Easing.h"

using namespace TransitionEffectUtil;

RailPaintTransition::RailPaintTransition() = default;
RailPaintTransition::~RailPaintTransition() = default;

// レールの線・帯・帯の先と後ろの丸みのスプライトを生成する
void RailPaintTransition::Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite){
	(void)fadeSprite;
	// 使うテクスチャを読み込む(読み込み済みのものは何もしない)
	TextureManager::GetInstance()->LoadTexture(kFadeTexture);
	TextureManager::GetInstance()->LoadTexture(kCircleTexture);

	// RailPaint のレールの線・帯・帯の先と後ろの丸み
	for(int i = 0; i < kRailPaintCount; ++i){
		railPaintLineSprites_.push_back(CreateBandSprite(spriteCommon,kFadeTexture));
		railPaintBodySprites_.push_back(CreateBandSprite(spriteCommon,kFadeTexture));
		railPaintHeadSprites_.push_back(CreateBandSprite(spriteCommon,kCircleTexture));
		railPaintTailSprites_.push_back(CreateBandSprite(spriteCommon,kCircleTexture));
	}
}

// スプライトを解放する
void RailPaintTransition::Finalize(){
	railPaintLineSprites_.clear();
	railPaintBodySprites_.clear();
	railPaintHeadSprites_.clear();
	railPaintTailSprites_.clear();
}

// RailPaint: 斜めのレールの線が画面を走った後、そのあとを黄色いインクが筆の先のように塗り進んで画面を覆う
// プレイ中に通ったレールが黄色く塗られていく様子を、画面全体で見せる。戻すときは塗った帯が右へ抜けていく
void RailPaintTransition::Update(float coverRate,bool isRevealing){
	const Vector2 lengthAxis = GetLengthAxis(kRailPaintAngle);
	const Vector2 thicknessAxis = GetThicknessAxis(kRailPaintAngle);
	// 帯の長さと、帯を並べる幅の合計を画面の対角線にして、傾けても画面全体を覆えるようにする
	const float length = kScreenDiagonal;
	const float spacing = kScreenDiagonal / static_cast<float>(kRailPaintCount);
	const float thickness = spacing * kRailPaintOverlapScale;
	const float travel = length * kRailPaintTravelScale;
	const float centerIndex = static_cast<float>(kRailPaintCount - 1) * kHalf;

	// レールの線は覆い始めに、左端から右端まで勢いよく走る(戻すときは出さない)
	float lineLength = isRevealing ? 0.0f : length * Easing::EaseOutCubic(GetRangeProgress(coverRate,kProgressMin,kRailPaintLineEnd));

	for(int i = 0; i < kRailPaintCount; ++i){
		// 帯の左端(塗り始める位置)。帯の中心から長さの半分だけ左へ戻った位置で、画面の外になる
		float across = (static_cast<float>(i) - centerIndex) * spacing;
		Vector2 bandCenter = MoveAlong(kScreenCenter,thicknessAxis,across);
		Vector2 start = MoveAlong(bandCenter,lengthAxis,-length * kHalf);

		// 塗った範囲の先(head)と後ろ(tail)の、左端からの距離
		float head = travel;
		float tail = 0.0f;
		if(isRevealing){
			float progress = GetStaggeredProgress(kFullyCovered - coverRate,i,kRailPaintCount,kRailPaintRevealStagger);
			tail = travel * Easing::EaseInCubic(progress);
		} else{
			// ゆっくり塗り始め、勢いに乗って進み、端でゆるめる
			float progress = GetStaggeredProgress(coverRate,i,kRailPaintCount,kRailPaintCoverStagger);
			head = travel * Easing::EaseInOutCubic(progress);
		}

		PlaceSprite(railPaintLineSprites_[i].get(),MoveAlong(start,lengthAxis,lineLength * kHalf),kRailPaintAngle,{lineLength, kRailPaintLineThickness},kAccentColor);

		const Vector4& color = (i % kInkColorAlternateCount == 0) ? kInkColor : kInkShadeColor;
		float paintedLength = head - tail;
		PlaceSprite(railPaintBodySprites_[i].get(),MoveAlong(start,lengthAxis,tail + paintedLength * kHalf),kRailPaintAngle,{paintedLength, thickness},color);
		// 帯の両端を丸くして、筆で塗ったインクに見せる(塗っていないときは大きさを0にして隠す)
		float capSize = paintedLength > 0.0f ? thickness : 0.0f;
		PlaceSprite(railPaintHeadSprites_[i].get(),MoveAlong(start,lengthAxis,head),0.0f,{capSize, capSize},color);
		PlaceSprite(railPaintTailSprites_[i].get(),MoveAlong(start,lengthAxis,tail),0.0f,{capSize, capSize},color);
	}
}

// スプライトを描画する
void RailPaintTransition::Draw(){
	// レールの線を先に描き、そのあとを塗り進む帯で隠していく
	for(const std::unique_ptr<Sprite>& sprite : railPaintLineSprites_){
		sprite->Draw();
	}
	for(int i = 0; i < kRailPaintCount; ++i){
		railPaintBodySprites_[i]->Draw();
		railPaintHeadSprites_[i]->Draw();
		railPaintTailSprites_[i]->Draw();
	}
}
