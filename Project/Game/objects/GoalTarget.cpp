#include "GoalTarget.h"
#include "PlayerBulletManager.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "GlobalVariables.h"
#include "ParticleManager.h"
#include "Editor/RailEditor.h"
#include <algorithm>
#include <cmath>

namespace{
	// 的の表示に使用するモデル(普通の的と同じ、同心円の模様の円盤)
	const std::string kTargetModelPath = "Bullseye/bullseye.obj";
	// 調整項目名(体力の最大値・プレイヤーより先を並走する距離・レールからの高さ)
	const char* kMaxHpKey = "goalTargetMaxHp";
	const char* kAheadDistanceKey = "goalTargetAheadDistance";
	const char* kHeightKey = "goalTargetHeight";
	// 揺れ・高さの基準にする真上の向き
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};
}

GoalTarget::GoalTarget() = default;
GoalTarget::~GoalTarget() = default;

// 初期化処理
void GoalTarget::Initialize(Obj3dCommon* objCommon,const std::string& paramGroup){
	paramGroup_ = paramGroup;

	// 調整項目の登録(第3引数はデフォルト値。保存済みJSONがあればそちらが優先される)
	GlobalVariables* gv = GlobalVariables::GetInstance();
	gv->AddItem(paramGroup_,kMaxHpKey,static_cast<int32_t>(kDefaultMaxHp));
	gv->AddItem(paramGroup_,kAheadDistanceKey,kDefaultAheadDistance);
	gv->AddItem(paramGroup_,kHeightKey,kDefaultHeight);

	ModelManager::GetInstance()->LoadModel(kTargetModelPath);
	model_ = ModelManager::GetInstance()->FindModel(kTargetModelPath);

	obj_ = std::make_unique<Obj3D>();
	obj_->Initialize(objCommon);
	obj_->SetModel(kTargetModelPath);

	Reset();
}

// 現れる前の状態(体力満タン・非表示)に戻す
// 体力の最大値は、ImGuiで変えた値がやり直すたびに反映されるよう、ここで調整項目から読み直す
void GoalTarget::Reset(){
	maxHp_ = (std::max)(static_cast<int>(GlobalVariables::GetInstance()->GetIntValue(paramGroup_,kMaxHpKey)),kMinMaxHp);
	hp_ = maxHp_;
	isAppeared_ = false;
	isDestroyed_ = false;
	swayTimer_ = 0.0f;
	if(Model::Material* mat = obj_->GetMaterial()){
		mat->color = kFullHpColor;
	}
}

// 並走する位置の更新と、弾との当たり判定
void GoalTarget::Update(const RailEditor* railEditor,float playerRailT,bool isOnGoalRail,const Vector3& cameraPosition,PlayerBulletManager& bullets,float deltaTime){
	if(isDestroyed_){
		return;
	}

	// プレイヤーがゴールのレールに乗ったら現れる
	if(isOnGoalRail){
		isAppeared_ = true;
	}
	if(!isAppeared_){
		return;
	}

	// ゴールのレールに乗っている間だけ、プレイヤーより先の位置へ並走させる
	// 乗っていないとき(ゴールのレールから跳ねて落ちたときなど)は、別のレールの上へ移らないよう今の位置に留める
	if(isOnGoalRail){
		swayTimer_ += deltaTime;

		Vector3 forward;
		const Vector3 railPosition = ComputeAheadPosition(railEditor,playerRailT,forward);

		// 進行方向に対して直角な右方向へ左右に、真上へ上下に揺らし、まっすぐ並走するだけより狙いにくくする
		const Vector3 right = Normalize(Cross(kWorldUp,forward));
		const float height = GlobalVariables::GetInstance()->GetFloatValue(paramGroup_,kHeightKey);
		const float sideOffset = std::sin(swayTimer_ * kSwaySideSpeed) * kSwaySideAmplitude;
		const float upOffset = height + std::sin(swayTimer_ * kSwayUpSpeed) * kSwayUpAmplitude;
		position_ = railPosition + right * sideOffset + kWorldUp * upOffset;
	}

	// 弾が当たったら体力を減らし、当たった位置に火花を出して効いていることを見せる
	// 同じフレームに何発当たっても1発ずつ判定し、当たった弾はすべて消す
	const Model::BoundingSphere hitSphere = GetHitSphere();
	while(hp_ > 0 && bullets.CheckHit(hitSphere)){
		hp_ -= bullets.GetDamage();
		ParticleManager::GetInstance()->EmitSpark(position_);
	}
	if(hp_ <= 0){
		hp_ = 0;
		isDestroyed_ = true;
		return;
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

	// 円盤の模様の面(Z軸)をカメラへ向ける(Y軸回転で左右、X軸回転で上下を合わせる)
	const Vector3 toCameraVector = cameraPosition - position_;
	if(Length(toCameraVector) >= kMinFaceDistance){
		const Vector3 toCamera = Normalize(toCameraVector);
		const float horizontalLength = std::sqrt(toCamera.x * toCamera.x + toCamera.z * toCamera.z);
		obj_->SetRotate({std::atan2(-toCamera.y,horizontalLength), std::atan2(toCamera.x,toCamera.z), 0.0f});
	}
	obj_->SetTranslate(position_);
	obj_->SetScale({kScale, kScale, kScale});
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		obj_->SetCamera(activeCamera);
	}
	obj_->Update();
}

// プレイヤーの進行度から、レールに沿って並走する距離だけ先の位置と、そこでの進行方向を求める
// AdvanceTByDistance()は今の位置での進み具合で直線近似するため、長い距離を一度に進めると曲がったところでずれる
// そこで短い距離ずつ進め、実際に進んだ距離(前の位置との距離)を残りの距離から引いていく
Vector3 GoalTarget::ComputeAheadPosition(const RailEditor* railEditor,float playerRailT,Vector3& outForward) const{
	float remainingDistance = GlobalVariables::GetInstance()->GetFloatValue(paramGroup_,kAheadDistanceKey);
	float t = playerRailT;
	Vector3 position = railEditor->GetPositionOnRail(t);
	while(remainingDistance > 0.0f && t < kRailEndT){
		const float nextT = (std::min)(railEditor->AdvanceTByDistance(t,(std::min)(remainingDistance,kAheadStepDistance)),kRailEndT);
		const Vector3 nextPosition = railEditor->GetPositionOnRail(nextT);
		const float stepLength = Distance(position,nextPosition);
		if(nextT <= t || stepLength < kMinAheadStepLength){
			break;
		}
		remainingDistance -= stepLength;
		t = nextT;
		position = nextPosition;
	}

	outForward = railEditor->GetForwardOnRail(t);
	// 終点まで進んでも残った距離は、終点の進行方向へまっすぐ伸ばす
	if(remainingDistance > 0.0f && t >= kRailEndT){
		position += outForward * remainingDistance;
	}
	return position;
}

// レティクルの方向が当たり判定の球を通っているかを調べる
bool GoalTarget::FindAimed(const Vector3& cameraPosition,const Vector3& cameraForward,float minDistance,float& outDistance) const{
	if(!isAppeared_ || isDestroyed_){
		return false;
	}

	// 球の中心からカメラの前方の線へ下ろした垂線の足までの距離
	// 下限より手前にあるときは、手元から撃つと後ろへ飛んでしまうため狙わない
	const Model::BoundingSphere sphere = GetHitSphere();
	const Vector3 toCenter = sphere.center - cameraPosition;
	const float alongDistance = Dot(toCenter,cameraForward);
	if(alongDistance < minDistance){
		return false;
	}

	// 線と球の中心の距離が半径以内なら、線が球を通っている(平方のまま比べて平方根を省く)
	const float distanceSquaredToLine = Dot(toCenter,toCenter) - alongDistance * alongDistance;
	if(distanceSquaredToLine > sphere.radius * sphere.radius){
		return false;
	}
	outDistance = alongDistance;
	return true;
}

// 当たり判定に使う、モデルから求めたワールド座標系の境界球を取得
Model::BoundingSphere GoalTarget::GetHitSphere() const{
	if(!model_){
		return {position_, 0.0f};
	}
	return model_->GetBoundingSphere(position_,kScale);
}

// 現れていて、まだ壊れていないときに描画する
void GoalTarget::Draw(){
	if(isAppeared_ && !isDestroyed_){
		obj_->Draw();
	}
}
