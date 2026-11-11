#include "InkBurstTransition.h"
#include "systems/transition/TransitionEffectUtil.h"
#include "TextureManager.h"
#include "Easing.h"
#include <iterator>
#include <string>

using namespace TransitionEffectUtil;

namespace{
	// 画面の中心から外へ向かう順に並べ、この順に弾けさせる(大きさ・傾き・画像を変えて同じ形が並ばないようにする)
	constexpr InkSplatLayout kInkSplatLayouts[] = {
		{{0.5f, 0.5f}, 1.3f, 0.3f, 0},
		{{0.27f, 0.33f}, 1.2f, 1.9f, 1},
		{{0.73f, 0.64f}, 1.25f, 4.0f, 2},
		{{0.72f, 0.24f}, 1.15f, 2.6f, 0},
		{{0.28f, 0.76f}, 1.2f, 5.1f, 1},
		{{0.06f, 0.5f}, 1.1f, 0.9f, 2},
		{{0.94f, 0.45f}, 1.15f, 3.3f, 0},
		{{0.5f, 0.1f}, 1.1f, 5.8f, 1},
		{{0.52f, 0.92f}, 1.15f, 1.2f, 2},
		{{0.08f, 0.1f}, 1.05f, 2.2f, 0},
		{{0.92f, 0.9f}, 1.1f, 4.6f, 1},
		{{0.1f, 0.92f}, 1.05f, 0.5f, 2},
		{{0.92f, 0.1f}, 1.05f, 3.8f, 0},
	};
	constexpr int kInkSplatCount = static_cast<int>(std::size(kInkSplatLayouts));
}

InkBurstTransition::InkBurstTransition() = default;
InkBurstTransition::~InkBurstTransition() = default;

// インクのしみのスプライトを生成する
void InkBurstTransition::Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite){
	fadeSprite_ = fadeSprite;

	// InkBurst のしみ
	for(const std::string& texture : kInkSplatTextures){
		TextureManager::GetInstance()->LoadTexture(texture);
	}
	for(const InkSplatLayout& layout : kInkSplatLayouts){
		inkSplatSprites_.push_back(CreateBandSprite(spriteCommon,kInkSplatTextures[layout.textureIndex]));
	}
}

// スプライトを解放する(画面全体を覆うスプライトは SceneTransition が解放するため、手放すだけにする)
void InkBurstTransition::Finalize(){
	inkSplatSprites_.clear();
	fadeSprite_ = nullptr;
}

// InkBurst: 自分の黄色いインクのしみが画面の中心から外へ次々に弾けて広がり、最後に隙間を塗って画面を覆う
// 戻すときは、しみが少し持ち上がってから下へ垂れ落ちるように抜けていく
void InkBurstTransition::Update(float coverRate,bool isRevealing){
	for(int i = 0; i < kInkSplatCount; ++i){
		const InkSplatLayout& layout = kInkSplatLayouts[i];
		float scale = kProgressMax;
		float drop = 0.0f;
		if(isRevealing){
			float progress = GetStaggeredProgress(kFullyCovered - coverRate,i,kInkSplatCount,kInkSplatRevealStagger);
			drop = kScreenHeight * kInkSplatFallScale * Easing::EaseInBack(progress);
		} else{
			// 弾けた勢いで少し大きくなりすぎてから収まる
			float progress = GetStaggeredProgress(coverRate,i,kInkSplatCount,kInkSplatCoverStagger);
			scale = Easing::EaseOutBack(progress);
		}

		float size = kScreenHeight * layout.sizeScale * scale;
		Vector2 center = {kScreenWidth * layout.position.x, kScreenHeight * layout.position.y + drop};
		const Vector4& color = (i % kInkColorAlternateCount == 0) ? kInkColor : kInkShadeColor;
		PlaceSprite(inkSplatSprites_[i].get(),center,layout.angle,{size, size},color);
	}

	// しみの隙間に見えている画面を、覆い終わる直前に全体を塗って隠す(戻し始めにすぐ消して、しみが落ちる様子を見せる)
	float fillAlpha = isRevealing
		? kProgressMax - GetRangeProgress(kFullyCovered - coverRate,kProgressMin,kInkFillRevealEnd)
		: GetRangeProgress(coverRate,kInkFillCoverStart,kFullyCovered);
	fadeSprite_->SetColor({kInkColor.x, kInkColor.y, kInkColor.z, fillAlpha});
	fadeSprite_->Update();
}

// スプライトを描画する
void InkBurstTransition::Draw(){
	// しみを描いてから、隙間を埋める全体の塗りを重ねる
	for(const std::unique_ptr<Sprite>& sprite : inkSplatSprites_){
		sprite->Draw();
	}
	fadeSprite_->Draw();
}
