#include "Player.h"
#include "PlayerState.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "InkEffectManager.h"
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

	// 調整項目名(体力の最大値・最後にダメージを受けてから体力が戻り始めるまでの時間・戻る速さ)
	const char* kMaxHpKey = "playerMaxHp";
	const char* kHpRegenStartDelayKey = "hpRegenStartDelay";
	const char* kHpRegenSpeedKey = "hpRegenSpeed";
}

// 最初はレールの上を走っている状態から始める(Reset() と同じ)
Player::Player() : state_(std::make_unique<RailRunState>()){}
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
	GlobalVariables::GetInstance()->AddItem(paramGroup_,kMaxHpKey,kDefaultMaxHp);
	GlobalVariables::GetInstance()->AddItem(paramGroup_,kHpRegenStartDelayKey,kDefaultHpRegenStartDelay);
	GlobalVariables::GetInstance()->AddItem(paramGroup_,kHpRegenSpeedKey,kDefaultHpRegenSpeed);
}

// レール先頭・オンレール・体力満タンの初期状態に戻す
void Player::Reset(){
	railT_ = 0.0f;
	isRailFinished_ = false; // レール終端フラグもリセットする

	// 移動の状態もリセットし、必ずレールの上を走っている状態から開始する
	// (レール上でのジャンプ・並走レールへの乗り換えの途中の値は状態クラスが持つため、状態を作り直すと消える)
	ChangeState(std::make_unique<RailRunState>());
	freeVelocityY_ = 0.0f;
	freeVelocityXZ_ = {0.0f, 0.0f, 0.0f};
	// やり直す前の着地のしぶきは出さない
	isLandingPending_ = false;

	// 終点で外れたレールも覚えていない状態に戻す
	endedRailIndex_ = -1;

	// 体力・無敵時間も初期状態に戻す
	// 体力の最大値は、ImGuiで変えた値がやり直すたびに反映されるよう、ここで調整項目から読み直す
	maxHp_ = (std::max)(GlobalVariables::GetInstance()->GetFloatValue(paramGroup_,kMaxHpKey),kMinMaxHp);
	hp_ = maxHp_;
	invincibleTimer_ = 0.0f;
	regenTimer_ = 0.0f;

	// 現れる演出の途中なら止め、ふつうに見える状態にする
	revealStyle_ = Model::RevealStyle::None;
	revealTimer_ = 0.0f;

	// 速さを上げている途中なら止める
	railBoostDuration_ = 0.0f;
	railBoostTimer_ = 0.0f;
}

// スタート台の上に立たせ、オフレールの状態から始める
// 台の上ではWASDで自由に動け、台と重なったレールの始点へ歩いて乗るとレールを進み始める
void Player::StartOnPlatform(const Vector3& startPosition,const Vector3& startRotation){
	ChangeState(std::make_unique<PlatformState>());
	freePosition_ = startPosition;
	freeVelocityY_ = 0.0f;
	freeVelocityXZ_ = {0.0f, 0.0f, 0.0f};
	// カメラはレールの始点の向きを正面にする
	freeBaseRotation_ = startRotation;
}

// レールの進行度startTの位置にオンレールの状態で乗せる
// 体力・無敵時間は残したまま、レールの進行とレール上でのジャンプ・乗り換えの途中状態だけを初めに戻す
void Player::BoardRailStart(float startT){
	railT_ = startT;
	isRailFinished_ = false;
	ChangeState(std::make_unique<RailRunState>());
	freeVelocityY_ = 0.0f;
	freeVelocityXZ_ = {0.0f, 0.0f, 0.0f};
	endedRailIndex_ = -1;
}

// 移動の状態を切り替える
void Player::ChangeState(std::unique_ptr<State> nextState){
	if(nextState){
		state_ = std::move(nextState);
	}
}

// オンレール中かどうか
bool Player::IsOnRail() const{
	return state_->IsOnRail();
}

// 足がレールに着いているかどうか
bool Player::IsGroundedOnRail() const{
	return state_->IsGroundedOnRail();
}

// ゴールのレールの終点に着いて止まったかどうか
bool Player::HasReachedGoal() const{
	return IsOnRail() && isRailFinished_;
}

// 出発の演出中に、プレイヤーの表示を演出側で決めた位置・向き・大きさにする
// 敵の検知・カメラの基準にも使う表示位置(position_)も、演出の位置にそろえる
void Player::SetDirectedPose(const Vector3& position,const Vector3& rotate,const Vector3& scaleRate){
	position_ = position;
	if(!obj_){
		return;
	}
	obj_->SetTranslate(position_);
	obj_->SetRotate(rotate);
	obj_->SetScale({kScale * scaleRate.x, kScale * scaleRate.y, kScale * scaleRate.z});
	obj_->Update();
}

// レールを進む速さを一時的に上げる
void Player::StartRailBoost(float rate,float duration){
	railBoostRate_ = rate;
	railBoostDuration_ = duration;
	railBoostTimer_ = 0.0f;
}

// レールの進行
// Edit中・開始演出中・オフレール中はその位置で静止させる
// 終端に到達したらループさせず、その場で停止させる(ループするレールだけは、終端から始点へ回って進み続ける)
void Player::UpdateRailProgress(RailEditor* railEditor,bool isGameplayActive,float deltaTime){
	if(!railEditor || !isGameplayActive || !IsOnRail() || isRailFinished_){
		return;
	}

	// 調整項目から最新の値を取得(ImGui編集/ホットリロードが即反映される)
	float railSpeed = GlobalVariables::GetInstance()->GetFloatValue(paramGroup_,"railSpeed");

	// 現在位置に対応する制御点のSpeed値を取得し、全体速度(railSpeed)に掛けて今フレームに進む距離を求める
	// 進行度に直接足すと制御点間の距離で速さが変わるため、距離として進めて一定の速さにする
	float pointSpeed = railEditor->GetSpeedOnRail(railT_);

	// ボスの2形態目などで、外から指定された倍率を掛ける
	railSpeed *= railSpeedRate_;

	// レール上で跳ねている間・並走レールへ飛び移っている間は、空中で前へ流されすぎないよう進む速さを落とす
	if(!IsGroundedOnRail()){
		railSpeed *= kRailAirborneSpeedRate;
	}

	// 発射台から飛び出した直後は速さを上げ、勢いよく飛び出してから少しずつ元の速さへ戻す
	if(railBoostTimer_ < railBoostDuration_){
		railBoostTimer_ = (std::min)(railBoostTimer_ + deltaTime,railBoostDuration_);
		railSpeed *= Lerp(railBoostRate_,1.0f,Easing::EaseOutCubic(railBoostTimer_ / railBoostDuration_));
	}

	const float railMoveSpeed = pointSpeed * railSpeed;
	railT_ = railEditor->AdvanceTByDistance(railT_,railMoveSpeed * deltaTime);

	// ループするレールには終点が無いため、進行度が1周分を越えたら越えた分を残して始点側へ回し、そのまま進み続ける
	// 進行度1と0は同じ位置・同じ向きのため、つなぎ目で位置もカメラも跳ばない
	if(railEditor->IsLoopRail(railEditor->GetActiveRailIndex())){
		if(railT_ >= kRailEndT){
			railT_ -= kRailEndT;
		}
		return;
	}

	if(railT_ >= kRailEndT){
		railT_ = kRailEndT; // 終端で固定する
		isRailFinished_ = true;

		// 終点で飛び移る先のレールが決めてあれば、並走レールへの飛び移りと同じ動きで、そのレールへ自動で飛び移って進み続ける
		// 乗るレールを切り替えるのはすぐに行い、見た目の位置だけを終点から飛び移る先へ山なりに寄せていく
		int transferRailIndex = -1;
		float transferT = 0.0f;
		if(railEditor->FindEndTransfer(railEditor->GetActiveRailIndex(),transferRailIndex,transferT)){
			// 跳ねている途中に終点へ着いたときも見た目の位置が跳ばないよう、今のずれ(跳ねている高さなど)も含めた位置から寄せる
			const Vector3 endPosition = railEditor->GetPositionOnRail(railT_) + state_->GetRailOffset();
			railEditor->SwitchActiveRail(transferRailIndex);
			railT_ = transferT;
			isRailFinished_ = false;
			ChangeState(std::make_unique<RailSwitchState>(endPosition - railEditor->GetPositionOnRail(railT_)));
			return;
		}

		// ゴールのレール以外は、終点に着いたらレールから外れ、進んでいた勢いのまま前へジャンプさせる(どのレールにも乗れずに落ちたらリスタートになる)
		// その場で真下に落としたり別の場所へ移したりすると不具合に見えるため、レールの続きのように前へ飛ばす
		// 跳ねている途中・飛び移っている途中の状態を残すと、離れた直後に同じレールへ着地し直してしまうため、落下の状態へ切り替えて消す
		// 上り坂で終わるレールでは、外れた位置が終点の少し手前のレールより上になり、そこへ着地し直してしまうため、外れたレールを覚えて着地先から外す
		if(!railEditor->IsGoalRail(railEditor->GetActiveRailIndex())){
			endedRailIndex_ = railEditor->GetActiveRailIndex();
			ChangeState(std::make_unique<FallState>());
			freePosition_ = railEditor->GetPositionOnRail(railT_);
			freeBaseRotation_ = railEditor->GetRotationOnRail(railT_);

			// 終点での進行方向へ、レールを進んでいた速さを弱めた勢いで飛び出し、小さなジャンプの初速を上向きに足す
			const Vector3 launchVelocity = railEditor->GetForwardOnRail(railT_) * (railMoveSpeed * kRailEndLaunchSpeedRate);
			freeVelocityXZ_ = {launchVelocity.x, 0.0f, launchVelocity.z};
			freeVelocityY_ = launchVelocity.y + kRailEndJumpSpeed;
		}
	}
}

// 基準位置・基準向きの計算
// オフレール中は、直前フレームで更新した自由移動座標(freePosition_)とジャンプ時に固定した向き(freeBaseRotation_)を基準にする
// オンレール中にジャンプ・並走レールへの乗り換えをしている間は、レール上の位置に今の状態のずれ(跳ねている高さ・飛び移り中のずれと山なりの高さ)を加える
void Player::UpdateBasePose(const RailEditor* railEditor){
	if(!railEditor){
		return;
	}

	const Vector3 railPos = railEditor->GetPositionOnRail(railT_) + state_->GetRailOffset();
	const Vector3 railRot = railEditor->GetRotationOnRail(railT_);

	const bool isOnRail = IsOnRail();
	basePosition_ = isOnRail?railPos:freePosition_;
	baseRotation_ = isOnRail?railRot:freeBaseRotation_;
}

// 表示位置の計算とモデルの行列更新
// プレイヤーはレールの上(基準位置)に立たせ、体は照準の方向(カメラの左右の向き)へ向ける
// レールの傾きに合わせて体を倒すと照準とずれて見えるため、体は常に直立させる
void Player::UpdateTransform(const Vector3& cameraRotation,float hopHeight){
	position_ = basePosition_;

	// レール・スタート台へ着地したフレームは、足元から輪の形にインクのしぶきを広げて着地の衝撃を見せる
	if(isLandingPending_){
		isLandingPending_ = false;
		if(inkEffect_){
			inkEffect_->EmitLandingRing(position_);
		}
	}

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

	// 現れる演出の見せ方と進み具合をマテリアルに渡す
	// しま・境界線の位置を体に合わせるため、範囲は足元から、行列を更新した後の境界球の上端までにする
	if(Model::Material* material = obj_->GetMaterial()){
		material->revealStyle = static_cast<int32_t>(revealStyle_);
		if(revealStyle_ != Model::RevealStyle::None){
			const Model::BoundingSphere sphere = obj_->GetWorldBoundingSphere();
			material->revealProgress = (revealDuration_ > 0.0f)?(std::min)(revealTimer_ / revealDuration_,1.0f):1.0f;
			material->revealBottom = position_.y + hopHeight;
			material->revealHeight = sphere.center.y + sphere.radius - material->revealBottom;
		}
	}
}

// 透明な体がだんだん元に戻る、現れる演出を始める
void Player::StartReveal(Model::RevealStyle style,float duration){
	revealStyle_ = style;
	revealTimer_ = 0.0f;
	revealDuration_ = duration;
}

// 現れる演出を進め、元に戻りきったらふつうに描く状態に戻す
void Player::UpdateReveal(float deltaTime){
	if(revealStyle_ == Model::RevealStyle::None){
		return;
	}
	revealTimer_ += deltaTime;
	if(revealTimer_ >= revealDuration_){
		revealStyle_ = Model::RevealStyle::None;
	}
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

// 体力の自然回復を進める
// 最後にダメージを受けてから hpRegenStartDelay 秒たつと、1秒あたり hpRegenSpeed ずつ体力が戻る(ダメージを受けると数え直す)
void Player::UpdateRegeneration(float deltaTime){
	// 体力が尽きているとき・減っていないときは回復しない
	if(hp_ <= 0.0f || hp_ >= maxHp_){
		return;
	}

	GlobalVariables* gv = GlobalVariables::GetInstance();
	regenTimer_ += deltaTime;
	if(regenTimer_ >= gv->GetFloatValue(paramGroup_,kHpRegenStartDelayKey)){
		hp_ = (std::min)(hp_ + gv->GetFloatValue(paramGroup_,kHpRegenSpeedKey) * deltaTime,maxHp_);
	}
}

// 敵弾が当たったときの処理
// 連続被弾で一瞬に体力が尽きないよう、被弾後は一定時間無敵にする
void Player::TakeDamage(float damage){
	if(invincibleTimer_ > 0.0f || hp_ <= 0.0f){
		return;
	}

	hp_ = (std::max)(hp_ - damage,0.0f);
	invincibleTimer_ = kInvincibleTime;
	// ダメージを受けたら、自然回復までの時間を最初から数え直す
	regenTimer_ = 0.0f;

	// 被弾位置に火花パーティクルを発生させ、当たったことを見た目で分かるようにする
	if(inkEffect_){
		inkEffect_->EmitSpark(position_);
	}
}

// 時間に応じて減り続けるダメージを受ける
// 敵のインクに浸かっている間のように、当たった瞬間ではなく受けている間ずっと減るダメージのため、無敵時間に関係なく減らす
void Player::TakeContinuousDamage(float damagePerSecond,float deltaTime){
	if(hp_ <= 0.0f){
		return;
	}

	hp_ = (std::max)(hp_ - damagePerSecond * deltaTime,0.0f);
	// ダメージを受けている間は、自然回復が始まらないよう数え直し続ける
	regenTimer_ = 0.0f;
}

// プレイヤーの自立(ジャンプ+WASD移動)
// ジャンプ・WASD移動・着地判定は今の移動の状態に任せ、次の状態が返ってきたら切り替える
// (レール上ではその場でのジャンプと並走レールへの乗り換えだけ、レールの外ではWASD移動と重力・着地判定を行う)
bool Player::UpdateMovement(Input* input,RailEditor* railEditor,const StartPlatform* startPlatform,const Vector3& cameraForward,const Vector3& cameraRight,float groundHeight,float deltaTime){
	if(!input || !railEditor){
		return false;
	}

	MoveContext context;
	context.input = input;
	context.railEditor = railEditor;
	context.startPlatform = startPlatform;
	context.cameraForward = cameraForward;
	context.cameraRight = cameraRight;
	context.deltaTime = deltaTime;
	ChangeState(state_->UpdateMovement(*this,context));

	// レールから落ちたときのリスタート
	// どのレールにも乗れないまま、地面の高さまで落ちたらリスタートが必要であることを呼び出し側へ伝える
	return !IsOnRail() && freePosition_.y <= groundHeight;
}

// 今のレール以外で最も近いレールが、飛び移れる近さと高さにあるかを調べる
bool Player::FindSideRailInReach(const RailEditor* railEditor,RailEditor::NearestRailResult& outNearest,Vector3& outToRailXZ) const{
	outNearest = railEditor->FindNearestRail(basePosition_,railEditor->GetActiveRailIndex());
	if(outNearest.railIndex < 0){
		return false;
	}

	// 水平方向(XZ平面)だけで見た、乗り換え先レールの最近傍点までの向きと距離
	outToRailXZ = {outNearest.position.x - basePosition_.x, 0.0f, outNearest.position.z - basePosition_.z};
	if(Length(outToRailXZ) > kSideRailSwitchMaxDistance){
		return false;
	}

	// 高さが大きく違うレールには乗り換えない
	return std::abs(outNearest.position.y - basePosition_.y) <= kSideRailSwitchMaxHeightDiff;
}

// 今のレールから飛び移れる並走レールが、左右どちらにあるかを求める
// A/D で飛び移る向き(カメラの右方向)と、レールまでの向きが真横に近いかで左右を決める
int Player::FindSideRailDirection(const RailEditor* railEditor,const Vector3& cameraRight,int& outRailIndex) const{
	outRailIndex = -1;
	RailEditor::NearestRailResult nearest;
	Vector3 toRailXZ;
	if(!railEditor || !FindSideRailInReach(railEditor,nearest,toRailXZ)){
		return 0;
	}

	const Vector3 rightXZ = Normalize(Vector3{cameraRight.x, 0.0f, cameraRight.z});
	const float side = Dot(Normalize(toRailXZ),rightXZ);
	if(side >= kSideRailDirectionThreshold){
		outRailIndex = nearest.railIndex;
		return 1;
	}
	if(side <= -kSideRailDirectionThreshold){
		outRailIndex = nearest.railIndex;
		return -1;
	}
	return 0;
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
