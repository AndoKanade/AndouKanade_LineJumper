#include "Enemy.h"
#include "EnemyAttackState.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include <cmath>
#include <string>

namespace{
	// 雑魚敵の表示に使用するモデル(プレイヤーと区別できるよう、4本脚で一つ目のロボットにする)
	const std::string kEnemyModelPath = "EnemyBot/enemyBot.obj";

	// 敵弾の色(自機の弾が青なので、区別できるよう赤にする)
	constexpr Vector4 kEnemyBulletColor = {1.0f, 0.2f, 0.2f, 1.0f};

	// 雑魚敵の体の色(自機と区別できるよう、弾と同系統の赤にする)
	constexpr Vector4 kEnemyColor = {1.0f, 0.3f, 0.3f, 1.0f};
}

Enemy::Enemy() = default;
Enemy::~Enemy() = default;

// 初期化処理
void Enemy::Initialize(Obj3dCommon* objCommon,const Vector3& basePosition,const Vector3& patrolDirection,int maxHp){
	// 描画用モデルの読み込みと3Dオブジェクトの生成
	ModelManager::GetInstance()->LoadModel(kEnemyModelPath);
	obj_ = std::make_unique<Obj3D>();
	obj_->Initialize(objCommon);
	obj_->SetModel(kEnemyModelPath);

	// 体の色は生成時に一度設定するだけでよいため、ここで赤にしておく
	if(Model::Material* material = obj_->GetMaterial()){
		material->color = kEnemyColor;
	}

	basePosition_ = basePosition;
	patrolDirection_ = Normalize(patrolDirection);

	// 体力の最大値を設定する(Reset()で現在の体力がこの値まで回復する)
	SetMaxHp(maxHp);

	// 敵弾の生成
	// 発射のたびに生成すると無駄な処理が毎フレーム発生するため、ここで最大数ぶんまとめて作り、以降は使い回す
	EnemyBulletPool::Settings bulletSettings;
	bulletSettings.count = kMaxBulletCount;
	bulletSettings.scale = kBulletScale;
	bulletSettings.lifeTime = kBulletLifeTime;
	bulletSettings.homingTurnRate = kHomingTurnRate;
	bulletSettings.color = kEnemyBulletColor;
	bulletPool_.Initialize(objCommon,bulletSettings);

	Reset();
}

// 初期状態(生存・移動位置の中心)へ戻す
void Enemy::Reset(){
	position_ = basePosition_;
	patrolOffset_ = 0.0f;
	patrolSign_ = 1.0f;
	rotationY_ = 0.0f;
	isAlive_ = true;
	isDetectingPlayer_ = false;

	// 体力を最大値まで戻す
	hp_ = maxHp_;

	// 発射済みの弾をすべて未使用に戻し、発射間隔も初期化する
	bulletPool_.Clear();

	// 攻撃の状態も先頭(単発)に戻してから、その攻撃の発射間隔で待ち時間を初期化する
	attackState_ = std::make_unique<SingleShotAttackState>();
	shotTimer_ = attackState_->GetShotInterval();
}

// 体力を減らす(撃破されたときtrueを返す)
bool Enemy::TakeDamage(int damage){
	// 撃破済みの敵はこれ以上体力が減らない
	if(!isAlive_){
		return false;
	}

	hp_ -= damage;
	if(hp_ <= 0){
		hp_ = 0;
		isAlive_ = false;
		return true;
	}

	return false;
}

// 往復移動の中心座標を設定する(現在の往復位置を保ったまま移動させる)
void Enemy::SetBasePosition(const Vector3& basePosition){
	basePosition_ = basePosition;
	position_ = basePosition_ + patrolDirection_ * patrolOffset_;
}

// 往復移動の方向を設定する(内部で正規化する)
void Enemy::SetPatrolDirection(const Vector3& patrolDirection){
	patrolDirection_ = Normalize(patrolDirection);
	position_ = basePosition_ + patrolDirection_ * patrolOffset_;
}

// 体力の最大値を設定する(現在の体力が最大値を超える場合は最大値に合わせる)
void Enemy::SetMaxHp(int maxHp){
	// 0以下だと生成直後に撃破された状態になってしまうため下限で制限する
	maxHp_ = (maxHp < kMinMaxHp)?kMinMaxHp:maxHp;
	if(hp_ > maxHp_){
		hp_ = maxHp_;
	}
}

// 更新処理
void Enemy::Update(const Vector3& playerPosition,float deltaTime){
	// 撃破済みでも発射済みの弾は飛び続けさせるため、弾の更新は本体より先に行う
	// 追尾弾の向き補正にプレイヤー座標が必要なため、そのまま渡す
	bulletPool_.Update(playerPosition,deltaTime);

	// 撃破済みのときは移動も向きの更新も行わない
	if(!isAlive_){
		return;
	}

	// 固定パターンの移動
	// 中心座標からのずれを進行向きに応じて増減させ、可動範囲の端に達したら折り返す
	patrolOffset_ += patrolSign_ * kMoveSpeed * deltaTime;
	if(patrolOffset_ > kPatrolRange){
		patrolOffset_ = kPatrolRange;
		patrolSign_ = -1.0f;
	} else if(patrolOffset_ < -kPatrolRange){
		patrolOffset_ = -kPatrolRange;
		patrolSign_ = 1.0f;
	}
	position_ = basePosition_ + patrolDirection_ * patrolOffset_;

	// プレイヤーの検知判定(検知範囲内かどうか)
	Vector3 toPlayer = playerPosition - position_;
	isDetectingPlayer_ = (Length(toPlayer) <= kDetectionRange);

	// 弾の発射処理
	// プレイヤーを検知している間だけ、一定間隔でプレイヤーへ向けて撃つ
	// 1回撃つごとに、今の攻撃の状態が決めた次の攻撃へ切り替え、単発・3方向拡散・追尾弾を順番に使う
	if(isDetectingPlayer_){
		shotTimer_ -= deltaTime;
		if(shotTimer_ <= 0.0f){
			FireCurrentAttack(playerPosition);

			// 次の攻撃へ切り替え、その攻撃の発射間隔で待ち時間を設定する
			attackState_ = attackState_->CreateNext();
			shotTimer_ = attackState_->GetShotInterval();
		}
	} else{
		// 非検知中は撃たない。次に検知した直後に即撃ちされないよう、待ち時間を戻しておく
		shotTimer_ = attackState_->GetShotInterval();
	}

	// 向きの決定
	// 検知中はプレイヤーの方向、非検知中は移動している方向を向く
	Vector3 facingDirection = isDetectingPlayer_?toPlayer:(patrolDirection_ * patrolSign_);

	// XZ平面での向き(Y軸回転)を求める。真上・真下方向しか成分が無い場合は前フレームの向きを維持する
	if(std::fabs(facingDirection.x) > 1e-5f || std::fabs(facingDirection.z) > 1e-5f){
		rotationY_ = std::atan2(facingDirection.x,facingDirection.z);
	}

	// 描画用オブジェクトへ反映する
	if(obj_){
		obj_->SetTranslate(position_);
		obj_->SetRotate({0.0f, rotationY_, 0.0f});
		obj_->SetScale({kScale, kScale, kScale});

		// アクティブカメラが切り替わっても正しく描画されるよう毎フレーム同期
		if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
			obj_->SetCamera(activeCamera);
		}

		obj_->Update();
	}
}

// 描画処理(撃破済みのときは本体を描画しない。発射済みの弾は残っていれば描画する)
void Enemy::Draw(){
	if(isAlive_ && obj_){
		obj_->Draw();
	}

	// 発射中の弾を描画する
	bulletPool_.Draw();
}

// 現在の攻撃の状態に発射を任せる
void Enemy::FireCurrentAttack(const Vector3& playerPosition){
	// 発射位置と、そこからプレイヤーへ向かう方向は全ての攻撃で共通なので先に求める
	Vector3 spawnPosition = GetBulletSpawnPosition();

	Vector3 toPlayer = playerPosition - spawnPosition;
	if(Length(toPlayer) < 1e-5f){
		return; // プレイヤーと発射位置がほぼ同じ場合は方向が定まらないため撃たない
	}
	Vector3 baseDirection = Normalize(toPlayer);

	// 撃ち方(単発・3方向拡散・追尾弾)は今の攻撃の状態が決める
	attackState_->Fire(bulletPool_,spawnPosition,baseDirection);
}

// 弾の発射位置(敵の中心より少し上=胸の高さ)を取得する
Vector3 Enemy::GetBulletSpawnPosition() const{
	Vector3 spawnPosition = position_;
	spawnPosition.y += kBulletSpawnUpOffset;
	return spawnPosition;
}

// 発射済みの弾とプレイヤーの当たり判定
int Enemy::CheckHitToPlayer(const Model::BoundingSphere& playerSphere){
	return bulletPool_.CheckHitToPlayer(playerSphere);
}

// 当たり判定用に、モデルから求めたワールド座標系の境界球を取得
// 本体の行列はUpdate()の最後で更新されるため、Update()後に呼べば現在の位置・向き・大きさが反映される
Model::BoundingSphere Enemy::GetHitSphere() const{
	if(!obj_){
		return {position_, 0.0f};
	}
	return obj_->GetWorldBoundingSphere();
}

// 発射済みで生存している弾の数を取得(デバッグ表示用)
int Enemy::GetActiveBulletCount() const{
	return bulletPool_.GetActiveCount();
}