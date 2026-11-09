#include "TutorialHints.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include "Easing.h"
#include <algorithm>
#include <string>

namespace{
	// tools/generate_tutorial_hints.py で生成した画像(並びは TutorialHints::Step と同じ)
	const std::string kHintTextures[] = {
		"resource/UI/hintMove.png",
		"resource/UI/hintShoot.png",
		"resource/UI/hintGate.png",
		"resource/UI/hintRailShoot.png",
		"resource/UI/hintKeyTarget.png",
	};
	// 白(画像の色をそのまま出す)。α値だけ変えて使う
	constexpr Vector4 kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
}

TutorialHints::TutorialHints() = default;
TutorialHints::~TutorialHints() = default;

// テクスチャの読み込みとスプライトの生成
// 位置・濃さは毎フレームのUpdate()で決めるため、ここでは生成と大きさ・基準点の設定だけ行う
void TutorialHints::Initialize(SpriteCommon* spriteCommon){
	static_assert(std::size(kHintTextures) == kHintCount,"案内の画像の数と段階の数が合っていない");
	spriteCommon_ = spriteCommon;

	for(int i = 0; i < kHintCount; ++i){
		TextureManager::GetInstance()->LoadTexture(kHintTextures[i]);
		sprites_[i] = std::make_unique<Sprite>();
		sprites_[i]->Initialize(spriteCommon,kHintTextures[i]);
		sprites_[i]->SetSize(kSize);
		sprites_[i]->SetAnchorPoint(kCenterAnchor);
	}
}

// 最初の案内から出し直す
void TutorialHints::Reset(){
	step_ = Step::Move;
	shownTimer_ = 0.0f;
	hasMoveStarted_ = false;
	shotCount_ = 0;
	alpha_ = 0.0f;
	fadingHintIndex_ = kNoHint;
	fadingAlpha_ = 0.0f;
	nextDelayTimer_ = 0.0f;
}

// 今の段階の案内を出す場面か
// 台の上の案内は台の上を動ける間、レールの案内はレールを進んでいる間だけ出す
bool TutorialHints::IsShowingScene(const Situation& situation) const{
	switch(step_){
	case Step::Move:
	case Step::Shoot:
	case Step::Gate:
		return situation.isOnPlatform;
	case Step::RailShoot:
		return situation.isRiding;
	case Step::KeyTarget:
		// 鍵の的が近くにある間だけ出し、何のための的か分かるようにする
		return situation.isRiding && situation.isKeyTargetNear;
	case Step::Done:
	default:
		return false;
	}
}

// 今の段階の操作をしたか
bool TutorialHints::IsStepCompleted(const Situation& situation) const{
	switch(step_){
	case Step::Move:
	{
		// 出し始めた位置から水平方向に一定の距離を歩いたら消す
		const Vector3 moved = {situation.playerPosition.x - moveStartPosition_.x, 0.0f, situation.playerPosition.z - moveStartPosition_.z};
		return hasMoveStarted_ && Length(moved) >= kMoveDoneDistance;
	}
	case Step::Shoot:
		return shotCount_ >= kShootDoneCount;
	case Step::Gate:
		return situation.hasOpenedGate;
	case Step::RailShoot:
		// 的を1つ壊すか、しばらく出したら消す(的が少ない区間でも出しっぱなしにしない)
		return situation.hasDestroyedTarget || shownTimer_ >= kRailShootMaxDuration;
	case Step::KeyTarget:
		return situation.hasDestroyedKeyTarget;
	case Step::Done:
	default:
		return false;
	}
}

// 指定した段階へ進める
// 出していた案内はその濃さから薄くして消し、少し間を置いてから次の案内を出す
void TutorialHints::AdvanceTo(Step step){
	if(step_ != Step::Done && alpha_ > 0.0f){
		fadingHintIndex_ = static_cast<int>(step_);
		fadingAlpha_ = alpha_;
	}
	step_ = step;
	shownTimer_ = 0.0f;
	alpha_ = 0.0f;
	nextDelayTimer_ = kNextHintDelay;
}

// 案内を進め、表示の動きを進める
void TutorialHints::Update(const Situation& situation,float deltaTime){
	// 消えていく案内を薄くする
	if(fadingHintIndex_ != kNoHint){
		fadingAlpha_ -= deltaTime / kFadeOutDuration;
		if(fadingAlpha_ <= 0.0f){
			fadingHintIndex_ = kNoHint;
		}
	}

	// 先の操作を先にしてしまったときは、そこまでの案内を飛ばす(案内を見る前にゲートを開けた・レールに乗ったなど)
	if(step_ < Step::RailShoot && (situation.hasOpenedGate || situation.isRiding)){
		AdvanceTo(Step::RailShoot);
	}
	if(step_ == Step::Done){
		return;
	}

	// 前の案内が消えるのを待ってから出す。出す場面でないときは薄くして待つ
	nextDelayTimer_ = (std::max)(nextDelayTimer_ - deltaTime,0.0f);
	const bool isShown = IsShowingScene(situation) && nextDelayTimer_ <= 0.0f;
	if(!isShown){
		alpha_ = (std::max)(alpha_ - deltaTime / kFadeOutDuration,0.0f);
		return;
	}

	// 移動の案内は、出し始めた瞬間の位置から歩いた距離を測る
	if(step_ == Step::Move && !hasMoveStarted_){
		hasMoveStarted_ = true;
		moveStartPosition_ = situation.playerPosition;
	}
	// 撃った回数は、試し撃ちの案内を出している間だけ数える(案内を見てから撃ったことを確かめる)
	if(step_ == Step::Shoot && situation.hasFired){
		++shotCount_;
	}
	shownTimer_ += deltaTime;
	alpha_ = (std::min)(alpha_ + deltaTime / kFadeInDuration,1.0f);

	if(IsStepCompleted(situation)){
		AdvanceTo(static_cast<Step>(static_cast<int>(step_) + 1));
	}
}

// 描画処理(今の案内と、消えていく前の案内を描く)
// 出し始めは少し上から下りてくるように動かす
void TutorialHints::Draw(){
	if(!spriteCommon_){
		return;
	}
	const bool hasCurrent = step_ != Step::Done && alpha_ > 0.0f;
	if(!hasCurrent && fadingHintIndex_ == kNoHint){
		return;
	}

	const float screenWidth = static_cast<float>(WinAPI::kClientWidth);
	const float screenHeight = static_cast<float>(WinAPI::kClientHeight);
	const Vector2 basePosition = {screenWidth * kCenterXRate, screenHeight * kHeightRate};

	spriteCommon_->Draw(); // 描画前処理
	if(fadingHintIndex_ != kNoHint){
		Sprite& sprite = *sprites_[fadingHintIndex_];
		sprite.SetPosition(basePosition);
		sprite.SetColor({kWhite.x, kWhite.y, kWhite.z, fadingAlpha_});
		sprite.Update();
		sprite.Draw();
	}
	if(hasCurrent){
		const float dropProgress = std::clamp(shownTimer_ / kDropDuration,0.0f,1.0f);
		const float drop = kDropDistance * (1.0f - Easing::EaseOutCubic(dropProgress));
		Sprite& sprite = *sprites_[static_cast<int>(step_)];
		sprite.SetPosition({basePosition.x, basePosition.y - drop});
		sprite.SetColor({kWhite.x, kWhite.y, kWhite.z, alpha_});
		sprite.Update();
		sprite.Draw();
	}
}
