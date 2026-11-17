#include "ClearResultUI.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include "Easing.h"
#include <algorithm>
#include <cmath>

namespace{
	// 画面全体を塗る白一色のテクスチャ(色を掛けて暗い下地にする)
	const std::string kWhiteTexture = "resource/white16x16.png";
	// 「GAME CLEAR!」と「SPACE でタイトルへ」を描いた画像(仮素材。tools/generate_result_assets.py で生成する)
	const std::string kClearTitleTexture = "resource/UI/clearTitle.png";
	// インクのしみ(白で描いた画像に色を掛けて使う。クリア画面へ切り替えるときの InkBurst と同じ画像)
	// 結果の行ごとに別の形にして、同じ形が並ばないようにする
	const std::string kSplatTextures[] = {
		"resource/UI/transition/inkSplat0.png",
		"resource/UI/transition/inkSplat1.png",
		"resource/UI/transition/inkSplat2.png",
	};
	// 数字(0〜9, +, /)を横一列に並べたテクスチャ(StageHUD と同じ)
	const std::string kDigitsTexture = "resource/UI/digits.png";
	// 壊した的の数の横に置く標的のアイコンと、最大連鎖数を入れる吹き出し(StageHUD と同じ)
	const std::string kTargetIconTexture = "resource/UI/targetIcon.png";
	const std::string kComboBubbleTexture = "resource/UI/comboBubble.png";
	// 「RANK」と、ランクの文字(S・A・B・C の順。tools/generate_clear_rank_assets.ps1 で生成する)
	const std::string kRankLabelTexture = "resource/UI/clearRank/rankLabel.png";
	const std::string kRankTextures[] = {
		"resource/UI/clearRank/rankS.png",
		"resource/UI/clearRank/rankA.png",
		"resource/UI/clearRank/rankB.png",
		"resource/UI/clearRank/rankC.png",
	};

	// 結果の行のしみの色(自分のインクの黄色と、少し濃い色。クリア画面へ切り替えるときの InkBurst と同じ色)
	constexpr Vector4 kTargetSplatColor = {1.0f, 0.85f, 0.15f, 1.0f};
	constexpr Vector4 kComboSplatColor = {0.95f, 0.62f, 0.1f, 1.0f};

	// 点滅の濃さを 0〜1 に収めるための半分の値
	constexpr float kHalf = 0.5f;
	// 進み具合の下限と上限
	constexpr float kProgressMin = 0.0f;
	constexpr float kProgressMax = 1.0f;
}

ClearResultUI::ClearResultUI() = default;
ClearResultUI::~ClearResultUI() = default;

// テクスチャの読み込みとスプライトの生成
// 位置・大きさ・角度・濃さは毎フレームの Update() で決めるため、ここでは生成と基準点・切り出す範囲の設定だけ行う
void ClearResultUI::Initialize(SpriteCommon* spriteCommon,int destroyedCount,int totalCount,int maxCombo,Rank rank){
	spriteCommon_ = spriteCommon;
	destroyedCount_ = destroyedCount;
	totalCount_ = totalCount;
	maxCombo_ = maxCombo;
	rank_ = rank;
	timer_ = 0.0f;

	TextureManager* textureManager = TextureManager::GetInstance();
	textureManager->LoadTexture(kWhiteTexture);
	textureManager->LoadTexture(kClearTitleTexture);
	for(const std::string& texture : kSplatTextures){
		textureManager->LoadTexture(texture);
	}
	textureManager->LoadTexture(kDigitsTexture);
	textureManager->LoadTexture(kTargetIconTexture);
	textureManager->LoadTexture(kComboBubbleTexture);
	textureManager->LoadTexture(kRankLabelTexture);
	// ランクの文字は、決まったランクの画像だけを読み込む
	const std::string& rankTexture = kRankTextures[static_cast<size_t>(rank_)];
	textureManager->LoadTexture(rankTexture);

	auto createSprite = [spriteCommon](const std::string& texture,const Vector2& anchor){
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon,texture);
		sprite->SetAnchorPoint(anchor);
		return sprite;
	};

	// 暗い下地は画面全体に引き伸ばし、動かさないためここで位置・大きさ・色まで決める
	backgroundSprite_ = createSprite(kWhiteTexture,kTopLeftAnchor);
	backgroundSprite_->SetPosition(kTopLeftAnchor);
	backgroundSprite_->SetSize({static_cast<float>(WinAPI::kClientWidth), static_cast<float>(WinAPI::kClientHeight)});
	backgroundSprite_->SetColor(kBackgroundColor);
	backgroundSprite_->Update();

	// クリア画面の画像から、「GAME CLEAR!」と「SPACE でタイトルへ」をそれぞれ切り出して別々に動かす
	titleSprite_ = createSprite(kClearTitleTexture,kCenterAnchor);
	titleSprite_->SetTextureLeftTop(kTitleTextureLeftTop);
	titleSprite_->SetTextureSize(kTitleTextureSize);
	hintSprite_ = createSprite(kClearTitleTexture,kCenterAnchor);
	hintSprite_->SetTextureLeftTop(kHintTextureLeftTop);
	hintSprite_->SetTextureSize(kHintTextureSize);

	for(size_t i = 0; i < kSplatCount; ++i){
		splatSprites_[i] = createSprite(kSplatTextures[i],kCenterAnchor);
	}

	targetIcon_ = createSprite(kTargetIconTexture,kCenterAnchor);
	comboBubble_ = createSprite(kComboBubbleTexture,kCenterAnchor);
	rankLabelSprite_ = createSprite(kRankLabelTexture,kCenterAnchor);
	rankSprite_ = createSprite(rankTexture,kCenterAnchor);

	digitSprites_.reserve(kMaxDigitCount);
	for(size_t i = 0; i < kMaxDigitCount; ++i){
		auto digit = createSprite(kDigitsTexture,kCenterAnchor);
		// テクスチャ全体ではなく1文字分だけを切り出して使う
		digit->SetTextureSize(kGlyphSize);
		digitSprites_.push_back(std::move(digit));
	}
}

// 表示の動きを進める
void ClearResultUI::Update(float deltaTime){
	timer_ += deltaTime;
	usedDigitCount_ = 0;

	const float screenWidth = static_cast<float>(WinAPI::kClientWidth);
	const float screenHeight = static_cast<float>(WinAPI::kClientHeight);

	// 「GAME CLEAR!」は何もない大きさから弾けるように少し大きくなりすぎてから収まる
	const float titleScale = Easing::EaseOutBack(GetProgress(kTitleStartTime,kTitleDuration));
	titleSprite_->SetPosition({screenWidth * kTitleCenterRate.x, screenHeight * kTitleCenterRate.y});
	titleSprite_->SetSize({kTitleTextureSize.x * titleScale, kTitleTextureSize.y * titleScale});
	titleSprite_->Update();

	// 壊した的の数: しみを塗ってから、標的のアイコンと「壊した数/総数」を出し、壊した数を 0 から増やす
	const Vector2 targetRowCenter = {screenWidth * kTargetRowCenterRate.x, screenHeight * kTargetRowCenterRate.y};
	UpdateSplat(splatSprites_[kTargetSplatIndex].get(),targetRowCenter,kRowSplatSize,kTargetSplatAngle,kTargetSplatColor,kTargetSplatStartTime);
	{
		const float alpha = GetProgress(kTargetCountStartTime,kContentFadeDuration);
		const float popScale = GetCountEndPopScale(kTargetCountStartTime);
		const Vector4 color = {kWhite.x, kWhite.y, kWhite.z, alpha};
		targetIcon_->SetPosition({targetRowCenter.x + kTargetIconOffsetX, targetRowCenter.y});
		targetIcon_->SetSize({kTargetIconSize * popScale, kTargetIconSize * popScale});
		targetIcon_->SetColor(color);
		targetIcon_->Update();
		const std::string text = std::to_string(GetCountUpValue(destroyedCount_,kTargetCountStartTime)) + "/" + std::to_string(totalCount_);
		LayoutText(text,{targetRowCenter.x + kTargetTextOffsetX, targetRowCenter.y},kResultCharHeight * popScale,color);
	}

	// 最大連鎖数: しみを塗ってから、吹き出しの中の「+連鎖数」を 0 から増やす
	const Vector2 comboRowCenter = {screenWidth * kComboRowCenterRate.x, screenHeight * kComboRowCenterRate.y};
	UpdateSplat(splatSprites_[kComboSplatIndex].get(),comboRowCenter,kRowSplatSize,kComboSplatAngle,kComboSplatColor,kComboSplatStartTime);
	{
		const float alpha = GetProgress(kComboCountStartTime,kContentFadeDuration);
		const float popScale = GetCountEndPopScale(kComboCountStartTime);
		const Vector4 color = {kWhite.x, kWhite.y, kWhite.z, alpha};
		comboBubble_->SetPosition(comboRowCenter);
		comboBubble_->SetSize({kComboBubbleSize.x * popScale, kComboBubbleSize.y * popScale});
		comboBubble_->SetColor(color);
		comboBubble_->Update();
		LayoutText("+" + std::to_string(GetCountUpValue(maxCombo_,kComboCountStartTime)),comboRowCenter,kResultCharHeight * popScale,color);
	}

	// ランク: ランクの色のしみを塗ってから、文字を大きい状態から押しつけるように縮めて収め、衝撃で左右に揺らす
	const Vector2 rankCenter = {screenWidth * kRankCenterRate.x, screenHeight * kRankCenterRate.y};
	UpdateSplat(splatSprites_[kRankSplatIndex].get(),rankCenter,kRankSplatSize,kRankSplatAngle,kRankColors[static_cast<size_t>(rank_)],kRankSplatStartTime);
	{
		const float stampProgress = GetProgress(kRankStampStartTime,kStampDuration);
		const float stampScale = Lerp(kStampStartScale,kProgressMax,Easing::EaseInCubic(stampProgress));
		float angle = 0.0f;
		const float wobbleTime = timer_ - (kRankStampStartTime + kStampDuration);
		if(wobbleTime > 0.0f && wobbleTime < kWobbleDuration){
			// 揺れはだんだん小さくして止める
			const float damping = kProgressMax - wobbleTime / kWobbleDuration;
			angle = std::sin(wobbleTime * kWobbleSpeed) * kWobbleAngle * damping;
		}
		rankSprite_->SetPosition(rankCenter);
		rankSprite_->SetSize({kRankSize.x * stampScale, kRankSize.y * stampScale});
		rankSprite_->SetRotation(angle);
		rankSprite_->SetColor({kWhite.x, kWhite.y, kWhite.z, stampProgress});
		rankSprite_->Update();

		// 「RANK」は押しつけ始めと同時に濃くする
		rankLabelSprite_->SetPosition({rankCenter.x, rankCenter.y + kRankLabelOffsetY});
		rankLabelSprite_->SetSize(kRankLabelSize);
		rankLabelSprite_->SetColor({kWhite.x, kWhite.y, kWhite.z, GetProgress(kRankStampStartTime,kContentFadeDuration)});
		rankLabelSprite_->Update();
	}

	// 「SPACE でタイトルへ」は、SPACE でタイトルへ戻れるようになってから点滅させる
	if(CanReturnToTitle()){
		const float blink = kHalf + kHalf * std::cos((timer_ - kReturnAcceptTime) * kHintBlinkSpeed);
		hintSprite_->SetPosition({screenWidth * kHintCenterRate.x, screenHeight * kHintCenterRate.y});
		hintSprite_->SetSize(kHintTextureSize);
		hintSprite_->SetColor({kWhite.x, kWhite.y, kWhite.z, Lerp(kHintMinAlpha,kProgressMax,blink)});
		hintSprite_->Update();
	}
}

// 途中を飛ばして、すべての結果を出し終えた状態にする
void ClearResultUI::Skip(){
	timer_ = (std::max)(timer_,kFinishTime);
}

// すべての結果を出し終えたかどうか
bool ClearResultUI::IsFinished() const{
	return timer_ >= kFinishTime;
}

// SPACE でタイトルへ戻れるかどうか
// 途中を飛ばしたときも、飛ばした時点(出し終えた時点)から少したつまでは戻れない
bool ClearResultUI::CanReturnToTitle() const{
	return timer_ >= kReturnAcceptTime;
}

// 描画処理(まだ出し始めていないものは描かない)
void ClearResultUI::Draw(){
	if(!spriteCommon_){
		return;
	}

	spriteCommon_->Draw(); // 描画前処理

	// 下地 → しみ → しみの上の中身 → 数字 の順に重ねる
	backgroundSprite_->Draw();
	if(timer_ >= kTargetSplatStartTime){
		splatSprites_[kTargetSplatIndex]->Draw();
	}
	if(timer_ >= kComboSplatStartTime){
		splatSprites_[kComboSplatIndex]->Draw();
	}
	if(timer_ >= kRankSplatStartTime){
		splatSprites_[kRankSplatIndex]->Draw();
	}
	titleSprite_->Draw();
	if(timer_ >= kTargetCountStartTime){
		targetIcon_->Draw();
	}
	if(timer_ >= kComboCountStartTime){
		comboBubble_->Draw();
	}
	if(timer_ >= kRankStampStartTime){
		rankLabelSprite_->Draw();
		rankSprite_->Draw();
	}
	for(size_t i = 0; i < usedDigitCount_; ++i){
		digitSprites_[i]->Draw();
	}
	if(CanReturnToTitle()){
		hintSprite_->Draw();
	}
}

// 結果からランクを決める
ClearResultUI::Rank ClearResultUI::CalculateRank(int destroyedCount,int totalCount,int maxCombo){
	// 的が1つも無いときは、壊した割合を求められないため最高の値として扱う
	float targetRate = kProgressMax;
	if(totalCount > 0){
		targetRate = std::clamp(static_cast<float>(destroyedCount) / static_cast<float>(totalCount),kProgressMin,kProgressMax);
	}
	// 最大連鎖数は、満点の連鎖数に届いたら最高の値にする
	const float comboRate = std::clamp(static_cast<float>(maxCombo) / static_cast<float>(kFullComboCount),kProgressMin,kProgressMax);
	const float score = targetRate * kTargetRateWeight + comboRate * kComboRateWeight;

	// 良いランクから順に、必要な値に届いているかを調べる
	for(size_t i = 0; i < kRankThresholds.size(); ++i){
		if(score >= kRankThresholds[i]){
			return static_cast<Rank>(i);
		}
	}
	return Rank::C;
}

// 出し始めの時間から、指定した時間をかけて進む割合(0〜1)を求める
float ClearResultUI::GetProgress(float startTime,float duration) const{
	return std::clamp((timer_ - startTime) / duration,kProgressMin,kProgressMax);
}

// インクのしみを、出し始めの時間から弾けるように広げて塗る(勢いで少し大きくなりすぎてから収まる)
void ClearResultUI::UpdateSplat(Sprite* sprite,const Vector2& center,const Vector2& size,float angle,const Vector4& color,float startTime){
	const float scale = Easing::EaseOutBack(GetProgress(startTime,kSplatDuration));
	sprite->SetPosition(center);
	sprite->SetRotation(angle);
	sprite->SetSize({size.x * scale, size.y * scale});
	sprite->SetColor(color);
	sprite->Update();
}

// 0 から目標の数まで、出し始めの時間からだんだん増やした今の数を求める
// 最初は速く回し、目標に近づくほどゆっくり増やして、止まる瞬間を見せる
int ClearResultUI::GetCountUpValue(int targetValue,float startTime) const{
	const float rate = Easing::EaseOutCubic(GetProgress(startTime,kCountUpDuration));
	return static_cast<int>(std::round(static_cast<float>(targetValue) * rate));
}

// 数え終わった瞬間に大きくして、だんだん元の大きさに戻す倍率を求める
float ClearResultUI::GetCountEndPopScale(float startTime) const{
	const float countEndTime = startTime + kCountUpDuration;
	if(timer_ < countEndTime){
		return kProgressMax;
	}
	const float popProgress = GetProgress(countEndTime,kCountEndPopDuration);
	return kProgressMax + kCountEndPopScale * (kProgressMax - Easing::EaseOutCubic(popProgress));
}

// 数字の文字列を、数字用スプライトを並べて配置する
void ClearResultUI::LayoutText(const std::string& text,const Vector2& center,float charHeight,const Vector4& color){
	const float charWidth = charHeight * (kGlyphSize.x / kGlyphSize.y);
	const float advance = charWidth * kCharSpacingRate;
	// 文字列全体の幅から、1文字目の中心の位置を求める
	const float totalWidth = advance * static_cast<float>(text.size() - 1);
	float x = center.x - totalWidth * kCenterAnchor.x;

	for(char c : text){
		if(usedDigitCount_ >= digitSprites_.size()){
			return;
		}

		// 文字からテクスチャ上の番号を求める(0〜9 の後に + と / が並んでいる)
		int glyphIndex = 0;
		if(c == '+'){
			glyphIndex = kGlyphPlusIndex;
		} else if(c == '/'){
			glyphIndex = kGlyphSlashIndex;
		} else{
			glyphIndex = c - '0';
		}

		Sprite* sprite = digitSprites_[usedDigitCount_].get();
		sprite->SetTextureLeftTop({kGlyphSize.x * static_cast<float>(glyphIndex), 0.0f});
		sprite->SetPosition({x, center.y});
		sprite->SetSize({charWidth, charHeight});
		sprite->SetColor(color);
		sprite->Update();

		++usedDigitCount_;
		x += advance;
	}
}
