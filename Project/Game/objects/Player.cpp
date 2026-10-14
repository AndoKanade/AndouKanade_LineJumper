#include "Player.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "ParticleManager.h"
#include "GlobalVariables.h"
#include "Input.h"
#include "Editor/RailEditor.h"
#include "objects/StartPlatform.h"
#include "Easing.h"

namespace{
	// プレイヤーの表示に使用するモデル(インクタンクを背負い、ブラスターを構えたキャラクター)
	const std::string kPlayerModelPath = "Player/player.obj";

	// 体の色(参考にしたスプラトゥーンの映像に合わせ、自分のインクと同じ黄色にする)
	constexpr Vector4 kPlayerColor = {1.0f, 0.85f, 0.15f, 1.0f};
}

Player::Player() = default;
Player::~Player() = default;

// 初期化処理
void Player::Initialize(Obj3dCommon* objCommon,const std::string& paramGroup){
	paramGroup_ = paramGroup;

	// プレイヤーのモデルの読み込みと生成
	ModelManager::GetInstance()->LoadModel(kPlayerModelPath);
	obj_ = std::make_unique<Obj3D>();
	obj_->Initialize(objCommon);
	obj_->SetModel(kPlayerModelPath);

	// 体の色は生成時に一度設定するだけでよいため、ここで黄色にしておく
	if(Model::Material* material = obj_->GetMaterial()){
		material->color = kPlayerColor;
	}

	// 調整項目の登録(第3引数はデフォルト値。保存済みJSONがあればそちらが優先される)
	GlobalVariables::GetInstance()->AddItem(paramGroup_,"railSpeed",kDefaultRailSpeed);
}

// レール先頭・オンレール・体力満タンの初期状態に戻す
void Player::Reset(){
	railT_ = 0.0f;
	isRailFinished_ = false; // レール終端フラグもリセットする

	// オンレール/オフレールの状態もリセットし、必ずオンレールから開始する
	isOnRail_ = true;
	freeVelocityY_ = 0.0f;
	isOnPlatform_ = false;

	// レール上でのジャンプ・並走レールへの乗り換えの途中状態も消す
	isRailHopping_ = false;
	railHopHeight_ = 0.0f;
	railHopVelocityY_ = 0.0f;
	isSwitchingRail_ = false;
	railSwitchTimer_ = 0.0f;

	// 体力・無敵時間も初期状態に戻す
	hp_ = kMaxHp;
	invincibleTimer_ = 0.0f;
}

// スタート台の上に立たせ、オフレールの状態から始める
// 台の上ではWASDで自由に動け、台と重なったレールの始点へ歩いて乗るとレールを進み始める
void Player::StartOnPlatform(const Vector3& startPosition,const Vector3& startRotation){
	isOnRail_ = false;
	isOnPlatform_ = true;
	freePosition_ = startPosition;
	freeVelocityY_ = 0.0f;
	// カメラはレールの始点の向きを正面にする
	freeBaseRotation_ = startRotation;
}

// レールの進行
// Edit中・開始演出中・オフレール中はその位置で静止させる
// 終端に到達したらループさせず、その場で停止させる
void Player::UpdateRailProgress(const RailEditor* railEditor,bool isGameplayActive,float deltaTime){
	if(!railEditor || !isGameplayActive || !isOnRail_ || isRailFinished_){
		return;
	}

	// 調整項目から最新の値を取得(ImGui編集/ホットリロードが即反映される)
	float railSpeed = GlobalVariables::GetInstance()->GetFloatValue(paramGroup_,"railSpeed");

	// 現在位置に対応する制御点のSpeed値を取得し、全体速度(railSpeed)に掛けて今フレームに進む距離を求める
	// 進行度に直接足すと制御点間の距離で速さが変わるため、距離として進めて一定の速さにする
	float pointSpeed = railEditor->GetSpeedOnRail(railT_);

	// レール上で跳ねている間・並走レールへ飛び移っている間は、空中で前へ流されすぎないよう進む速さを落とす
	if(!IsGroundedOnRail()){
		railSpeed *= kRailAirborneSpeedRate;
	}

	railT_ = railEditor->AdvanceTByDistance(railT_,pointSpeed * railSpeed * deltaTime);
	if(railT_ >= kRailEndT){
		railT_ = kRailEndT; // 終端で固定する
		isRailFinished_ = true;
	}
}

// 基準位置・基準向きの計算
// オフレール中は、直前フレームで更新した自由移動座標(freePosition_)とジャンプ時に固定した向き(freeBaseRotation_)を基準にする
// オンレール中にジャンプ・並走レールへの乗り換えをしている間は、レール上の位置に浮き上がりとずれを加える
void Player::UpdateBasePose(const RailEditor* railEditor){
	if(!railEditor){
		return;
	}

	Vector3 railPos = railEditor->GetPositionOnRail(railT_);
	Vector3 railRot = railEditor->GetRotationOnRail(railT_);

	// レール上で跳ねている高さを加える(レールの坂に合わせて上下しつつ跳ねる)
	railPos.y += railHopHeight_;

	if(isSwitchingRail_){
		const float progress = railSwitchTimer_ / kRailSwitchDuration;

		// 乗り換え先レールの上を進みながら、乗り換え前の位置からのずれを減らしていき乗り換え先へ寄せる
		// 勢いよく飛び出して乗り換え先の上でゆっくり収まるよう、減速するイージングを使う
		railPos += railSwitchStartOffset_ * (1.0f - Easing::EaseOutCubic(progress));

		// 飛び移っている間は山なりに浮かせる(始まりと終わりで0、真ん中で最も高くなる)
		railPos.y += kRailSwitchArcHeight * kParabolaPeakScale * progress * (1.0f - progress);
	}

	basePosition_ = isOnRail_?railPos:freePosition_;
	baseRotation_ = isOnRail_?railRot:freeBaseRotation_;
}

// 表示位置の計算とモデルの行列更新
// プレイヤーはレールの上(基準位置)に立たせ、体は照準の方向(カメラの左右の向き)へ向ける
// レールの傾きに合わせて体を倒すと照準とずれて見えるため、体は常に直立させる
void Player::UpdateTransform(const Vector3& cameraRotation,float hopHeight){
	position_ = basePosition_;

	if(!obj_){
		return;
	}

	// タイトル中の跳ねは見た目だけに反映し、カメラ・ロゴ・敵が基準にする position_ は動かさない
	obj_->SetTranslate({position_.x, position_.y + hopHeight, position_.z});
	obj_->SetRotate({0.0f, cameraRotation.y, 0.0f});
	obj_->SetScale({kScale, kScale, kScale}); // 小さめのスケールで表示
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		obj_->SetCamera(activeCamera);
	}
	obj_->Update();
}

// 被弾後の無敵時間を進める
void Player::UpdateInvincible(float deltaTime){
	if(invincibleTimer_ > 0.0f){
		invincibleTimer_ -= deltaTime;
		if(invincibleTimer_ < 0.0f){
			invincibleTimer_ = 0.0f;
		}
	}
}

// 敵弾が当たったときの処理
// 連続被弾で一瞬に体力が尽きないよう、被弾後は一定時間無敵にする
void Player::TakeDamage(){
	if(invincibleTimer_ > 0.0f || hp_ <= 0){
		return;
	}

	--hp_;
	invincibleTimer_ = kInvincibleTime;

	// 被弾位置に火花パーティクルを発生させ、当たったことを見た目で分かるようにする
	ParticleManager::GetInstance()->EmitSpark(position_);
}

// プレイヤーの自立(ジャンプ+WASD移動)
// オンレール中はWASDで自由に動かさず、レールに沿った進行(UpdateRailProgress)を続けたまま、
// その場でのジャンプと並走レールへの乗り換えだけを行う(参考にしたスプラトゥーンのレールと同じく、レール上では射撃に専念させる)
// オフレール中(スタート台の上やそこから飛び降りた後)はWASDでの水平移動と重力・ジャンプ初速による垂直移動を行い、
// レール座標によるオンレール判定を使って着地先レールへ乗る
bool Player::UpdateMovement(Input* input,RailEditor* railEditor,const StartPlatform* startPlatform,const Vector3& cameraForward,const Vector3& cameraRight,float groundHeight,float deltaTime){
	if(!input || !railEditor){
		return false;
	}

	if(isOnRail_){
		// 跳ねている高さ・飛び移りの経過時間を進める
		UpdateRailJump(deltaTime);

		// 足がレールに着いているときだけジャンプキー(SPACE)を受け付ける(空中での連続ジャンプ・飛び移り中の再入力を防ぐ)
		// タイトルのスタートもSPACEだが、押した瞬間はまだプレイ可能になっていないためジャンプしない
		if(!isRailHopping_ && !isSwitchingRail_ && input->TriggerKey(DIK_SPACE)){
			// A/Dを押しながらジャンプしたときは、その方向に並走しているレールがあれば飛び移る
			Vector3 rightXZ = Normalize(Vector3{cameraRight.x, 0.0f, cameraRight.z});
			Vector3 sideDir = {0.0f, 0.0f, 0.0f};
			if(input->PushKey(DIK_D)) sideDir += rightXZ;
			if(input->PushKey(DIK_A)) sideDir += -rightXZ;

			// A/D同時押しで打ち消し合ったときや、押した方向にレールがないときは、その場で跳ねるジャンプにする
			if(Length(sideDir) > 0.0f && TrySwitchToSideRail(railEditor,sideDir)){
				return false;
			}

			// レールからは離れず、レールに沿って進みながら上へ跳ねる
			isRailHopping_ = true;
			railHopVelocityY_ = kJumpSpeed;
		}
		return false;
	}

	// 移動前の高さを覚えておく(この後の着地判定でレールの高さを跨いだ瞬間を検出するのに使う)
	const float previousPositionY = freePosition_.y;

	// スタート台の上に立っている間は、レール上と同じくSPACEでジャンプする
	if(isOnPlatform_ && input->TriggerKey(DIK_SPACE)){
		isOnPlatform_ = false;
		freeVelocityY_ = kJumpSpeed;
	}

	// オフレール中はカメラ向き基準(XZ平面)でWASD移動する
	Vector3 forwardXZ = Normalize(Vector3{cameraForward.x, 0.0f, cameraForward.z});
	Vector3 rightXZ = Normalize(Vector3{cameraRight.x, 0.0f, cameraRight.z});

	Vector3 moveDir = {0.0f, 0.0f, 0.0f};
	if(input->PushKey(DIK_W)) moveDir += forwardXZ;
	if(input->PushKey(DIK_S)) moveDir += -forwardXZ;
	if(input->PushKey(DIK_D)) moveDir += rightXZ;
	if(input->PushKey(DIK_A)) moveDir += -rightXZ;
	moveDir = Normalize(moveDir);

	freePosition_ += moveDir * kMoveSpeed * deltaTime;

	// スタート台の出発ゲートが閉じている間は、ゲートより先へ進めない(ジャンプで飛び越えることもできない)
	if(startPlatform){
		freePosition_ = startPlatform->ClampBehindClosedGate(freePosition_);

		// 台の上に立って歩いている間は、端から落ちないよう台の範囲内に押し戻す
		// ジャンプ中は押し戻さないため、自分から飛び降りることはできる
		if(isOnPlatform_){
			freePosition_ = startPlatform->ClampInsideTop(freePosition_);
		}
	}

	// 重力を適用してY方向の速度を更新し、位置に反映する
	freeVelocityY_ -= kGravity * deltaTime;
	freePosition_.y += freeVelocityY_ * deltaTime;

	// 落下中のみ着地判定を行う(上昇中に離脱直後の位置へ即座に再着地しないようにする)
	if(freeVelocityY_ > 0.0f){
		return false;
	}

	// スタート台の上面を落下で跨いだら、上面に立たせる
	// 台の上を歩いている間も重力で少し沈むため、毎フレームここで上面に戻して立たせ続ける
	// 台の端から外へ出たときは範囲外になり、そのまま落ちる
	isOnPlatform_ = false;
	if(startPlatform && startPlatform->IsInsideTop(freePosition_)){
		const float topHeight = startPlatform->GetTopHeight();
		if(previousPositionY >= topHeight && freePosition_.y <= topHeight){
			freePosition_.y = topHeight;
			freeVelocityY_ = 0.0f;
			isOnPlatform_ = true;
		}
	}

	RailEditor::NearestRailResult nearest = railEditor->FindNearestRail(freePosition_);
	// 距離1つ(球状の判定)だとレールの横や上にいるだけで乗ってしまうため、
	// 「水平方向でレールの真上にいる」ことと「落下でレールの高さを跨いだ」ことの両方を条件にする
	if(nearest.railIndex >= 0){
		// 水平方向(XZ平面)だけで見た、レール最近傍点までの距離
		Vector3 horizontalDiff = {freePosition_.x - nearest.position.x, 0.0f, freePosition_.z - nearest.position.z};
		bool isAboveRail = Length(horizontalDiff) <= kOnRailHorizontalThreshold;

		// 前フレームはレールより上にいて、今フレームでレールの高さ以下まで落ちた瞬間だけ着地とみなす
		bool crossedRailHeight = (previousPositionY > nearest.position.y) && (freePosition_.y <= nearest.position.y);

		// スタート台の上ではレールと同じ高さを歩くため高さを跨がない。台と重なったレールの真上まで歩いたら乗せる
		bool walkedOntoRail = isOnPlatform_ && std::abs(freePosition_.y - nearest.position.y) <= kPlatformBoardHeightRange;

		if(isAboveRail && (crossedRailHeight || walkedOntoRail)){
			railEditor->SwitchActiveRail(nearest.railIndex);
			railT_ = nearest.t;
			isRailFinished_ = false; // 着地先レールを最後まで進めるようにする
			isOnRail_ = true;
			isOnPlatform_ = false;
			freeVelocityY_ = 0.0f;
		}
	}

	// レールから落ちたときのリスタート
	// どのレールにも乗れないまま、地面の高さまで落ちたらリスタートが必要であることを呼び出し側へ伝える
	return !isOnRail_ && freePosition_.y <= groundHeight;
}

// 押した方向のすぐ横を並走しているレールへ、飛び移る動きを付けて乗り換える
// 今のレール以外で最も近いレールを探し、「近い」「押した方向の真横にある」「高さが近い」をすべて満たすときだけ乗り換える
// 乗り換え自体はすぐに行い、見た目の位置だけを乗り換え前の位置から乗り換え先へ山なりに寄せていく
bool Player::TrySwitchToSideRail(RailEditor* railEditor,const Vector3& sideDirXZ){
	RailEditor::NearestRailResult nearest = railEditor->FindNearestRail(basePosition_,railEditor->GetActiveRailIndex());
	if(nearest.railIndex < 0){
		return false;
	}

	// 水平方向(XZ平面)だけで見た、乗り換え先レールの最近傍点までの向きと距離
	Vector3 toRailXZ = {nearest.position.x - basePosition_.x, 0.0f, nearest.position.z - basePosition_.z};
	float horizontalDistance = Length(toRailXZ);
	if(horizontalDistance > kSideRailSwitchMaxDistance){
		return false;
	}

	// 押した方向の真横にあるか(前後にずれた位置のレールには乗り換えない)
	if(Dot(Normalize(toRailXZ),sideDirXZ) < kSideRailDirectionThreshold){
		return false;
	}

	// 高さが大きく違うレールには乗り換えない
	if(std::abs(nearest.position.y - basePosition_.y) > kSideRailSwitchMaxHeightDiff){
		return false;
	}

	// 乗り換え先レールの最近傍点からそのまま進行を続ける
	// 飛び移っている間も乗り換え先レールに沿って進めることで、勢いを保ったまま横へ移る動きにする
	railEditor->SwitchActiveRail(nearest.railIndex);
	railT_ = nearest.t;
	isRailFinished_ = false; // 乗り換え先レールを最後まで進めるようにする

	// 乗り換え前の位置(足がレールに着いているときだけ乗り換えるため、基準位置がそのままレール上の位置)からのずれを覚えて、飛び移る動きを始める
	railSwitchStartOffset_ = basePosition_ - nearest.position;
	railSwitchTimer_ = 0.0f;
	isSwitchingRail_ = true;
	return true;
}

// レール上でのジャンプ(その場で跳ねる)と、並走レールへ飛び移る動きを進める
// レールに沿った進行はUpdateRailProgressで続けているため、ここでは高さと経過時間だけを扱う
void Player::UpdateRailJump(float deltaTime){
	if(isRailHopping_){
		// 重力を適用してY方向の速度を更新し、跳ねている高さに反映する
		railHopVelocityY_ -= kGravity * deltaTime;
		railHopHeight_ += railHopVelocityY_ * deltaTime;

		// レールの高さまで戻ったら着地させる(レールから外れることはないため、必ず同じレールに着地する)
		if(railHopHeight_ <= 0.0f){
			railHopHeight_ = 0.0f;
			railHopVelocityY_ = 0.0f;
			isRailHopping_ = false;
		}
	}

	if(isSwitchingRail_){
		// 決めた時間が経ったら乗り換え先レールの上に着地させる(このときずれと山なりの高さはちょうど0になる)
		railSwitchTimer_ += deltaTime;
		if(railSwitchTimer_ >= kRailSwitchDuration){
			railSwitchTimer_ = kRailSwitchDuration;
			isSwitchingRail_ = false;
		}
	}
}

// 描画処理
void Player::Draw(){
	if(obj_){
		obj_->Draw();
	}
}

// 当たり判定用に、モデルから求めたワールド座標系の境界球を取得
// 行列はUpdateTransform()で更新されるため、その後に呼べば現在の位置・向き・大きさが反映される
Model::BoundingSphere Player::GetHitSphere() const{
	if(!obj_){
		return {position_, 0.0f};
	}
	return obj_->GetWorldBoundingSphere();
}
