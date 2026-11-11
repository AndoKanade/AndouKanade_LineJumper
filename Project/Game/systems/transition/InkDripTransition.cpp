#include "InkDripTransition.h"
#include "systems/transition/TransitionEffectUtil.h"
#include "TextureManager.h"
#include "Easing.h"
#include <algorithm>
#include <iterator>
#include <utility>

using namespace TransitionEffectUtil;

namespace{
	// 垂れるインクの本体は上の端を基準に下へ伸ばすため、上の辺の中央を基準点にする
	constexpr Vector2 kTopCenterAnchor = {0.5f, 0.0f};
}

InkDripTransition::InkDripTransition() = default;
InkDripTransition::~InkDripTransition() = default;

// 垂れるインクのスプライトを生成する
void InkDripTransition::Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite){
	fadeSprite_ = fadeSprite;
	// 使うテクスチャを読み込む(読み込み済みのものは何もしない)
	TextureManager::GetInstance()->LoadTexture(kFadeTexture);

	// InkDrip の垂れるインク(本体は上の端を基準に伸ばす)
	TextureManager::GetInstance()->LoadTexture(kCircleTexture);
	for(int i = 0; i < kDripCount; ++i){
		std::unique_ptr<Sprite> rim = CreateBandSprite(spriteCommon,kFadeTexture);
		rim->SetAnchorPoint(kTopCenterAnchor);
		dripRimSprites_.push_back(std::move(rim));
		dripRimTipSprites_.push_back(CreateBandSprite(spriteCommon,kCircleTexture));

		std::unique_ptr<Sprite> body = CreateBandSprite(spriteCommon,kFadeTexture);
		body->SetAnchorPoint(kTopCenterAnchor);
		dripSprites_.push_back(std::move(body));
		dripTipSprites_.push_back(CreateBandSprite(spriteCommon,kCircleTexture));
	}
}

// スプライトを解放する(画面全体を覆うスプライトは SceneTransition が解放するため、手放すだけにする)
void InkDripTransition::Finalize(){
	dripRimSprites_.clear();
	dripRimTipSprites_.clear();
	dripSprites_.clear();
	dripTipSprites_.clear();
	fadeSprite_ = nullptr;
}

// InkDrip: 画面の上の縁を暗いインクで埋めてから、太さの違うインクが順番ばらばらに加速しながら垂れてきて画面を覆う
// 戻すときは、覆ったインクがひとかたまりのまま下へ沈んでいく
void InkDripTransition::Update(float coverRate,bool isRevealing){
	const float spacing = kScreenWidth / static_cast<float>(kDripCount);
	const float bandLength = kScreenHeight * kDripBandRate;
	constexpr int widthScaleCount = static_cast<int>(std::size(kDripWidthScales));

	// 戻すときに沈む距離と、画面を暗くする濃さ
	float sink = 0.0f;
	float tintAlpha = 0.0f;
	if(isRevealing){
		sink = kScreenHeight * kDripSinkScale * Easing::EaseInCubic(kFullyCovered - coverRate);
		tintAlpha = kDripTintAlpha * coverRate;
	} else{
		// 垂れている間は少しずつ暗くし、覆い終わる直前に隙間が残っていても隠れるよう濃さを1まで上げる
		tintAlpha = (std::max)(kDripTintAlpha * Easing::EaseOutCubic(coverRate),GetRangeProgress(coverRate,kDripFillStart,kFullyCovered));
	}
	fadeSprite_->SetColor({kBaseColor.x, kBaseColor.y, kBaseColor.z, tintAlpha});
	fadeSprite_->Update();

	for(int i = 0; i < kDripCount; ++i){
		float width = spacing * kDripWidthScales[i % widthScaleCount];
		// 先の丸みの分だけ長くして、垂れきったときに画面の下の端まで届くようにする
		float fullLength = kScreenHeight + width * kHalf;

		float length = fullLength;
		if(!isRevealing){
			// 上の縁の帯はすぐに埋め、そこから先は重さで加速するように垂れる
			int order = (i * kDripOrderStep) % kDripCount;
			float progress = GetStaggeredProgress(coverRate,order,kDripCount,kDripStagger);
			float band = bandLength * Easing::EaseOutCubic(GetRangeProgress(coverRate,kProgressMin,kDripBandEnd));
			length = band + (fullLength - band) * progress * progress;
		}

		float x = (static_cast<float>(i) + kHalf) * spacing;
		// 差し色の縁取りは本体より少しだけ長くして、垂れた先にだけ見えるようにする
		PlaceSprite(dripRimSprites_[i].get(),{x, sink},0.0f,{width, length + kDripRimLead},kAccentColor);
		PlaceSprite(dripRimTipSprites_[i].get(),{x, sink + length + kDripRimLead},0.0f,{width, width},kAccentColor);
		PlaceSprite(dripSprites_[i].get(),{x, sink},0.0f,{width, length},kBaseColor);
		PlaceSprite(dripTipSprites_[i].get(),{x, sink + length},0.0f,{width, width},kBaseColor);
	}
}

// スプライトを描画する
void InkDripTransition::Draw(){
	// 暗くした画面 → 差し色の縁取り → 本体 の順に重ね、本体より先に出た縁取りだけが見えるようにする
	fadeSprite_->Draw();
	for(size_t i = 0; i < dripRimSprites_.size(); ++i){
		dripRimSprites_[i]->Draw();
		dripRimTipSprites_[i]->Draw();
	}
	for(size_t i = 0; i < dripSprites_.size(); ++i){
		dripSprites_[i]->Draw();
		dripTipSprites_[i]->Draw();
	}
}
