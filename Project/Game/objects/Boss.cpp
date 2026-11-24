#include "Boss.h"
#include "PlayerBulletManager.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "GlobalVariables.h"
#include "InkEffectManager.h"
#include "Easing.h"
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
	// 調整項目名(2形態目の体力・変わる演出の時間・上へ上げる量・攻撃の間隔・輪を回る速さの倍率・現れる輪のレールの番号)
	const char* kPhase2HpKey = "phase2Hp";
	const char* kPhaseChangeDurationKey = "phaseChangeDuration";
	const char* kPhase2RiseKey = "phase2Rise";
	const char* kPhase2AttackIntervalKey = "phase2AttackInterval";
	const char* kPhase2RailSpeedRateKey = "phase2RailSpeedRate";
	const char* kPhase2RailIndexKey = "phase2RailIndex";
	// 調整項目名(輪を塗り返す攻撃の、予告の時間と攻撃中の時間)
	const char* kRingWarnTimeKey = "ringWarnTime";
	const char* kRingDangerTimeKey = "ringDangerTime";
	// 調整項目名(先読みの連射・扇状の弾の1発のダメージ、追尾弾のダメージ、塗られた輪に乗っている間の1秒あたりのダメージ)
	const char* kBulletDamageKey = "bulletDamage";
	const char* kHomingDamageKey = "homingDamage";
	const char* kRingDamagePerSecondKey = "ringDamagePerSecond";
	// 調整項目名(弾を撃つ前の予備動作の時間)
	const char* kTelegraphTimeKey = "telegraphTime";
	// 点滅の sin の結果(-1〜1)を 0〜1 にする係数
	constexpr float kHalf = 0.5f;
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
	gv->AddItem(kBossGroup,kPhase2HpKey,static_cast<int32_t>(kDefaultPhase2Hp));
	gv->AddItem(kBossGroup,kPhaseChangeDurationKey,kDefaultPhaseChangeDuration);
	gv->AddItem(kBossGroup,kPhase2RiseKey,kDefaultPhase2Rise);
	gv->AddItem(kBossGroup,kPhase2AttackIntervalKey,kDefaultPhase2AttackInterval);
	gv->AddItem(kBossGroup,kPhase2RailSpeedRateKey,kDefaultPhase2RailSpeedRate);
	gv->AddItem(kBossGroup,kPhase2RailIndexKey,static_cast<int32_t>(kDefaultPhase2RailIndex));
	gv->AddItem(kBossGroup,kRingWarnTimeKey,kDefaultRingWarnTime);
	gv->AddItem(kBossGroup,kRingDangerTimeKey,kDefaultRingDangerTime);
	gv->AddItem(kBossGroup,kTelegraphTimeKey,kDefaultTelegraphTime);
	gv->AddItem(kBossGroup,kBulletDamageKey,kDefaultBulletDamage);
	gv->AddItem(kBossGroup,kHomingDamageKey,kDefaultHomingDamage);
	gv->AddItem(kBossGroup,kRingDamagePerSecondKey,kDefaultRingDamagePerSecond);

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

// 戦う前の状態(1形態目・体力満タン・弾なし)に戻す
void Boss::Reset(){
	ResetToPhase(kFirstPhase);
}

// 指定した形態の最初の状態(体力満タン・弾なし)にする
// 体力の最大値は、ImGuiで変えた値がやり直すたびに反映されるよう、ここで調整項目から読み直す
void Boss::ResetToPhase(int phase){
	phase_ = (phase >= kSecondPhase)?kSecondPhase:kFirstPhase;
	const char* hpKey = (phase_ == kSecondPhase)?kPhase2HpKey:kPhase1HpKey;
	maxHp_ = (std::max)(static_cast<int>(GlobalVariables::GetInstance()->GetIntValue(kBossGroup,hpKey)),kMinMaxHp);
	hp_ = maxHp_;
	isDefeated_ = false;
	isVanished_ = false;
	defeatTimer_ = 0.0f;
	bobTimer_ = 0.0f;
	// 形態が変わる演出の途中でも、演出を終えた状態から始める
	phaseChangeTimer_ = 0.0f;
	hasPhaseChanged_ = false;
	if(Model::Material* mat = obj_->GetMaterial()){
		mat->color = (phase_ == kSecondPhase)?kPhase2FullHpColor:kFullHpColor;
	}
	ResetAttack();
}

// 1形態目の体力が尽きたときに、2形態目へ変わり始める
// 体力を2形態目の最大値まで戻し、撃った弾を消し、変わる演出の間は弾が当たらず攻撃もしないようにする
void Boss::BeginPhaseChange(){
	GlobalVariables* gv = GlobalVariables::GetInstance();
	phase_ = kSecondPhase;
	maxHp_ = (std::max)(static_cast<int>(gv->GetIntValue(kBossGroup,kPhase2HpKey)),kMinMaxHp);
	hp_ = maxHp_;
	phaseChangeDuration_ = gv->GetFloatValue(kBossGroup,kPhaseChangeDurationKey);
	phaseChangeTimer_ = phaseChangeDuration_;
	hasPhaseChanged_ = true;
	ResetAttack();
}

// このフレームで2形態目に変わり始めたかを受け取る(1回読むと消える)
bool Boss::ConsumePhaseChanged(){
	const bool hasChanged = hasPhaseChanged_;
	hasPhaseChanged_ = false;
	return hasChanged;
}

// 2形態目で現れる輪のレールの番号(調整項目)を取得
int Boss::GetPhase2RailIndex() const{
	return static_cast<int>(GlobalVariables::GetInstance()->GetIntValue(kBossGroup,kPhase2RailIndexKey));
}

// 今の形態でプレイヤーが輪を回る速さに掛ける倍率を取得
float Boss::GetRailSpeedRate() const{
	if(phase_ < kSecondPhase){
		return 1.0f;
	}
	return GlobalVariables::GetInstance()->GetFloatValue(kBossGroup,kPhase2RailSpeedRateKey);
}

// 輪を塗り返す攻撃で、対象の輪の色を寄せる先の色を取得
Vector4 Boss::GetRingAttackColor() const{
	return (ringAttackState_ == RingAttackState::Danger)?kRingDangerColor:kRingWarnColor;
}

// 輪を塗り返す攻撃で、対象の輪の色を寄せる割合を取得
// 予告の間は0〜1を行き来させて点滅させ、攻撃中は寄せきった色にする
float Boss::GetRingAttackTintRate() const{
	switch(ringAttackState_){
	case RingAttackState::Warning:
		return kHalf + kHalf * std::sin(ringAttackTimer_ * kRingWarnBlinkSpeed);
	case RingAttackState::Danger:
		return 1.0f;
	case RingAttackState::None:
	default:
		return 0.0f;
	}
}

// 体力は残したまま、撃った弾を消して攻撃を最初の待ち時間からやり直す
void Boss::ResetAttack(){
	bulletPool_.Clear();
	attackIndex_ = 0;
	attackTimer_ = GlobalVariables::GetInstance()->GetFloatValue(kBossGroup,kAttackIntervalKey);
	burstShotsLeft_ = 0;
	telegraphRate_ = 0.0f;
	chargeEmitTimer_ = 0.0f;
	// 輪を塗り返す攻撃も、予告・攻撃中の途中なら止める
	ringAttackState_ = RingAttackState::None;
	ringAttackRailIndex_ = kNoRingAttackRail;
	ringAttackTimer_ = 0.0f;
	// やり直しでプレイヤーの位置が飛ぶため、速さは次のフレームから求め直す
	playerVelocity_ = {0.0f, 0.0f, 0.0f};
	hasPreviousPlayerPosition_ = false;
}

// 位置・向き・攻撃の更新と、プレイヤーの弾との当たり判定
void Boss::Update(bool isBossStage,const Vector3& playerPosition,PlayerBulletManager& bullets,bool canBeHit,bool canAttack,int playerRailIndex,float deltaTime){
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

	// 2形態目へ変わる演出を進める(0〜1の進み具合。演出をしていないときは1)
	const bool isChangingPhase = phaseChangeTimer_ > 0.0f;
	phaseChangeTimer_ = (std::max)(phaseChangeTimer_ - deltaTime,0.0f);
	const float phaseChangeProgress = (phaseChangeDuration_ > 0.0f)?1.0f - phaseChangeTimer_ / phaseChangeDuration_:1.0f;

	// 決めた位置を中心に、上下へゆっくり揺らして浮かんでいるように見せる
	bobTimer_ += deltaTime;
	position_ = basePosition;
	position_.y += std::sin(bobTimer_ * gv->GetFloatValue(kBossGroup,kBobSpeedKey)) * gv->GetFloatValue(kBossGroup,kBobAmplitudeKey);
	// 2形態目は、変わる演出の間に上へ浮き上がり、そのまま1形態目より高い位置に留まる(体も一瞬ふくらませる)
	if(phase_ == kSecondPhase){
		position_.y += gv->GetFloatValue(kBossGroup,kPhase2RiseKey) * Easing::EaseOutCubic(phaseChangeProgress);
		scale_ *= 1.0f + kPhaseChangeScalePulse * std::sin(phaseChangeProgress * kPi);
	}

	// 輪を塗り返す攻撃の予告・攻撃中の時間を進める
	UpdateRingAttack(deltaTime);

	// 弾が当たったら体力を減らし、当たった位置に火花を出して効いていることを見せる
	// 同じフレームに何発当たっても1発ずつ判定し、当たった弾はすべて消す
	// 2形態目へ変わる演出の間は当たらない(変わった直後の体力を、演出の間に減らされないようにする)
	if(canBeHit && !isChangingPhase){
		const Model::BoundingSphere hitSphere = GetHitSphere();
		while(hp_ > 0 && bullets.CheckHit(hitSphere)){
			hp_ -= bullets.GetDamage();
			// 火花は、体の中心からプレイヤーの方へ向いた体の表面に出す(中心に出すと体に隠れて見えない)
			const Vector3 toPlayer = playerPosition - position_;
			const Vector3 sparkPosition = (Length(toPlayer) >= kMinFaceDistance)?position_ + Normalize(toPlayer) * hitSphere.radius:position_;
			// インクのしぶきも同じ位置から、プレイヤーの方へ跳ねさせる(重なっていて向きが定まらないときは真上へ跳ねさせる)
			const Vector3 splashDirection = (Length(toPlayer) >= kMinFaceDistance)?Normalize(toPlayer):kWorldUp;
			if(inkEffect_){
				inkEffect_->EmitSpark(sparkPosition);
				inkEffect_->EmitHitSplash(sparkPosition,splashDirection);
			}
		}
		if(hp_ <= 0 && phase_ == kFirstPhase){
			// 1形態目の体力が尽きたら、倒れずに2形態目へ変わる
			BeginPhaseChange();
		} else if(hp_ <= 0){
			hp_ = 0;
			isDefeated_ = true;
			// 倒した後に残った弾・輪の塗り返しでやられないよう、撃った弾を消して攻撃を止める
			ResetAttack();
			return;
		}
	}

	// プレイヤーが輪に乗っている間、決まった順番で攻撃する(2形態目へ変わる演出の間は攻撃しない)
	UpdateAttack(playerPosition,canAttack && phaseChangeTimer_ <= 0.0f,playerRailIndex,deltaTime);

	// 予備動作の間は、周りから光の粒を体へ吸い込ませ、撃つ瞬間(攻撃の待ち時間が尽きる瞬間)に体へ届くようにする
	// 時間の刻みに関係なく同じ量になるよう、決まった間隔ごとに出す
	if(telegraphRate_ > 0.0f && inkEffect_){
		const float bodyRadius = GetHitSphere().radius;
		chargeEmitTimer_ += deltaTime;
		while(chargeEmitTimer_ >= kChargeEmitInterval){
			chargeEmitTimer_ -= kChargeEmitInterval;
			inkEffect_->EmitBossCharge(position_,bodyRadius,attackTimer_);
		}
	} else{
		chargeEmitTimer_ = 0.0f;
	}

	// 減った体力の割合に合わせて、満タンの色から残りわずかの色へ近づける(満タンの色は形態ごとに変える)
	if(Model::Material* mat = obj_->GetMaterial()){
		const Vector4& fullHpColor = (phase_ == kSecondPhase)?kPhase2FullHpColor:kFullHpColor;
		const float damageRate = 1.0f - static_cast<float>(hp_) / static_cast<float>(maxHp_);
		// 予備動作の間は、撃つ直前へ向けてだんだん白く光らせる(進み具合の2乗で、撃つ直前に一気に白くする)
		const float telegraphColorRate = kTelegraphColorRate * telegraphRate_ * telegraphRate_;
		mat->color = {
			Lerp(Lerp(fullHpColor.x,kLowHpColor.x,damageRate),kTelegraphColor.x,telegraphColorRate),
			Lerp(Lerp(fullHpColor.y,kLowHpColor.y,damageRate),kTelegraphColor.y,telegraphColorRate),
			Lerp(Lerp(fullHpColor.z,kLowHpColor.z,damageRate),kTelegraphColor.z,telegraphColorRate),
			Lerp(Lerp(fullHpColor.w,kLowHpColor.w,damageRate),kTelegraphColor.w,telegraphColorRate),
		};
	}

	// 一つ目のある正面(Z軸)を、Y軸回転でプレイヤーの方へ向ける(体は直立させたままにする)
	const Vector3 toPlayerXZ = {playerPosition.x - position_.x, 0.0f, playerPosition.z - position_.z};
	if(Length(toPlayerXZ) >= kMinFaceDistance){
		obj_->SetRotate({0.0f, std::atan2(toPlayerXZ.x,toPlayerXZ.z), 0.0f});
	}
	obj_->SetTranslate(position_);
	// 予備動作の間は、撃つ直前へ向けて体をふくらませ、撃った瞬間に元の大きさへ戻す(当たり判定の大きさは変えない)
	const float visualScale = scale_ * (1.0f + kTelegraphScaleRate * telegraphRate_ * telegraphRate_);
	obj_->SetScale({visualScale, visualScale, visualScale});
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
void Boss::UpdateAttack(const Vector3& playerPosition,bool canAttack,int playerRailIndex,float deltaTime){
	GlobalVariables* gv = GlobalVariables::GetInstance();
	// 2形態目は、攻撃の間隔を短くする
	const float attackInterval = gv->GetFloatValue(kBossGroup,(phase_ == kSecondPhase)?kPhase2AttackIntervalKey:kAttackIntervalKey);

	// プレイヤーが輪に乗っていない間は撃たない。乗った直後にすぐ撃たれないよう、待ち時間を戻し、連射の途中も打ち切る
	if(!canAttack){
		attackTimer_ = attackInterval;
		burstShotsLeft_ = 0;
		telegraphRate_ = 0.0f;
		return;
	}

	attackTimer_ -= deltaTime;

	// 弾を撃つ攻撃の前は、待ち時間の最後の telegraphTime 秒を予備動作にして、撃つタイミングを見せる
	// 連射の2発目以降(1発目の予備動作で足りる)と、輪の塗り返し(輪の点滅が予告になる)では行わない
	const float telegraphTime = gv->GetFloatValue(kBossGroup,kTelegraphTimeKey);
	const bool telegraphs = burstShotsLeft_ <= 0 && GetAttackType(attackIndex_) != AttackType::RingInk && telegraphTime > 0.0f;
	telegraphRate_ = telegraphs?std::clamp(1.0f - attackTimer_ / telegraphTime,0.0f,1.0f):0.0f;

	if(attackTimer_ > 0.0f){
		return;
	}
	// 撃った瞬間に、ふくらんだ体と色を元に戻す
	telegraphRate_ = 0.0f;

	const float bulletSpeed = (std::max)(gv->GetFloatValue(kBossGroup,kBulletSpeedKey),kMinBulletSpeed);
	const float bulletDamage = gv->GetFloatValue(kBossGroup,kBulletDamageKey);

	switch(GetAttackType(attackIndex_)){
	case AttackType::RingInk:
		// プレイヤーが乗っている輪を、予告のあと敵のインクで塗り返す(前の塗り返しが続いている間・輪に乗っていない間は始めない)
		// 予告と攻撃中の時間は弾の攻撃と別に進めるため、すぐ次の攻撃の待ち時間に入る
		if(ringAttackState_ == RingAttackState::None && playerRailIndex >= 0){
			ringAttackState_ = RingAttackState::Warning;
			ringAttackRailIndex_ = playerRailIndex;
			ringAttackTimer_ = 0.0f;
		}
		break;

	case AttackType::Fan:
	{
		// プレイヤーが進む先を中心に、左右へ角度をつけた弾を同時に撃つ
		// 中央の弾を基準(添字fanCount / 2)として、そこからのずれ分だけ角度をつける
		const int fanCount = (std::max)(static_cast<int>(gv->GetIntValue(kBossGroup,kFanCountKey)),kMinShotCount);
		const float fanAngle = gv->GetFloatValue(kBossGroup,kFanAngleKey);
		const Vector3 target = ComputeLeadTarget(playerPosition,bulletSpeed);
		const int centerIndex = fanCount / 2;
		for(int i = 0; i < fanCount; ++i){
			FireToward(target,static_cast<float>(i - centerIndex) * fanAngle,bulletSpeed,false,bulletDamage);
		}
		break;
	}
	case AttackType::Homing:
	{
		// 発射後もプレイヤーを追い続ける遅い弾を1発だけ撃つ(追いかけるため先読みはしない)
		const float homingSpeed = (std::max)(gv->GetFloatValue(kBossGroup,kHomingSpeedKey),kMinBulletSpeed);
		FireToward({playerPosition.x, playerPosition.y + kPlayerAimHeight, playerPosition.z},0.0f,homingSpeed,true,gv->GetFloatValue(kBossGroup,kHomingDamageKey));
		break;
	}
	case AttackType::Burst:
	default:
		// プレイヤーが進む先を狙った弾を、短い間隔で続けて撃つ(1発ごとに狙い直す)
		if(burstShotsLeft_ <= 0){
			burstShotsLeft_ = (std::max)(static_cast<int>(gv->GetIntValue(kBossGroup,kBurstCountKey)),kMinShotCount);
		}
		FireToward(ComputeLeadTarget(playerPosition,bulletSpeed),0.0f,bulletSpeed,false,bulletDamage);
		--burstShotsLeft_;
		// 連射の残りがあるときは、攻撃の種類を進めずに次の1発を待つ
		if(burstShotsLeft_ > 0){
			attackTimer_ = gv->GetFloatValue(kBossGroup,kBurstIntervalKey);
			return;
		}
		break;
	}

	// 次の攻撃へ切り替え、攻撃の間隔だけ待つ
	attackIndex_ = (attackIndex_ + 1) % GetAttackOrderCount();
	attackTimer_ = attackInterval;
}

// 輪を塗り返す攻撃の予告・攻撃中の時間を進める
// 予告の時間がたったら攻撃中に、攻撃中の時間がたったら終わりにする
void Boss::UpdateRingAttack(float deltaTime){
	if(ringAttackState_ == RingAttackState::None){
		return;
	}

	GlobalVariables* gv = GlobalVariables::GetInstance();
	ringAttackTimer_ += deltaTime;
	if(ringAttackState_ == RingAttackState::Warning && ringAttackTimer_ >= gv->GetFloatValue(kBossGroup,kRingWarnTimeKey)){
		ringAttackState_ = RingAttackState::Danger;
		ringAttackTimer_ = 0.0f;
	} else if(ringAttackState_ == RingAttackState::Danger && ringAttackTimer_ >= gv->GetFloatValue(kBossGroup,kRingDangerTimeKey)){
		ringAttackState_ = RingAttackState::None;
		ringAttackRailIndex_ = kNoRingAttackRail;
		ringAttackTimer_ = 0.0f;
	}
}

// 今の形態の攻撃の順番の、index番目の種類を取得
Boss::AttackType Boss::GetAttackType(size_t index) const{
	if(phase_ == kSecondPhase){
		return kPhase2AttackOrder[index % kPhase2AttackOrderCount];
	}
	return kPhase1AttackOrder[index % kPhase1AttackOrderCount];
}

// 今の形態の攻撃の順番の要素数を取得
size_t Boss::GetAttackOrderCount() const{
	return (phase_ == kSecondPhase)?kPhase2AttackOrderCount:kPhase1AttackOrderCount;
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
void Boss::FireToward(const Vector3& target,float yawOffset,float speed,bool isHoming,float damage){
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
	bulletPool_.Fire(position_ + direction * GetHitSphere().radius,direction,speed,isHoming,damage);
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
float Boss::CheckHitToPlayer(const Model::BoundingSphere& playerSphere){
	float maxDamage = 0.0f;
	bulletPool_.CheckHitToPlayer(playerSphere,&maxDamage);
	return maxDamage;
}

// 輪を塗り返す攻撃の、塗られた輪に乗っている間に受ける1秒あたりのダメージ(調整項目)を取得
float Boss::GetRingDamagePerSecond() const{
	return GlobalVariables::GetInstance()->GetFloatValue(kBossGroup,kRingDamagePerSecondKey);
}

// 当たり判定に使う、体の球をワールド座標系に置いた球を取得
// モデルの頂点から求めた境界球はとげの先まで含むため、体より大きくなり、体の外を通った弾まで当たってしまう
// そこで、モデルの体の球(中心は原点、半径はkModelBodyRadius)をそのまま使い、見た目の体に当たった弾だけを当てる(とげには当たらない)
Model::BoundingSphere Boss::GetHitSphere() const{
	return {position_, kModelBodyRadius * scale_};
}

// 倒されてから消えるまでの動きを進める
// 倒された位置を中心に体を小刻みに震わせ、今の色と白の間で点滅させて、壊れかけていることを見せる
void Boss::UpdateDefeated(float deltaTime){
	if(!isDefeated_ || isVanished_){
		return;
	}
	defeatTimer_ += deltaTime;

	const float shakeWidth = GetBodyRadius() * kDefeatShakeRate;
	const Vector3 shakeOffset = {
		std::sin(defeatTimer_ * kDefeatShakeSpeed.x) * shakeWidth,
		std::sin(defeatTimer_ * kDefeatShakeSpeed.y) * shakeWidth,
		std::sin(defeatTimer_ * kDefeatShakeSpeed.z) * shakeWidth
	};
	obj_->SetTranslate(position_ + shakeOffset);

	if(Model::Material* mat = obj_->GetMaterial()){
		const float blinkRate = kHalf + kHalf * std::sin(defeatTimer_ * kDefeatBlinkSpeed);
		mat->color = {
			Lerp(kLowHpColor.x,kTelegraphColor.x,blinkRate),
			Lerp(kLowHpColor.y,kTelegraphColor.y,blinkRate),
			Lerp(kLowHpColor.z,kTelegraphColor.z,blinkRate),
			Lerp(kLowHpColor.w,kTelegraphColor.w,blinkRate),
		};
	}

	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		obj_->SetCamera(activeCamera);
	}
	obj_->Update();
}

// ボスのいるステージで、まだ消えていないときに本体を描画し、撃った弾を描画する
void Boss::Draw(){
	if(IsShown()){
		obj_->Draw();
	}
	if(isBossStage_){
		bulletPool_.Draw();
	}
}
