#include "PlayerState.h"
#include "Input.h"
#include "Editor/RailEditor.h"
#include "objects/StartPlatform.h"
#include "Easing.h"
#include <cmath>

// -------------------------------------------------
// レールの上を走っている状態
// -------------------------------------------------

// オンレール中はWASDで自由に動かさず、レールに沿った進行(Player::UpdateRailProgress)を続けたまま、
// その場でのジャンプと並走レールへの乗り換えだけを行う(参考にしたスプラトゥーンのレールと同じく、レール上では射撃に専念させる)
std::unique_ptr<Player::State> Player::RailRunState::UpdateMovement(Player& owner,const MoveContext& context){
	return HandleJumpInput(owner,context);
}

// 足がレールに着いているときのジャンプキー(SPACE)の受け付け
// タイトルのスタートもSPACEだが、押した瞬間はまだプレイ可能になっていないためジャンプしない
std::unique_ptr<Player::State> Player::RailRunState::HandleJumpInput(Player& owner,const MoveContext& context){
	if(!context.input->TriggerKey(DIK_SPACE)){
		return nullptr;
	}

	// A/Dを押しながらジャンプしたときは、その方向に並走しているレールがあれば飛び移る
	Vector3 rightXZ = Normalize(Vector3{context.cameraRight.x, 0.0f, context.cameraRight.z});
	Vector3 sideDir = {0.0f, 0.0f, 0.0f};
	if(context.input->PushKey(DIK_D)) sideDir += rightXZ;
	if(context.input->PushKey(DIK_A)) sideDir += -rightXZ;

	// ループするレールの上では、輪の中心(ボス)を見ていると隣の輪が左右ではなく前後に来るため、W/Sでも飛び移れるようにする
	if(context.railEditor->IsLoopRail(context.railEditor->GetActiveRailIndex())){
		Vector3 forwardXZ = Normalize(Vector3{context.cameraForward.x, 0.0f, context.cameraForward.z});
		if(context.input->PushKey(DIK_W)) sideDir += forwardXZ;
		if(context.input->PushKey(DIK_S)) sideDir += -forwardXZ;
	}

	// A/D同時押しで打ち消し合ったときや、押した方向にレールがないときは、その場で跳ねるジャンプにする
	if(Length(sideDir) > 0.0f){
		if(std::unique_ptr<State> switchState = TrySwitchToSideRail(owner,context.railEditor,Normalize(sideDir))){
			return switchState;
		}
	}

	// レールからは離れず、レールに沿って進みながら上へ跳ねる
	return std::make_unique<RailHopState>();
}

// 押した方向のすぐ横を並走しているレールへ、飛び移る動きを付けて乗り換える
// 今のレール以外で最も近いレールを探し、「近い」「押した方向の真横にある」「高さが近い」をすべて満たすときだけ乗り換える
// 乗り換え自体はすぐに行い、見た目の位置だけを乗り換え前の位置から乗り換え先へ山なりに寄せていく
std::unique_ptr<Player::State> Player::RailRunState::TrySwitchToSideRail(Player& owner,RailEditor* railEditor,const Vector3& sideDirXZ){
	RailEditor::NearestRailResult nearest;
	Vector3 toRailXZ;
	if(!owner.FindSideRailInReach(railEditor,nearest,toRailXZ)){
		return nullptr;
	}

	// 押した方向の真横にあるか(前後にずれた位置のレールには乗り換えない)
	if(Dot(Normalize(toRailXZ),sideDirXZ) < kSideRailDirectionThreshold){
		return nullptr;
	}

	// 乗り換え先レールの最近傍点からそのまま進行を続ける
	// 飛び移っている間も乗り換え先レールに沿って進めることで、勢いを保ったまま横へ移る動きにする
	railEditor->SwitchActiveRail(nearest.railIndex);
	owner.railT_ = nearest.t;
	owner.isRailFinished_ = false; // 乗り換え先レールを最後まで進めるようにする

	// 乗り換え前の位置(足がレールに着いているときだけ乗り換えるため、基準位置がそのままレール上の位置)からのずれを覚えて、飛び移る動きを始める
	return std::make_unique<RailSwitchState>(owner.basePosition_ - nearest.position);
}

// -------------------------------------------------
// レールの上で跳ねている状態
// -------------------------------------------------

// 重力を適用してY方向の速度を更新し、跳ねている高さに反映する
// レールに沿った進行はPlayer::UpdateRailProgressで続けているため、ここでは高さだけを扱う
std::unique_ptr<Player::State> Player::RailHopState::UpdateMovement(Player& owner,const MoveContext& context){
	hopVelocityY_ -= kGravity * context.deltaTime;
	hopHeight_ += hopVelocityY_ * context.deltaTime;

	// レールの高さまで戻るまでは跳ね続ける(跳ねている間は次のジャンプを受け付けない)
	if(hopHeight_ > 0.0f){
		return nullptr;
	}

	// レールの高さまで戻ったら着地させる(レールから外れることはないため、必ず同じレールに着地する)
	// 着地したフレームからジャンプを受け付ける
	std::unique_ptr<State> jumpState = RailRunState::HandleJumpInput(owner,context);
	return jumpState?std::move(jumpState):std::make_unique<RailRunState>();
}

// -------------------------------------------------
// 並走レールへ飛び移っている状態
// -------------------------------------------------

// 決めた時間が経ったら乗り換え先レールの上に着地させる(このときずれと山なりの高さはちょうど0になる)
std::unique_ptr<Player::State> Player::RailSwitchState::UpdateMovement(Player& owner,const MoveContext& context){
	timer_ += context.deltaTime;
	if(timer_ < kRailSwitchDuration){
		// 飛び移っている間は次のジャンプを受け付けない
		return nullptr;
	}
	timer_ = kRailSwitchDuration;

	// 着地したフレームからジャンプを受け付ける
	std::unique_ptr<State> jumpState = RailRunState::HandleJumpInput(owner,context);
	return jumpState?std::move(jumpState):std::make_unique<RailRunState>();
}

// 乗り換え先レールの上を進みながら、乗り換え前の位置からのずれを減らしていき乗り換え先へ寄せる
Vector3 Player::RailSwitchState::GetRailOffset() const{
	const float progress = timer_ / kRailSwitchDuration;

	// 勢いよく飛び出して乗り換え先の上でゆっくり収まるよう、減速するイージングを使う
	Vector3 offset = startOffset_ * (1.0f - Easing::EaseOutCubic(progress));

	// 飛び移っている間は山なりに浮かせる(始まりと終わりで0、真ん中で最も高くなる)
	offset.y += kRailSwitchArcHeight * kParabolaPeakScale * progress * (1.0f - progress);
	return offset;
}

// -------------------------------------------------
// レールの外にいる状態の共通部分
// -------------------------------------------------

// WASDでの水平移動と重力・ジャンプ初速による垂直移動を行い、レール座標によるオンレール判定を使って着地先レールへ乗る
Player::OffRailState::FreeMoveResult Player::OffRailState::UpdateFreeMovement(Player& owner,const MoveContext& context,bool isStandingOnPlatform){
	const float deltaTime = context.deltaTime;

	// 移動前の高さを覚えておく(この後の着地判定でレールの高さを跨いだ瞬間を検出するのに使う)
	const float previousPositionY = owner.freePosition_.y;

	// オフレール中はカメラ向き基準(XZ平面)でWASD移動する
	Vector3 forwardXZ = Normalize(Vector3{context.cameraForward.x, 0.0f, context.cameraForward.z});
	Vector3 rightXZ = Normalize(Vector3{context.cameraRight.x, 0.0f, context.cameraRight.z});

	Vector3 moveDir = {0.0f, 0.0f, 0.0f};
	if(context.input->PushKey(DIK_W)) moveDir += forwardXZ;
	if(context.input->PushKey(DIK_S)) moveDir += -forwardXZ;
	if(context.input->PushKey(DIK_D)) moveDir += rightXZ;
	if(context.input->PushKey(DIK_A)) moveDir += -rightXZ;
	moveDir = Normalize(moveDir);

	owner.freePosition_ += moveDir * kMoveSpeed * deltaTime;
	// レールの終点から飛び出した勢いで、着地するか落ちきるまで前へ進み続ける
	owner.freePosition_ += owner.freeVelocityXZ_ * deltaTime;

	// スタート台の出発ゲートが閉じている間は、ゲートより先へ進めない(ジャンプで飛び越えることもできない)
	const StartPlatform* startPlatform = context.startPlatform;
	if(startPlatform){
		owner.freePosition_ = startPlatform->ClampBehindClosedGate(owner.freePosition_);

		// 台の上に立って歩いている間は、端から落ちないよう台の範囲内に押し戻す
		// ジャンプ中は押し戻さないため、自分から飛び降りることはできる
		if(isStandingOnPlatform){
			owner.freePosition_ = startPlatform->ClampInsideTop(owner.freePosition_);
		}
	}

	// 重力を適用してY方向の速度を更新し、位置に反映する
	owner.freeVelocityY_ -= kGravity * deltaTime;
	owner.freePosition_.y += owner.freeVelocityY_ * deltaTime;

	// 落下中のみ着地判定を行う(上昇中に離脱直後の位置へ即座に再着地しないようにする)
	if(owner.freeVelocityY_ > 0.0f){
		return FreeMoveResult::Airborne;
	}

	// スタート台の上面を落下で跨いだら、上面に立たせる
	// 台の上を歩いている間も重力で少し沈むため、毎フレームここで上面に戻して立たせ続ける
	// 台の端から外へ出たときは範囲外になり、そのまま落ちる
	bool isOnPlatform = false;
	if(startPlatform && startPlatform->IsInsideTop(owner.freePosition_)){
		const float topHeight = startPlatform->GetTopHeight();
		if(previousPositionY >= topHeight && owner.freePosition_.y <= topHeight){
			owner.freePosition_.y = topHeight;
			owner.freeVelocityY_ = 0.0f;
			owner.freeVelocityXZ_ = {0.0f, 0.0f, 0.0f};
			isOnPlatform = true;
		}
	}

	// 終点で外れて落ちている間は、外れたレールには着地させない
	RailEditor* railEditor = context.railEditor;
	RailEditor::NearestRailResult nearest = railEditor->FindNearestRail(owner.freePosition_,owner.endedRailIndex_);
	// 距離1つ(球状の判定)だとレールの横や上にいるだけで乗ってしまうため、
	// 「水平方向でレールの真上にいる」ことと「落下でレールの高さを跨いだ」ことの両方を条件にする
	if(nearest.railIndex >= 0){
		// 水平方向(XZ平面)だけで見た、レール最近傍点までの距離
		Vector3 horizontalDiff = {owner.freePosition_.x - nearest.position.x, 0.0f, owner.freePosition_.z - nearest.position.z};
		bool isAboveRail = Length(horizontalDiff) <= kOnRailHorizontalThreshold;

		// 前フレームはレールより上にいて、今フレームでレールの高さ以下まで落ちた瞬間だけ着地とみなす
		bool crossedRailHeight = (previousPositionY > nearest.position.y) && (owner.freePosition_.y <= nearest.position.y);

		// スタート台の上ではレールと同じ高さを歩くため高さを跨がない。台と重なったレールの真上まで歩いたら乗せる
		bool walkedOntoRail = isOnPlatform && std::abs(owner.freePosition_.y - nearest.position.y) <= kPlatformBoardHeightRange;

		if(isAboveRail && (crossedRailHeight || walkedOntoRail)){
			railEditor->SwitchActiveRail(nearest.railIndex);
			owner.railT_ = nearest.t;
			owner.isRailFinished_ = false; // 着地先レールを最後まで進めるようにする
			owner.endedRailIndex_ = -1;
			owner.freeVelocityY_ = 0.0f;
			owner.freeVelocityXZ_ = {0.0f, 0.0f, 0.0f};
			return FreeMoveResult::OnRail;
		}
	}

	return isOnPlatform?FreeMoveResult::OnPlatform:FreeMoveResult::Airborne;
}

// -------------------------------------------------
// スタート台の上を歩いている状態
// -------------------------------------------------

// スタート台の上に立っている間は、レール上と同じくSPACEでジャンプする
std::unique_ptr<Player::State> Player::PlatformState::UpdateMovement(Player& owner,const MoveContext& context){
	bool isStanding = true;
	if(context.input->TriggerKey(DIK_SPACE)){
		isStanding = false;
		owner.freeVelocityY_ = kJumpSpeed;
	}

	switch(UpdateFreeMovement(owner,context,isStanding)){
	case FreeMoveResult::OnRail:
		return std::make_unique<RailRunState>();
	case FreeMoveResult::Airborne:
		return std::make_unique<FallState>();
	case FreeMoveResult::OnPlatform:
	default:
		return nullptr;
	}
}

// -------------------------------------------------
// ジャンプ・落下している状態
// -------------------------------------------------

// 空中では台の端で押し戻さず、スタート台の上面かレールに着地したらその状態へ移る
std::unique_ptr<Player::State> Player::FallState::UpdateMovement(Player& owner,const MoveContext& context){
	switch(UpdateFreeMovement(owner,context,false)){
	case FreeMoveResult::OnRail:
		return std::make_unique<RailRunState>();
	case FreeMoveResult::OnPlatform:
		return std::make_unique<PlatformState>();
	case FreeMoveResult::Airborne:
	default:
		return nullptr;
	}
}
