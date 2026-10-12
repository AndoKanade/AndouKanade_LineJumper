#include "SceneTransition.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include "Easing.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace{
	// 画面を覆う白一色のテクスチャ(色を掛けて黒にする)
	// 読み込み時にミップマップを生成するため、1x1 では失敗する。縮小できる大きさにしている
	const std::string kFadeTexture = "resource/white16x16.png";
	// RetryCutIn で差し込む「RETRY」の文字(白で描いた画像に色を掛けて使う。仮素材)
	const std::string kRetryTexture = "resource/UI/retry.png";
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
