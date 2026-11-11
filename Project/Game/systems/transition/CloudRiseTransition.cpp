#include "CloudRiseTransition.h"
#include "systems/transition/TransitionEffectUtil.h"
#include "TextureManager.h"
#include "Easing.h"
#include <iterator>

using namespace TransitionEffectUtil;

namespace{
	// CloudRise の雲の大きさ(半径。画面の高さに対する割合)。並べる順に繰り返し使い、大きさを不揃いにする
	constexpr float kCloudRadiusRates[] = {0.26f, 0.2f, 0.3f, 0.23f, 0.28f, 0.18f, 0.32f};
	// 雲を1段おきに半個分ずらして、れんが積みのように並べる(段の番号をこの数で割った余りでずらすか決める)
	constexpr int kCloudRowShiftInterval = 2;
	// 画面を戻すときの割れる向き(画面の左半分の雲は左へ、右半分の雲は右へ)
	constexpr float kSplitToLeft = -1.0f;
	constexpr float kSplitToRight = 1.0f;
}

CloudRiseTransition::CloudRiseTransition() = default;
CloudRiseTransition::~CloudRiseTransition() = default;

// 雲のかたまりのスプライトを生成する
void CloudRiseTransition::Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite){
	fadeSprite_ = fadeSprite;
	// 使うテクスチャを読み込む(読み込み済みのものは何もしない)
	TextureManager::GetInstance()->LoadTexture(kCircleTexture);

	// CloudRise の雲のかたまり
	for(int i = 0; i < kCloudRowCount * kCloudColumnCount; ++i){
		cloudSprites_.push_back(CreateBandSprite(spriteCommon,kCircleTexture));
	}
}

// スプライトを解放する(画面全体を覆うスプライトは SceneTransition が解放するため、手放すだけにする)
void CloudRiseTransition::Finalize(){
	cloudSprites_.clear();
	fadeSprite_ = nullptr;
}

// CloudRise: 下の段から順に雲のかたまりが湧き上がってふくらみ、画面を雲で包む
// 雲海の上を進むステージから、雲を抜けて次の画面へ出るように見せる。戻すときは雲が左右に割れていく
void CloudRiseTransition::Update(float coverRate,bool isRevealing){
	const float columnSpacing = kProgressMax / static_cast<float>(kCloudColumnCount);
	constexpr int radiusCount = static_cast<int>(std::size(kCloudRadiusRates));
	// 戻すときに左右へ割れる距離(画面の幅に対する割合)
	const float split = isRevealing ? kCloudSplitDistance * Easing::EaseInCubic(kFullyCovered - coverRate) : 0.0f;

	for(int row = 0; row < kCloudRowCount; ++row){
		// 段の中心の高さ(下の段から順に、画面の下の外から上の外まで並べる)
		float rowRate = static_cast<float>(row) / static_cast<float>(kCloudRowCount - 1);
		float rowHeight = Lerp(kCloudBottomRowHeight,kCloudTopRowHeight,rowRate);
		// 下の段から順に湧き上がる
		float progress = isRevealing ? kProgressMax : Easing::EaseOutCubic(GetStaggeredProgress(coverRate,row,kCloudRowCount,kCloudStagger));
		// 1段おきに半個分ずらして、雲どうしの隙間が縦に並ばないようにする
		float rowShift = (row % kCloudRowShiftInterval == 0) ? 0.0f : columnSpacing * kHalf;

		for(int column = 0; column < kCloudColumnCount; ++column){
			int index = row * kCloudColumnCount + column;
			float x = (static_cast<float>(column) + kHalf) * columnSpacing + rowShift;
			// 画面の左半分の雲は左へ、右半分の雲は右へ割れる
			x += split * (x < kHalf ? kSplitToLeft : kSplitToRight);
			// 湧き上がる前は画面の高さ分だけ下にあり、ふくらみながら上がってくる(湧き上がる前は大きさを0にして隠す)
			float y = rowHeight + (kProgressMax - progress);
			float radius = kScreenHeight * kCloudRadiusRates[index % radiusCount];
			float size = progress > 0.0f ? (radius + radius) * Lerp(kCloudStartSizeRate,kProgressMax,progress) : 0.0f;

			const Vector4& color = (index % kCloudShadeInterval == 0) ? kCloudShadeColor : kCloudColor;
			PlaceSprite(cloudSprites_[index].get(),{kScreenWidth * x, kScreenHeight * y},0.0f,{size, size},color);
		}
	}

	// 雲の隙間に見えている画面を、覆い終わる直前に全体を白で塗って隠す(戻すときはすぐに消す)
	float fillAlpha = isRevealing ? 0.0f : GetRangeProgress(coverRate,kCloudFillStart,kFullyCovered);
	fadeSprite_->SetColor({kCloudColor.x, kCloudColor.y, kCloudColor.z, fillAlpha});
	fadeSprite_->Update();
}

// 雲を描いてから、隙間を埋める全体の塗りを重ねて描画する
void CloudRiseTransition::Draw(){
	for(const std::unique_ptr<Sprite>& sprite : cloudSprites_){
		sprite->Draw();
	}
	fadeSprite_->Draw();
}
