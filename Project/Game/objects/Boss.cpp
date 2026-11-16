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

	ModelManager::GetInstance()->LoadModel(kBossModelPath);

	obj_ = std::make_unique<Obj3D>();
	obj_->Initialize(objCommon);
	obj_->SetModel(kBossModelPath);

	Reset();
}

// 戦う前の状態(体力満タン)に戻す
// 体力の最大値は、ImGuiで変えた値がやり直すたびに反映されるよう、ここで調整項目から読み直す
void Boss::Reset(){
	maxHp_ = (std::max)(static_cast<int>(GlobalVariables::GetInstance()->GetIntValue(kBossGroup,kPhase1HpKey)),kMinMaxHp);
	hp_ = maxHp_;
	isDefeated_ = false;
	bobTimer_ = 0.0f;
	if(Model::Material* mat = obj_->GetMaterial()){
		mat->color = kFullHpColor;
	}
}

// 位置・向きの更新と、プレイヤーの弾との当たり判定
void Boss::Update(bool isBossStage,const Vector3& playerPosition,PlayerBulletManager& bullets,bool canBeHit,float deltaTime){
	isBossStage_ = isBossStage;
	if(!isBossStage_ || isDefeated_){
		return;
	}

	// 調整項目から最新の値を取得(ImGui編集/ホットリロードが即反映される)
	GlobalVariables* gv = GlobalVariables::GetInstance();
	const Vector3 basePosition = gv->GetVector3Value(kBossGroup,kPositionKey);
	scale_ = (std::max)(gv->GetFloatValue(kBossGroup,kScaleKey),kMinScale);

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
			return;
		}
	}

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

// 当たり判定に使う、体の球をワールド座標系に置いた球を取得
// モデルの頂点から求めた境界球はとげの先まで含むため、体より大きくなり、体の外を通った弾まで当たってしまう
// そこで、モデルの体の球(中心は原点、半径はkModelBodyRadius)をそのまま使い、見た目の体に当たった弾だけを当てる(とげには当たらない)
Model::BoundingSphere Boss::GetHitSphere() const{
	return {position_, kModelBodyRadius * scale_};
}

// ボスのいるステージで、まだ倒されていないときに描画する
void Boss::Draw(){
	if(IsActive()){
		obj_->Draw();
	}
}
