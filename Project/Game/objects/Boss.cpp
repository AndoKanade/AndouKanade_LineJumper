#include "Boss.h"
#include "PlayerBulletManager.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "GlobalVariables.h"
#include "ParticleManager.h"
#include <algorithm>
#include <cmath>

namespace{
	// ボスの表示に使用するモデル(球の体にとげと一つ目を付けた仮のモデル。tools/generate_boss_assets.py で生成する)
	// 体力に合わせて変わる色を掛けるため、体は白にしている
	const std::string kBossModelPath = "Boss/boss.obj";
	// 調整項目を登録するGlobalVariablesのグループ名(resource/GlobalVariables/Boss.json に保存される)
	const char* kBossGroup = "Boss";
	// 調整項目名(1形態目の体力・体の中心を置く位置・表示スケール・上下の揺れの幅と速さ)
	const char* kPhase1HpKey = "phase1Hp";
	const char* kPositionKey = "position";
	const char* kScaleKey = "scale";
	const char* kBobAmplitudeKey = "bobAmplitude";
	const char* kBobSpeedKey = "bobSpeed";
	// 調整項目名(攻撃の間隔・弾の速さ・連射の弾数と間隔・扇状の弾の弾数と角度・追尾弾の速さと曲がりやすさ・先読みの強さ)
	const char* kAttackIntervalKey = "attackInterval";
	const char* kBulletSpeedKey = "bulletSpeed";
	const char* kBurstCountKey = "burstCount";
	const char* kBurstIntervalKey = "burstInterval";
	const char* kFanCountKey = "fanCount";
	const char* kFanAngleKey = "fanAngle";
	const char* kHomingSpeedKey = "homingSpeed";
	const char* kHomingTurnRateKey = "homingTurnRate";
	const char* kLeadRateKey = "leadRate";
}

Boss::Boss() = default;
Boss::~Boss() = default;

// 初期化処理
void Boss::Initialize(Obj3dCommon* objCommon){
	// 調整項目の登録(第3引数はデフォルト値。保存済みJSONがあればそちらが優先される)
	GlobalVariables* gv = GlobalVariables::GetInstance();
	gv->CreateGroup(kBossGroup);
	gv->AddItem(kBossGroup,kPhase1HpKey,static_cast<int32_t>(kDefaultPhase1Hp));
	gv->AddItem(kBossGroup,kPositionKey,kDefaultPosition);
	gv->AddItem(kBossGroup,kScaleKey,kDefaultScale);
	gv->AddItem(kBossGroup,kBobAmplitudeKey,kDefaultBobAmplitude);
	gv->AddItem(kBossGroup,kBobSpeedKey,kDefaultBobSpeed);
	gv->AddItem(kBossGroup,kAttackIntervalKey,kDefaultAttackInterval);
	gv->AddItem(kBossGroup,kBulletSpeedKey,kDefaultBulletSpeed);
	gv->AddItem(kBossGroup,kBurstCountKey,static_cast<int32_t>(kDefaultBurstCount));
	gv->AddItem(kBossGroup,kBurstIntervalKey,kDefaultBurstInterval);
	gv->AddItem(kBossGroup,kFanCountKey,static_cast<int32_t>(kDefaultFanCount));
	gv->AddItem(kBossGroup,kFanAngleKey,kDefaultFanAngle);
	gv->AddItem(kBossGroup,kHomingSpeedKey,kDefaultHomingSpeed);
	gv->AddItem(kBossGroup,kHomingTurnRateKey,kDefaultHomingTurnRate);
	gv->AddItem(kBossGroup,kLeadRateKey,kDefaultLeadRate);

	ModelManager::GetInstance()->LoadModel(kBossModelPath);

	obj_ = std::make_unique<Obj3D>();
	obj_->Initialize(objCommon);
	obj_->SetModel(kBossModelPath);

	// 弾の生成(雑魚敵と同じ使い回しの仕組みを使う。追尾弾の曲がりやすさは調整項目から毎フレーム渡す)
	EnemyBulletPool::Settings bulletSettings;
	bulletSettings.count = kMaxBulletCount;
	bulletSettings.scale = kBulletScale;
	bulletSettings.lifeTime = kBulletLifeTime;
	bulletSettings.homingTurnRate = kDefaultHomingTurnRate;
	bulletSettings.color = kBulletColor;
	bulletPool_.Initialize(objCommon,bulletSettings);

	Reset();
}

// 戦う前の状態(体力満タン・弾なし)に戻す
// 体力の最大値は、ImGuiで変えた値がやり直すたびに反映されるよう、ここで調整項目から読み直す
void Boss::Reset(){
	maxHp_ = (std::max)(static_cast<int>(GlobalVariables::GetInstance()->GetIntValue(kBossGroup,kPhase1HpKey)),kMinMaxHp);
	hp_ = maxHp_;
	isDefeated_ = false;
	bobTimer_ = 0.0f;
	if(Model::Material* mat = obj_->GetMaterial()){
		mat->color = kFullHpColor;
	}
	ResetAttack();
}

// 体力は残したまま、撃った弾を消して攻撃を最初の待ち時間からやり直す
void Boss::ResetAttack(){
	bulletPool_.Clear();
	attackIndex_ = 0;
	attackTimer_ = GlobalVariables::GetInstance()->GetFloatValue(kBossGroup,kAttackIntervalKey);
	burstShotsLeft_ = 0;
	// やり直しでプレイヤーの位置が飛ぶため、速さは次のフレームから求め直す
	playerVelocity_ = {0.0f, 0.0f, 0.0f};
	hasPreviousPlayerPosition_ = false;
}

// 位置・向き・攻撃の更新と、プレイヤーの弾との当たり判定
void Boss::Update(bool isBossStage,const Vector3& playerPosition,PlayerBulletManager& bullets,bool canBeHit,bool canAttack,float deltaTime){
	isBossStage_ = isBossStage;
	if(!isBossStage_ || isDefeated_){
		return;
	}

	// 調整項目から最新の値を取得(ImGui編集/ホットリロードが即反映される)
	GlobalVariables* gv = GlobalVariables::GetInstance();
	const Vector3 basePosition = gv->GetVector3Value(kBossGroup,kPositionKey);
	scale_ = (std::max)(gv->GetFloatValue(kBossGroup,kScaleKey),kMinScale);

	// 撃った弾を進める(追尾弾はプレイヤーの方へ少しずつ向きを変える)
	bulletPool_.SetHomingTurnRate(gv->GetFloatValue(kBossGroup,kHomingTurnRateKey));
	bulletPool_.Update(playerPosition,deltaTime);

	// 先読みに使うプレイヤーの速さを求める
	UpdatePlayerVelocity(playerPosition,deltaTime);

	// 決めた位置を中心に、上下へゆっくり揺らして浮かんでいるように見せる
	bobTimer_ += deltaTime;
	position_ = basePosition;
	position_.y += std::sin(bobTimer_ * gv->GetFloatValue(kBossGroup,kBobSpeedKey)) * gv->GetFloatValue(kBossGroup,kBobAmplitudeKey);

	// 弾が当たったら体力を減らし、当たった位置に火花を出して効いていることを見せる
	// 同じフレームに何発当たっても1発ずつ判定し、当たった弾はすべて消す
	if(canBeHit){
		const Model::BoundingSphere hitSphere = GetHitSphere();
		while(hp_ > 0 && bullets.CheckHit(hitSphere)){
			hp_ -= bullets.GetDamage();
			// 火花は、体の中心からプレイヤーの方へ向いた体の表面に出す(中心に出すと体に隠れて見えない)
			const Vector3 toPlayer = playerPosition - position_;
			const Vector3 sparkPosition = (Length(toPlayer) >= kMinFaceDistance)?position_ + Normalize(toPlayer) * hitSphere.radius:position_;
			ParticleManager::GetInstance()->EmitSpark(sparkPosition);
		}
		if(hp_ <= 0){
			hp_ = 0;
			isDefeated_ = true;
			// 倒した後に残った弾でやられないよう、撃った弾を消す
			bulletPool_.Clear();
			return;
		}
	}

	// プレイヤーが輪に乗っている間、決まった順番で攻撃する
	UpdateAttack(playerPosition,canAttack,deltaTime);

	// 減った体力の割合に合わせて、満タンの色から残りわずかの色へ近づける
	if(Model::Material* mat = obj_->GetMaterial()){
		const float damageRate = 1.0f - static_cast<float>(hp_) / static_cast<float>(maxHp_);
		mat->color = {
			Lerp(kFullHpColor.x,kLowHpColor.x,damageRate),
			Lerp(kFullHpColor.y,kLowHpColor.y,damageRate),
			Lerp(kFullHpColor.z,kLowHpColor.z,damageRate),
			Lerp(kFullHpColor.w,kLowHpColor.w,damageRate),
		};
	}

	// 一つ目のある正面(Z軸)を、Y軸回転でプレイヤーの方へ向ける(体は直立させたままにする)
	const Vector3 toPlayerXZ = {playerPosition.x - position_.x, 0.0f, playerPosition.z - position_.z};
	if(Length(toPlayerXZ) >= kMinFaceDistance){
		obj_->SetRotate({0.0f, std::atan2(toPlayerXZ.x,toPlayerXZ.z), 0.0f});
	}
	obj_->SetTranslate(position_);
	obj_->SetScale({scale_, scale_, scale_});
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		obj_->SetCamera(activeCamera);
	}
	obj_->Update();
}

// プレイヤーの水平方向の速さを、前のフレームからの移動量で求める
// レールの上で跳ねている間の上下の動きは先読みに使わないため、水平方向だけを見る
void Boss::UpdatePlayerVelocity(const Vector3& playerPosition,float deltaTime){
	// 時間が止まっている間は速さを求められないため、前の値を残す
	if(deltaTime <= 0.0f){
		return;
	}

	if(hasPreviousPlayerPosition_){
		Vector3 velocity = (playerPosition - previousPlayerPosition_) * (1.0f / deltaTime);
		velocity.y = 0.0f;
		// やり直しなどで位置が飛んだフレームは、速さとして使わない
		playerVelocity_ = (Length(velocity) <= kMaxTrackedPlayerSpeed)?velocity:Vector3{0.0f, 0.0f, 0.0f};
	}
	previousPlayerPosition_ = playerPosition;
	hasPreviousPlayerPosition_ = true;
}

// 攻撃の待ち時間を進め、時間が来たら今の種類の攻撃を撃つ
// 1回の攻撃を終えるたびに次の種類へ進め、先読みの連射・扇状の弾・追尾弾を順番に使う
void Boss::UpdateAttack(const Vector3& playerPosition,bool canAttack,float deltaTime){
	GlobalVariables* gv = GlobalVariables::GetInstance();
	const float attackInterval = gv->GetFloatValue(kBossGroup,kAttackIntervalKey);

	// プレイヤーが輪に乗っていない間は撃たない。乗った直後にすぐ撃たれないよう、待ち時間を戻し、連射の途中も打ち切る
	if(!canAttack){
		attackTimer_ = attackInterval;
		burstShotsLeft_ = 0;
		return;
	}

	attackTimer_ -= deltaTime;
	if(attackTimer_ > 0.0f){
		return;
	}

	const float bulletSpeed = (std::max)(gv->GetFloatValue(kBossGroup,kBulletSpeedKey),kMinBulletSpeed);

	switch(kAttackOrder[attackIndex_]){
	case AttackType::Fan:
	{
		// プレイヤーが進む先を中心に、左右へ角度をつけた弾を同時に撃つ
		// 中央の弾を基準(添字fanCount / 2)として、そこからのずれ分だけ角度をつける
		const int fanCount = (std::max)(static_cast<int>(gv->GetIntValue(kBossGroup,kFanCountKey)),kMinShotCount);
		const float fanAngle = gv->GetFloatValue(kBossGroup,kFanAngleKey);
		const Vector3 target = ComputeLeadTarget(playerPosition,bulletSpeed);
		const int centerIndex = fanCount / 2;
		for(int i = 0; i < fanCount; ++i){
			FireToward(target,static_cast<float>(i - centerIndex) * fanAngle,bulletSpeed,false);
		}
		break;
	}
	case AttackType::Homing:
	{
		// 発射後もプレイヤーを追い続ける遅い弾を1発だけ撃つ(追いかけるため先読みはしない)
		const float homingSpeed = (std::max)(gv->GetFloatValue(kBossGroup,kHomingSpeedKey),kMinBulletSpeed);
		FireToward({playerPosition.x, playerPosition.y + kPlayerAimHeight, playerPosition.z},0.0f,homingSpeed,true);
		break;
	}
	case AttackType::Burst:
	default:
		// プレイヤーが進む先を狙った弾を、短い間隔で続けて撃つ(1発ごとに狙い直す)
		if(burstShotsLeft_ <= 0){
			burstShotsLeft_ = (std::max)(static_cast<int>(gv->GetIntValue(kBossGroup,kBurstCountKey)),kMinShotCount);
		}
		FireToward(ComputeLeadTarget(playerPosition,bulletSpeed),0.0f,bulletSpeed,false);
		--burstShotsLeft_;
		// 連射の残りがあるときは、攻撃の種類を進めずに次の1発を待つ
		if(burstShotsLeft_ > 0){
			attackTimer_ = gv->GetFloatValue(kBossGroup,kBurstIntervalKey);
			return;
		}
		break;
	}

	// 次の攻撃へ切り替え、攻撃の間隔だけ待つ
	attackIndex_ = (attackIndex_ + 1) % kAttackOrderCount;
	attackTimer_ = attackInterval;
}

// 速さbulletSpeedの弾が届くころにプレイヤーがいる位置を求める
// 輪を回っているプレイヤーは、撃った瞬間の位置を狙っても弾が届くころには先へ進んでいて当たらないため、進む先を狙う
// レールの上で跳ねると進む速さが落ちるため、跳ねれば先読みの弾を避けられる
Vector3 Boss::ComputeLeadTarget(const Vector3& playerPosition,float bulletSpeed) const{
	const Vector3 aimPosition = {playerPosition.x, playerPosition.y + kPlayerAimHeight, playerPosition.z};

	// 弾は体の表面から撃つため、体の半径の分だけ飛ぶ距離が短い
	const float travelDistance = (std::max)(Length(aimPosition - position_) - GetHitSphere().radius,0.0f);
	const float travelTime = travelDistance / bulletSpeed;
	const float leadRate = GlobalVariables::GetInstance()->GetFloatValue(kBossGroup,kLeadRateKey);
	return aimPosition + playerVelocity_ * (travelTime * leadRate);
}

// 体の表面から、狙う点へ向かう向きを左右にずらして弾を1発撃つ
void Boss::FireToward(const Vector3& target,float yawOffset,float speed,bool isHoming){
	const Vector3 toTarget = target - position_;
	if(Length(toTarget) < kMinFaceDistance){
		return; // 狙う点がボスの中心とほぼ同じ場合は方向が定まらないため撃たない
	}
	const Vector3 baseDirection = Normalize(toTarget);

	// Y軸回転でXZ平面上の向きだけを回す(上下の角度は狙う点への向きと同じにする)
	const float cosAngle = std::cos(yawOffset);
	const float sinAngle = std::sin(yawOffset);
	const Vector3 direction = {
		baseDirection.x * cosAngle + baseDirection.z * sinAngle,
		baseDirection.y,
		-baseDirection.x * sinAngle + baseDirection.z * cosAngle
	};

	// 体の中から出てくるように見えないよう、体の表面から撃つ
	bulletPool_.Fire(position_ + direction * GetHitSphere().radius,direction,speed,isHoming);
}

// レティクルの方向が当たり判定の球を通っているかを調べる
bool Boss::FindAimed(const Vector3& cameraPosition,const Vector3& cameraForward,float minDistance,float& outDistance) const{
	if(!IsActive()){
		return false;
	}

	// 球の中心からカメラの前方の線へ下ろした垂線の足までの距離
	const Model::BoundingSphere sphere = GetHitSphere();
	const Vector3 toCenter = sphere.center - cameraPosition;
	const float alongDistance = Dot(toCenter,cameraForward);

	// 線と球の中心の距離が半径以内なら、線が球を通っている(平方のまま比べて平方根を省く)
	const float distanceSquaredToLine = Dot(toCenter,toCenter) - alongDistance * alongDistance;
	const float radiusSquared = sphere.radius * sphere.radius;
	if(distanceSquaredToLine > radiusSquared){
		return false;
	}

	// 体が大きいため、中心ではなく線が球に入る手前の面までの距離を返す(弾を体の表面へ向けて撃てるようにする)
	// 下限より手前にあるときは、手元から撃つと後ろへ飛んでしまうため狙わない
	const float surfaceDistance = alongDistance - std::sqrt(radiusSquared - distanceSquaredToLine);
	if(surfaceDistance < minDistance){
		return false;
	}
	outDistance = surfaceDistance;
	return true;
}

// ボスの弾とプレイヤーの当たり判定
int Boss::CheckHitToPlayer(const Model::BoundingSphere& playerSphere){
	return bulletPool_.CheckHitToPlayer(playerSphere);
}

// 当たり判定に使う、体の球をワールド座標系に置いた球を取得
// モデルの頂点から求めた境界球はとげの先まで含むため、体より大きくなり、体の外を通った弾まで当たってしまう
// そこで、モデルの体の球(中心は原点、半径はkModelBodyRadius)をそのまま使い、見た目の体に当たった弾だけを当てる(とげには当たらない)
Model::BoundingSphere Boss::GetHitSphere() const{
	return {position_, kModelBodyRadius * scale_};
}

// ボスのいるステージで、まだ倒されていないときに本体を描画し、撃った弾を描画する
void Boss::Draw(){
	if(IsActive()){
		obj_->Draw();
	}
	if(isBossStage_){
		bulletPool_.Draw();
	}
}
