#include "SceneTransition.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include "Easing.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <utility>

namespace{
	// 画面を覆う白一色のテクスチャ(色を掛けて黒にする)
	// 読み込み時にミップマップを生成するため、1x1 では失敗する。縮小できる大きさにしている
	const std::string kFadeTexture = "resource/white16x16.png";
	// RetryCutIn で差し込む「RETRY」の文字(白で描いた画像に色を掛けて使う。仮素材)
	const std::string kRetryTexture = "resource/UI/retry.png";
	// InkBurst で弾けるインクのしみ(白で描いた画像に色を掛けて使う。tools/generate_transition_assets.py で生成する)
	const std::string kInkSplatTextures[] = {
		"resource/UI/transition/inkSplat0.png",
		"resource/UI/transition/inkSplat1.png",
		"resource/UI/transition/inkSplat2.png",
	};
	// 垂れるインクの先・塗り進むインクの先・雲のかたまりに使う白い円(同じスクリプトで生成する)
	const std::string kCircleTexture = "resource/UI/transition/circle.png";
	// 垂れるインクの本体は上の端を基準に下へ伸ばすため、上の辺の中央を基準点にする
	constexpr Vector2 kTopCenterAnchor = {0.5f, 0.0f};

	// InkBurst のしみ1つ分の置き方
	struct InkSplatLayout{
		Vector2 position; // 中心の位置(画面の幅・高さに対する割合)
		float sizeScale;  // 大きさ(画面の高さに対する倍率)
		float angle;      // 傾き(ラジアン)
		int textureIndex; // 使うしみの画像の番号
	};
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
	// しみの色を交互に変えるための割る数
	constexpr int kInkColorAlternateCount = 2;

	// GoalStamp で押す「GOAL!」の文字(色付きの画像をそのまま使う。同じスクリプトで生成する)
	const std::string kGoalTexture = "resource/UI/transition/goal.png";
	// GoalStamp で文字を押した衝撃で周りに飛ぶしみ(位置・大きさ・傾き・画像の番号。InkBurst と同じ並べ方)
	constexpr InkSplatLayout kStampSplashLayouts[] = {
		{{0.3f, 0.3f}, 0.5f, 1.0f, 0},
		{{0.72f, 0.28f}, 0.45f, 2.0f, 1},
		{{0.25f, 0.68f}, 0.5f, 3.0f, 2},
		{{0.75f, 0.7f}, 0.55f, 4.0f, 0},
		{{0.5f, 0.78f}, 0.4f, 5.0f, 1},
	};
	constexpr int kStampSplashCount = static_cast<int>(std::size(kStampSplashLayouts));
	// GoalStamp で中央から広がるしみに使う画像の番号
	constexpr int kStampSpreadTextureIndex = 0;

	// CloudRise の雲の大きさ(半径。画面の高さに対する割合)。並べる順に繰り返し使い、大きさを不揃いにする
	constexpr float kCloudRadiusRates[] = {0.26f, 0.2f, 0.3f, 0.23f, 0.28f, 0.18f, 0.32f};
	// 雲を1段おきに半個分ずらして、れんが積みのように並べる(段の番号をこの数で割った余りでずらすか決める)
	constexpr int kCloudRowShiftInterval = 2;
	// 画面を戻すときの割れる向き(画面の左半分の雲は左へ、右半分の雲は右へ)
	constexpr float kSplitToLeft = -1.0f;
	constexpr float kSplitToRight = 1.0f;
	// スプライトを左上基準で配置するためのアンカー
	constexpr Vector2 kTopLeftAnchor = {0.0f, 0.0f};
	// 帯を傾けても中心がずれないよう、中心基準で配置するためのアンカー
	constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
	// 画面を覆う色(黒)
	constexpr float kFadeColor = 0.0f;

	// 画面の大きさと中心
	constexpr float kScreenWidth = static_cast<float>(WinAPI::kClientWidth);
	constexpr float kScreenHeight = static_cast<float>(WinAPI::kClientHeight);
	constexpr float kHalf = 0.5f;
	constexpr Vector2 kScreenCenter = {kScreenWidth * kHalf, kScreenHeight * kHalf};
	// 画面の対角線の長さ。帯の長さをこれにすると、どの傾きでも画面の端から端まで届く
	const float kScreenDiagonal = std::sqrt(kScreenWidth * kScreenWidth + kScreenHeight * kScreenHeight);

	// 進み具合の下限と上限
	constexpr float kProgressMin = 0.0f;
	constexpr float kProgressMax = 1.0f;
	// 帯が左右どちらから走り込むか(長さの向きに対する符号)
	constexpr float kFromLeft = -1.0f;
	constexpr float kFromRight = 1.0f;
	// 帯を左右交互にするための割る数
	constexpr int kAlternateCount = 2;

	// スプライトを、中心の位置・傾き・大きさ・色を指定して置く
	void PlaceSprite(Sprite* sprite,const Vector2& center,float angle,const Vector2& size,const Vector4& color){
		sprite->SetPosition(center);
		sprite->SetRotation(angle);
		sprite->SetSize(size);
		sprite->SetColor(color);
		sprite->Update();
	}

	// 傾けた帯の長さの向き(スプライトの横方向)。Sprite の回転と同じ向きに回す
	Vector2 GetLengthAxis(float angle){
		return {std::cos(angle), std::sin(angle)};
	}

	// 傾けた帯の太さの向き(スプライトの縦方向)
	Vector2 GetThicknessAxis(float angle){
		return {-std::sin(angle), std::cos(angle)};
	}

	// 位置を指定した向きに指定した距離だけ動かす
	Vector2 MoveAlong(const Vector2& position,const Vector2& axis,float distance){
		return {position.x + axis.x * distance, position.y + axis.y * distance};
	}

	// 帯ごとに走り出すタイミングをずらした進み具合(0〜1)を求める
	// order 番目の帯は stagger * order だけ遅れて動き出し、最後の帯が動き終わるときに全体の進み具合が1になる
	float GetStaggeredProgress(float rate,int order,int count,float stagger){
		float duration = kProgressMax - stagger * static_cast<float>(count - 1);
		return std::clamp((rate - stagger * static_cast<float>(order)) / duration,kProgressMin,kProgressMax);
	}

	// 全体の進み具合のうち、start〜end の区間がどこまで進んだか(0〜1)を求める
	float GetRangeProgress(float rate,float start,float end){
		return std::clamp((rate - start) / (end - start),kProgressMin,kProgressMax);
	}
}

SceneTransition::SceneTransition() = default;
SceneTransition::~SceneTransition() = default;

// 画面全体を覆うスプライトを生成する
void SceneTransition::Initialize(SpriteCommon* spriteCommon){
	spriteCommon_ = spriteCommon;

	// 画面全体を覆う黒のスプライト(左上から画面サイズに引き伸ばす)
	TextureManager::GetInstance()->LoadTexture(kFadeTexture);
	fadeSprite_ = std::make_unique<Sprite>();
	fadeSprite_->Initialize(spriteCommon_,kFadeTexture);
	fadeSprite_->SetSize({kScreenWidth, kScreenHeight});
	fadeSprite_->SetAnchorPoint(kTopLeftAnchor);
	fadeSprite_->SetPosition(kTopLeftAnchor);

	// 帯は傾けて置くため、中心基準のスプライトとして作る
	auto createBandSprite = [this](const std::string& texture){
		std::unique_ptr<Sprite> sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_,texture);
		sprite->SetAnchorPoint(kCenterAnchor);
		return sprite;
	};

	// RailStripes の帯と線
	for(int i = 0; i < kStripeCount; ++i){
		stripeSprites_.push_back(createBandSprite(kFadeTexture));
		stripeLineSprites_.push_back(createBandSprite(kFadeTexture));
	}

	// FallAndLift の帯と線
	for(int i = 0; i < kColumnCount; ++i){
		columnSprites_.push_back(createBandSprite(kFadeTexture));
		columnLineSprites_.push_back(createBandSprite(kFadeTexture));
	}

	// RetryCutIn の黒の帯・色付きの帯・文字
	TextureManager::GetInstance()->LoadTexture(kRetryTexture);
	cutInCoverSprite_ = createBandSprite(kFadeTexture);
	cutInBandSprite_ = createBandSprite(kFadeTexture);
	cutInTextSprite_ = createBandSprite(kRetryTexture);
	// 文字は画像の大きさ(Initialize で設定される)を基準に拡大して、演出中は変えない
	const Vector2& textureSize = cutInTextSprite_->GetSize();
	cutInTextSprite_->SetSize({textureSize.x * kCutInTextScale, textureSize.y * kCutInTextScale});

	// InkBurst のしみ
	for(const std::string& texture : kInkSplatTextures){
		TextureManager::GetInstance()->LoadTexture(texture);
	}
	for(const InkSplatLayout& layout : kInkSplatLayouts){
		inkSplatSprites_.push_back(createBandSprite(kInkSplatTextures[layout.textureIndex]));
	}

	// InkDrip の垂れるインク(本体は上の端を基準に伸ばす)
	TextureManager::GetInstance()->LoadTexture(kCircleTexture);
	for(int i = 0; i < kDripCount; ++i){
		std::unique_ptr<Sprite> rim = createBandSprite(kFadeTexture);
		rim->SetAnchorPoint(kTopCenterAnchor);
		dripRimSprites_.push_back(std::move(rim));
		dripRimTipSprites_.push_back(createBandSprite(kCircleTexture));

		std::unique_ptr<Sprite> body = createBandSprite(kFadeTexture);
		body->SetAnchorPoint(kTopCenterAnchor);
		dripSprites_.push_back(std::move(body));
		dripTipSprites_.push_back(createBandSprite(kCircleTexture));
	}

	// RailPaint のレールの線・帯・帯の先と後ろの丸み
	for(int i = 0; i < kRailPaintCount; ++i){
		railPaintLineSprites_.push_back(createBandSprite(kFadeTexture));
		railPaintBodySprites_.push_back(createBandSprite(kFadeTexture));
		railPaintHeadSprites_.push_back(createBandSprite(kCircleTexture));
		railPaintTailSprites_.push_back(createBandSprite(kCircleTexture));
	}

	// GoalStamp の暗くする全面・飛ぶしみ・広がるしみ・文字
	TextureManager::GetInstance()->LoadTexture(kGoalTexture);
	stampDimSprite_ = createBandSprite(kFadeTexture);
	stampDimSprite_->SetAnchorPoint(kTopLeftAnchor);
	stampDimSprite_->SetPosition(kTopLeftAnchor);
	stampDimSprite_->SetSize({kScreenWidth, kScreenHeight});
	for(const InkSplatLayout& layout : kStampSplashLayouts){
		stampSplashSprites_.push_back(createBandSprite(kInkSplatTextures[layout.textureIndex]));
	}
	stampSpreadSprite_ = createBandSprite(kInkSplatTextures[kStampSpreadTextureIndex]);
	stampTextSprite_ = createBandSprite(kGoalTexture);
	// 文字は画像の大きさ(Initialize で設定される)を基準に、押す勢いに合わせて大きさを変える
	const Vector2& goalTextureSize = stampTextSprite_->GetSize();
	stampTextSprite_->SetSize({goalTextureSize.x * kStampTextScale, goalTextureSize.y * kStampTextScale});
	stampTextBaseSize_ = stampTextSprite_->GetSize();

	// CloudRise の雲のかたまり
	for(int i = 0; i < kCloudRowCount * kCloudColumnCount; ++i){
		cloudSprites_.push_back(createBandSprite(kCircleTexture));
	}
}

// スプライトを解放する
void SceneTransition::Finalize(){
	fadeSprite_.reset();
	stripeSprites_.clear();
	stripeLineSprites_.clear();
	columnSprites_.clear();
	columnLineSprites_.clear();
	cutInCoverSprite_.reset();
	cutInBandSprite_.reset();
	cutInTextSprite_.reset();
	inkSplatSprites_.clear();
	dripRimSprites_.clear();
	dripRimTipSprites_.clear();
	dripSprites_.clear();
	dripTipSprites_.clear();
	railPaintLineSprites_.clear();
	railPaintBodySprites_.clear();
	railPaintHeadSprites_.clear();
	railPaintTailSprites_.clear();
	stampDimSprite_.reset();
	stampSplashSprites_.clear();
	stampSpreadSprite_.reset();
	stampTextSprite_.reset();
	cloudSprites_.clear();
	onCovered_ = nullptr;
	phase_ = Phase::None;
	coverRate_ = kNotCovered;
}

// 暗転を始める
bool SceneTransition::Start(std::function<void()> onCovered,Style style){
	if(IsActive()){
		return false;
	}

	onCovered_ = std::move(onCovered);
	style_ = style;
	coverRate_ = kNotCovered;
	phase_ = Phase::Covering;
	return true;
}

// 暗転を進める
void SceneTransition::Update(float deltaTime){
	switch(phase_){
	case Phase::Covering:
		coverRate_ += deltaTime / GetCoverDuration();
		if(coverRate_ >= kFullyCovered){
			coverRate_ = kFullyCovered;
			phase_ = Phase::Covered;
		}
		break;

	case Phase::Covered:
		// 覆い終わったフレームで真っ暗な画面が描画された後に呼ぶため、次のフレームのここで処理する
		// 呼んだ処理の中で次の暗転が予約されても上書きしないよう、先に取り出してから呼ぶ
		if(onCovered_){
			std::function<void()> onCovered = std::move(onCovered_);
			onCovered_ = nullptr;
			onCovered();
		}
		phase_ = Phase::Revealing;
		break;

	case Phase::Revealing:
		coverRate_ -= deltaTime / GetRevealDuration();
		if(coverRate_ <= kNotCovered){
			coverRate_ = kNotCovered;
			phase_ = Phase::None;
		}
		break;

	case Phase::None:
	default:
		break;
	}

	// 暗転していないとき・スプライトを解放した後は見た目を更新しない
	if(!IsActive() || !fadeSprite_){
		return;
	}

	// 今の演出の見た目だけを更新する
	switch(style_){
	case Style::RailStripes:
		UpdateRailStripes();
		break;
	case Style::FallAndLift:
		UpdateFallAndLift();
		break;
	case Style::RetryCutIn:
		UpdateRetryCutIn();
		break;
	case Style::InkBurst:
		UpdateInkBurst();
		break;
	case Style::InkDrip:
		UpdateInkDrip();
		break;
	case Style::RailPaint:
		UpdateRailPaint();
		break;
	case Style::GoalStamp:
		UpdateGoalStamp();
		break;
	case Style::CloudRise:
		UpdateCloudRise();
		break;
	case Style::Fade:
	default:
		UpdateFade();
		break;
	}
}

// 画面を覆うスプライトを描画する
void SceneTransition::Draw(){
	if(!IsActive() || !spriteCommon_ || !fadeSprite_){
		return;
	}

	spriteCommon_->Draw(); // 描画前処理

	switch(style_){
	case Style::RailStripes:
		// 帯をすべて描いてから線を描き、重なった帯に線が隠れないようにする
		for(const std::unique_ptr<Sprite>& sprite : stripeSprites_){
			sprite->Draw();
		}
		for(const std::unique_ptr<Sprite>& sprite : stripeLineSprites_){
			sprite->Draw();
		}
		break;

	case Style::FallAndLift:
		for(const std::unique_ptr<Sprite>& sprite : columnSprites_){
			sprite->Draw();
		}
		for(const std::unique_ptr<Sprite>& sprite : columnLineSprites_){
			sprite->Draw();
		}
		break;

	case Style::RetryCutIn:
		// 暗くした画面 → 広がる黒の帯 → 色付きの帯 → 文字 の順に重ねる
		fadeSprite_->Draw();
		cutInCoverSprite_->Draw();
		cutInBandSprite_->Draw();
		cutInTextSprite_->Draw();
		break;

	case Style::InkBurst:
		// しみを描いてから、隙間を埋める全体の塗りを重ねる
		for(const std::unique_ptr<Sprite>& sprite : inkSplatSprites_){
			sprite->Draw();
		}
		fadeSprite_->Draw();
		break;

	case Style::InkDrip:
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
		break;

	case Style::RailPaint:
		// レールの線を先に描き、そのあとを塗り進む帯で隠していく
		for(const std::unique_ptr<Sprite>& sprite : railPaintLineSprites_){
			sprite->Draw();
		}
		for(int i = 0; i < kRailPaintCount; ++i){
			railPaintBodySprites_[i]->Draw();
			railPaintHeadSprites_[i]->Draw();
			railPaintTailSprites_[i]->Draw();
		}
		break;

	case Style::GoalStamp:
		// 暗くした画面 → 飛んだしみ → 広がるしみ → 文字 → 全体の塗り の順に重ねる
		stampDimSprite_->Draw();
		for(const std::unique_ptr<Sprite>& sprite : stampSplashSprites_){
			sprite->Draw();
		}
		stampSpreadSprite_->Draw();
		stampTextSprite_->Draw();
		fadeSprite_->Draw();
		break;

	case Style::CloudRise:
		for(const std::unique_ptr<Sprite>& sprite : cloudSprites_){
			sprite->Draw();
		}
		fadeSprite_->Draw();
		break;

	case Style::Fade:
	default:
		fadeSprite_->Draw();
		break;
	}
}

// 演出ごとの画面を覆いきるまでの時間(秒)
float SceneTransition::GetCoverDuration() const{
	switch(style_){
	case Style::RailStripes:
		return kStripeCoverDuration;
	case Style::FallAndLift:
		return kColumnCoverDuration;
	case Style::RetryCutIn:
		return kCutInCoverDuration;
	case Style::InkBurst:
		return kInkBurstCoverDuration;
	case Style::InkDrip:
		return kDripCoverDuration;
	case Style::RailPaint:
		return kRailPaintCoverDuration;
	case Style::GoalStamp:
		return kStampCoverDuration;
	case Style::CloudRise:
		return kCloudCoverDuration;
	case Style::Fade:
	default:
		return kFadeCoverDuration;
	}
}

// 演出ごとの画面を戻しきるまでの時間(秒)
float SceneTransition::GetRevealDuration() const{
	switch(style_){
	case Style::RailStripes:
		return kStripeRevealDuration;
	case Style::FallAndLift:
		return kColumnRevealDuration;
	case Style::RetryCutIn:
		return kCutInRevealDuration;
	case Style::InkBurst:
		return kInkBurstRevealDuration;
	case Style::InkDrip:
		return kDripRevealDuration;
	case Style::RailPaint:
		return kRailPaintRevealDuration;
	case Style::GoalStamp:
		return kStampRevealDuration;
	case Style::CloudRise:
		return kCloudRevealDuration;
	case Style::Fade:
	default:
		return kFadeRevealDuration;
	}
}

// Fade: ゆっくり動き出してゆっくり止まるように黒の濃さを変える
void SceneTransition::UpdateFade(){
	fadeSprite_->SetColor({kFadeColor, kFadeColor, kFadeColor, Easing::EaseInOutCubic(coverRate_)});
	fadeSprite_->Update();
}

// RailStripes: レールを模した斜めの帯が左右から交互に走り込んで画面を覆い、始点の方向(左)へ抜けていく
void SceneTransition::UpdateRailStripes(){
	const Vector2 lengthAxis = GetLengthAxis(kStripeAngle);
	const Vector2 thicknessAxis = GetThicknessAxis(kStripeAngle);
	// 帯の長さと、帯を並べる幅の合計を画面の対角線にして、傾けても画面全体を覆えるようにする
	const float length = kScreenDiagonal;
	const float spacing = kScreenDiagonal / static_cast<float>(kStripeCount);
	const Vector2 bodySize = {length, spacing * kStripeOverlapScale};
	const Vector2 lineSize = {length, kStripeLineThickness};
	// 帯を画面の中心を挟んで上下対称に並べるため、真ん中の帯の番号を求めておく
	const float centerIndex = static_cast<float>(kStripeCount - 1) * kHalf;
	const bool isRevealing = phase_ == Phase::Revealing;

	for(int i = 0; i < kStripeCount; ++i){
		float progress = GetStaggeredProgress(coverRate_,i,kStripeCount,kStripeStagger);

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

// FallAndLift: 帯が上からばらばらに落ちてきて画面を覆い、少し沈んでから上へ持ち上げるように抜けていく
void SceneTransition::UpdateFallAndLift(){
	const Vector2 thicknessAxis = GetThicknessAxis(kColumnAngle);
	// 画面の外に置く分を除いた本数で画面の横幅を分ける
	const float spacing = kScreenWidth / static_cast<float>(kColumnCount - kColumnMarginCount);
	const Vector2 bodySize = {spacing * kColumnOverlapScale, kScreenHeight * kColumnHeightScale};
	const Vector2 lineSize = {bodySize.x, kColumnLineThickness};
	const float travel = kScreenHeight * kColumnTravelScale;
	// 左端の帯を画面の左外に置くための、並べ始める位置のずらし量(帯の本数で数える)
	const float startOffset = kHalf - static_cast<float>(kColumnMarginCount) * kHalf;
	const bool isRevealing = phase_ == Phase::Revealing;

	for(int i = 0; i < kColumnCount; ++i){
		// 左から順に落ちると単調になるため、落ちる順番をばらけさせる
		int order = (i * kColumnOrderStep) % kColumnCount;
		float progress = GetStaggeredProgress(coverRate_,order,kColumnCount,kColumnStagger);

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

// RetryCutIn: 画面を暗くして斜めの帯と「RETRY」の文字を差し込み、黒の帯を広げて覆う
// 戻すときは帯と文字が右へ抜け、黒の帯が細くなって画面が戻る
void SceneTransition::UpdateRetryCutIn(){
	const Vector2 lengthAxis = GetLengthAxis(kCutInAngle);
	// 帯の長さを画面の対角線にして、傾けても画面の端から端まで届くようにする
	const float length = kScreenDiagonal;

	// 画面を暗くする濃さ・帯と文字を動かす距離・黒の帯の太さ
	float tintAlpha = 0.0f;
	float bandSlide = 0.0f;
	float textSlide = 0.0f;
	float coverThickness = 0.0f;

	if(phase_ == Phase::Revealing){
		// 戻し始めの前半で、帯と文字が加速しながら右へ抜けきる
		float exit = GetRangeProgress(kFullyCovered - coverRate_,kProgressMin,kCutInExitEnd);
		bandSlide = kFromRight * length * Easing::EaseInCubic(exit);
		// 文字は覆っている間に流れた位置から抜けていく
		textSlide = bandSlide - kCutInTextDrift;
		tintAlpha = kCutInTintAlpha * coverRate_;
		coverThickness = length * Easing::EaseInOutCubic(coverRate_);
	} else{
		// 帯は左から、文字は右から勢いよく差し込み、同時に画面を暗くする
		float slide = Easing::EaseOutCubic(GetRangeProgress(coverRate_,kProgressMin,kCutInSlideEnd));
		bandSlide = kFromLeft * length * (kProgressMax - slide);
		// 差し込んだ後も文字を左へゆっくり流し続け、止まって見えないようにする
		float drift = GetRangeProgress(coverRate_,kCutInSlideEnd,kFullyCovered);
		textSlide = kFromRight * length * (kProgressMax - slide) - kCutInTextDrift * drift;
		tintAlpha = kCutInTintAlpha * slide;
		// 文字を読ませた後で、黒の帯を帯の太さの向きに広げて画面全体を覆う
		float cover = GetRangeProgress(coverRate_,kCutInCoverStart,kFullyCovered);
		coverThickness = length * Easing::EaseInOutCubic(cover);
	}

	fadeSprite_->SetColor({kFadeColor, kFadeColor, kFadeColor, tintAlpha});
	fadeSprite_->Update();

	// 黒の帯は帯の長さと太さを対角線にすると、傾けていても画面全体を覆える
	PlaceSprite(cutInCoverSprite_.get(),kScreenCenter,kCutInAngle,{length, coverThickness},kBaseColor);
	PlaceSprite(cutInBandSprite_.get(),MoveAlong(kScreenCenter,lengthAxis,bandSlide),kCutInAngle,{length, kCutInBandThickness},kAccentColor);
	PlaceSprite(cutInTextSprite_.get(),MoveAlong(kScreenCenter,lengthAxis,textSlide),kCutInAngle,cutInTextSprite_->GetSize(),kBaseColor);
}

// InkBurst: 自分の黄色いインクのしみが画面の中心から外へ次々に弾けて広がり、最後に隙間を塗って画面を覆う
// 戻すときは、しみが少し持ち上がってから下へ垂れ落ちるように抜けていく
void SceneTransition::UpdateInkBurst(){
	const bool isRevealing = phase_ == Phase::Revealing;

	for(int i = 0; i < kInkSplatCount; ++i){
		const InkSplatLayout& layout = kInkSplatLayouts[i];
		float scale = kProgressMax;
		float drop = 0.0f;
		if(isRevealing){
			float progress = GetStaggeredProgress(kFullyCovered - coverRate_,i,kInkSplatCount,kInkSplatRevealStagger);
			drop = kScreenHeight * kInkSplatFallScale * Easing::EaseInBack(progress);
		} else{
			// 弾けた勢いで少し大きくなりすぎてから収まる
			float progress = GetStaggeredProgress(coverRate_,i,kInkSplatCount,kInkSplatCoverStagger);
			scale = Easing::EaseOutBack(progress);
		}

		float size = kScreenHeight * layout.sizeScale * scale;
		Vector2 center = {kScreenWidth * layout.position.x, kScreenHeight * layout.position.y + drop};
		const Vector4& color = (i % kInkColorAlternateCount == 0) ? kInkColor : kInkShadeColor;
		PlaceSprite(inkSplatSprites_[i].get(),center,layout.angle,{size, size},color);
	}

	// しみの隙間に見えている画面を、覆い終わる直前に全体を塗って隠す(戻し始めにすぐ消して、しみが落ちる様子を見せる)
	float fillAlpha = isRevealing
		? kProgressMax - GetRangeProgress(kFullyCovered - coverRate_,kProgressMin,kInkFillRevealEnd)
		: GetRangeProgress(coverRate_,kInkFillCoverStart,kFullyCovered);
	fadeSprite_->SetColor({kInkColor.x, kInkColor.y, kInkColor.z, fillAlpha});
	fadeSprite_->Update();
}

// InkDrip: 画面の上の縁を暗いインクで埋めてから、太さの違うインクが順番ばらばらに加速しながら垂れてきて画面を覆う
// 戻すときは、覆ったインクがひとかたまりのまま下へ沈んでいく
void SceneTransition::UpdateInkDrip(){
	const bool isRevealing = phase_ == Phase::Revealing;
	const float spacing = kScreenWidth / static_cast<float>(kDripCount);
	const float bandLength = kScreenHeight * kDripBandRate;
	constexpr int widthScaleCount = static_cast<int>(std::size(kDripWidthScales));

	// 戻すときに沈む距離と、画面を暗くする濃さ
	float sink = 0.0f;
	float tintAlpha = 0.0f;
	if(isRevealing){
		sink = kScreenHeight * kDripSinkScale * Easing::EaseInCubic(kFullyCovered - coverRate_);
		tintAlpha = kDripTintAlpha * coverRate_;
	} else{
		// 垂れている間は少しずつ暗くし、覆い終わる直前に隙間が残っていても隠れるよう濃さを1まで上げる
		tintAlpha = (std::max)(kDripTintAlpha * Easing::EaseOutCubic(coverRate_),GetRangeProgress(coverRate_,kDripFillStart,kFullyCovered));
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
			float progress = GetStaggeredProgress(coverRate_,order,kDripCount,kDripStagger);
			float band = bandLength * Easing::EaseOutCubic(GetRangeProgress(coverRate_,kProgressMin,kDripBandEnd));
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

// RailPaint: 斜めのレールの線が画面を走った後、そのあとを黄色いインクが筆の先のように塗り進んで画面を覆う
// プレイ中に通ったレールが黄色く塗られていく様子を、画面全体で見せる。戻すときは塗った帯が右へ抜けていく
void SceneTransition::UpdateRailPaint(){
	const bool isRevealing = phase_ == Phase::Revealing;
	const Vector2 lengthAxis = GetLengthAxis(kRailPaintAngle);
	const Vector2 thicknessAxis = GetThicknessAxis(kRailPaintAngle);
	// 帯の長さと、帯を並べる幅の合計を画面の対角線にして、傾けても画面全体を覆えるようにする
	const float length = kScreenDiagonal;
	const float spacing = kScreenDiagonal / static_cast<float>(kRailPaintCount);
	const float thickness = spacing * kRailPaintOverlapScale;
	const float travel = length * kRailPaintTravelScale;
	const float centerIndex = static_cast<float>(kRailPaintCount - 1) * kHalf;

	// レールの線は覆い始めに、左端から右端まで勢いよく走る(戻すときは出さない)
	float lineLength = isRevealing ? 0.0f : length * Easing::EaseOutCubic(GetRangeProgress(coverRate_,kProgressMin,kRailPaintLineEnd));

	for(int i = 0; i < kRailPaintCount; ++i){
		// 帯の左端(塗り始める位置)。帯の中心から長さの半分だけ左へ戻った位置で、画面の外になる
		float across = (static_cast<float>(i) - centerIndex) * spacing;
		Vector2 bandCenter = MoveAlong(kScreenCenter,thicknessAxis,across);
		Vector2 start = MoveAlong(bandCenter,lengthAxis,-length * kHalf);

		// 塗った範囲の先(head)と後ろ(tail)の、左端からの距離
		float head = travel;
		float tail = 0.0f;
		if(isRevealing){
			float progress = GetStaggeredProgress(kFullyCovered - coverRate_,i,kRailPaintCount,kRailPaintRevealStagger);
			tail = travel * Easing::EaseInCubic(progress);
		} else{
			// ゆっくり塗り始め、勢いに乗って進み、端でゆるめる
			float progress = GetStaggeredProgress(coverRate_,i,kRailPaintCount,kRailPaintCoverStagger);
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

// GoalStamp: 画面を少し暗くし、「GOAL!」の文字を大きい状態から押しつけて揺らす。押した衝撃でインクのしみが周りに飛び、
// 中央から黄色いインクが広がって画面を覆う。戻すときは、広がったインクが縮みながら下へ落ちていく
void SceneTransition::UpdateGoalStamp(){
	const Vector2 stampCenter = {kScreenCenter.x, kScreenHeight * kStampHeightRate};

	if(phase_ == Phase::Revealing){
		float progress = Easing::EaseInCubic(kFullyCovered - coverRate_);
		float spreadSize = kScreenHeight * kStampSpreadScale * (kProgressMax - progress);
		Vector2 spreadCenter = {stampCenter.x, stampCenter.y + kScreenHeight * kStampSpreadFallScale * progress};
		PlaceSprite(stampSpreadSprite_.get(),spreadCenter,kStampSpreadAngle,{spreadSize, spreadSize},kInkColor);

		// 覆っていた全体の塗りはすぐに消し、広がったインクが落ちていく様子を見せる
		float fillAlpha = kProgressMax - GetRangeProgress(kFullyCovered - coverRate_,kProgressMin,kStampFillRevealEnd);
		fadeSprite_->SetColor({kInkColor.x, kInkColor.y, kInkColor.z, fillAlpha});
		fadeSprite_->Update();

		// 暗くした画面・飛んだしみ・文字は戻すときには出さない
		stampDimSprite_->SetColor({kFadeColor, kFadeColor, kFadeColor, 0.0f});
		stampDimSprite_->Update();
		for(const std::unique_ptr<Sprite>& sprite : stampSplashSprites_){
			sprite->SetSize({0.0f, 0.0f});
			sprite->Update();
		}
		stampTextSprite_->SetSize({0.0f, 0.0f});
		stampTextSprite_->Update();
		return;
	}

	// 画面を少し暗くして、文字を目立たせる
	stampDimSprite_->SetColor({kFadeColor, kFadeColor, kFadeColor, kStampDimAlpha * GetRangeProgress(coverRate_,kProgressMin,kStampDimEnd)});
	stampDimSprite_->Update();

	// 文字は大きい状態から加速しながら縮んで押しつけられ、押した後は左右に揺れてから止まる
	float slam = GetRangeProgress(coverRate_,kProgressMin,kStampSlamEnd);
	float textScale = Lerp(kStampStartScale,kProgressMax,Easing::EaseInCubic(slam));
	float shake = 0.0f;
	if(slam >= kProgressMax){
		float shakeRemain = kProgressMax - GetRangeProgress(coverRate_,kStampSlamEnd,kStampShakeEnd);
		shake = kStampShakeWidth * std::sin(coverRate_ * kStampShakeSpeed) * shakeRemain;
	}
	float textAlpha = GetRangeProgress(coverRate_,kProgressMin,kStampTextAppearEnd);
	PlaceSprite(stampTextSprite_.get(),{stampCenter.x + shake, stampCenter.y},0.0f,
		{stampTextBaseSize_.x * textScale, stampTextBaseSize_.y * textScale},{kProgressMax, kProgressMax, kProgressMax, textAlpha});

	// 押した瞬間に、周りにしみが勢いよく飛ぶ(押す前は大きさを0にして隠す)
	float splash = slam >= kProgressMax ? Easing::EaseOutBack(GetRangeProgress(coverRate_,kStampSlamEnd,kStampSplashEnd)) : 0.0f;
	for(int i = 0; i < kStampSplashCount; ++i){
		const InkSplatLayout& layout = kStampSplashLayouts[i];
		float size = kScreenHeight * layout.sizeScale * splash;
		Vector2 center = {kScreenWidth * layout.position.x, kScreenHeight * layout.position.y};
		const Vector4& color = (i % kInkColorAlternateCount == 0) ? kInkShadeColor : kInkColor;
		PlaceSprite(stampSplashSprites_[i].get(),center,layout.angle,{size, size},color);
	}

	// 文字を読ませた後で、中央から黄色いインクが加速しながら広がる
	float spreadSize = kScreenHeight * kStampSpreadScale * Easing::EaseInCubic(GetRangeProgress(coverRate_,kStampSpreadStart,kFullyCovered));
	PlaceSprite(stampSpreadSprite_.get(),stampCenter,kStampSpreadAngle,{spreadSize, spreadSize},kInkColor);

	// しみの隙間に見えている画面を、覆い終わる直前に全体を塗って隠す
	fadeSprite_->SetColor({kInkColor.x, kInkColor.y, kInkColor.z, GetRangeProgress(coverRate_,kStampFillStart,kFullyCovered)});
	fadeSprite_->Update();
}

// CloudRise: 下の段から順に雲のかたまりが湧き上がってふくらみ、画面を雲で包む
// 雲海の上を進むステージから、雲を抜けて次の画面へ出るように見せる。戻すときは雲が左右に割れていく
void SceneTransition::UpdateCloudRise(){
	const bool isRevealing = phase_ == Phase::Revealing;
	const float columnSpacing = kProgressMax / static_cast<float>(kCloudColumnCount);
	constexpr int radiusCount = static_cast<int>(std::size(kCloudRadiusRates));
	// 戻すときに左右へ割れる距離(画面の幅に対する割合)
	const float split = isRevealing ? kCloudSplitDistance * Easing::EaseInCubic(kFullyCovered - coverRate_) : 0.0f;

	for(int row = 0; row < kCloudRowCount; ++row){
		// 段の中心の高さ(下の段から順に、画面の下の外から上の外まで並べる)
		float rowRate = static_cast<float>(row) / static_cast<float>(kCloudRowCount - 1);
		float rowHeight = Lerp(kCloudBottomRowHeight,kCloudTopRowHeight,rowRate);
		// 下の段から順に湧き上がる
		float progress = isRevealing ? kProgressMax : Easing::EaseOutCubic(GetStaggeredProgress(coverRate_,row,kCloudRowCount,kCloudStagger));
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
	float fillAlpha = isRevealing ? 0.0f : GetRangeProgress(coverRate_,kCloudFillStart,kFullyCovered);
	fadeSprite_->SetColor({kCloudColor.x, kCloudColor.y, kCloudColor.z, fillAlpha});
	fadeSprite_->Update();
}
