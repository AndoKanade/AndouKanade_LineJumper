#include "HomingShotManager.h"
#include "TargetManager.h"
#include "GoalTarget.h"
#include "InkEffectManager.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "Model.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "GlobalVariables.h"
#include "Camera/CameraPose.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <utility>

namespace{
	// 弾の表示に使用するモデル(メインの弾と同じ、凹凸の無いなめらかな球)
	const std::string kShotModelPath = "SmoothSphere/smoothSphere.obj";
	// 弾の色(メインの弾と同じ、プレイヤーのインクの黄色)
	constexpr Vector4 kShotColor = {1.0f, 0.9f, 0.2f, 1.0f};
	// 真上の向き(背中のタンクの高さ・弾を開く向きを求めるのに使う)
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};

	// 調整項目の名前
	constexpr const char* kRangeKey = "homingShotRange";
	constexpr const char* kFlightTimeKey = "homingShotFlightTime";
	constexpr const char* kGoalDamageKey = "homingShotGoalDamage";

	/// <summary>
	/// 点から線分までの最短距離の2乗を求める(進路がほかの的の近くを通るかを、進路の上で調べた点の間も含めて調べるのに使う)
	/// </summary>
	/// <param name="point">点</param>
	/// <param name="segmentStart">線分の始点</param>
	/// <param name="segmentEnd">線分の終点</param>
	/// <returns>最短距離の2乗</returns>
	float DistanceSquaredToSegment(const Vector3& point,const Vector3& segmentStart,const Vector3& segmentEnd){
		const Vector3 segment = segmentEnd - segmentStart;
		const float segmentLengthSquared = Dot(segment,segment);
		// 線分の上で点に一番近い位置の割合(0: 始点 〜 1: 終点)。長さが無い線分は始点で調べる
		float rate = 0.0f;
		if(segmentLengthSquared > 0.0f){
			rate = std::clamp(Dot(point - segmentStart,segment) / segmentLengthSquared,0.0f,1.0f);
		}
		const Vector3 diff = point - (segmentStart + segment * rate);
		return Dot(diff,diff);
	}
}

HomingShotManager::HomingShotManager() = default;
HomingShotManager::~HomingShotManager() = default;

// 初期化処理
void HomingShotManager::Initialize(Obj3dCommon* objCommon,const std::string& paramGroup){
	objCommon_ = objCommon;
	paramGroup_ = paramGroup;

	// 調整項目の登録(第3引数はデフォルト値。保存済みJSONがあればそちらが優先される)
	GlobalVariables::GetInstance()->AddItem(paramGroup_,kRangeKey,kDefaultRange);
	GlobalVariables::GetInstance()->AddItem(paramGroup_,kFlightTimeKey,kDefaultFlightTime);
	GlobalVariables::GetInstance()->AddItem(paramGroup_,kGoalDamageKey,static_cast<int32_t>(kDefaultGoalDamage));

	ModelManager::GetInstance()->LoadModel(kShotModelPath);

	// 弾の生成
	// 撃つたびに生成すると無駄な処理が毎回発生するため、ここで最大数ぶんまとめて作り、以降は使い回す
	shots_.resize(kMaxShotCount);
	for(auto& shot : shots_){
		shot.obj = std::make_unique<Obj3D>();
		shot.obj->Initialize(objCommon_);
		shot.obj->SetModel(kShotModelPath);

		// 色は生成時に一度設定するだけでよいため、ここでインクの色にしておく
		// メインの弾と同じく、周りの明るさに関係なく光って見えるよう、ライティングは切る
		if(Model::Material* material = shot.obj->GetMaterial()){
			material->color = kShotColor;
			material->enableLighting = 0;
		}
	}
}

// 狙う的を選んで3発撃つ
bool HomingShotManager::Fire(const Vector3& playerPosition,const Vector3& cameraForward,const Matrix4x4& viewProjection,const TargetManager& targets,const GoalTarget& goalTarget){
	// 使い回す弾が1回分残っていなければ撃たない(途中までしか撃てないと、狙った的に弾が足りなくなるため)
	const int freeCount = static_cast<int>(std::count_if(shots_.begin(),shots_.end(),[](const Shot& shot){ return !shot.isAlive; }));
	if(freeCount < kShotsPerVolley){
		return false;
	}

	// 背中の向き(水平)を求める。照準の上下の可動範囲は限られているため、カメラの前方が真上・真下を向くことはないが、念のため調べる
	Vector3 backward = {-cameraForward.x, 0.0f, -cameraForward.z};
	if(Length(backward) < kDirectionEpsilon){
		return false;
	}

	std::vector<AimTarget> selectedTargets;
	SelectTargets(playerPosition,viewProjection,targets,goalTarget,selectedTargets);
	if(selectedTargets.empty()){
		return false;
	}

	LaunchFrame frame;
	frame.backward = Normalize(backward);
	// 前方(背中の逆)に対する右の向き
	frame.right = Normalize(Cross(kWorldUp,-frame.backward));
	frame.origin = playerPosition + kWorldUp * kTankHeight + frame.backward * kTankBackOffset;

	// 弾ごとに狙う的を割り振る
	// 的が n 個のとき、i 番目の弾は近い順で (i × n / 3) 番目の的へ向かう(3個なら1発ずつ、2個なら近い方に2発、1個なら3発とも)
	const int selectedCount = static_cast<int>(selectedTargets.size());
	std::array<AimTarget,kShotsPerVolley> assignedTargets{};
	for(int i = 0; i < kShotsPerVolley; ++i){
		assignedTargets[i] = selectedTargets[i * selectedCount / kShotsPerVolley];
	}
	// 左へ開く弾ほど左にある的へ向かうよう、的を右の向きに沿った位置の順に並べ、進路が交差しにくくする
	std::sort(assignedTargets.begin(),assignedTargets.end(),[&](const AimTarget& a,const AimTarget& b){
		return Dot(GetAimPosition(a,targets,goalTarget) - frame.origin,frame.right) < Dot(GetAimPosition(b,targets,goalTarget) - frame.origin,frame.right);
	});

	// 未使用の弾に、開く向きの順(左・上・右)で進路を決めて飛ばし始める
	int slot = 0;
	for(auto& shot : shots_){
		if(slot >= kShotsPerVolley){
			break;
		}
		if(shot.isAlive){
			continue;
		}
		shot.target = assignedTargets[slot];
		shot.controlPoints = PlanPath(frame,kSlotAngles[slot],shot.target,targets,goalTarget);
		shot.timer = 0.0f;
		shot.trailTimer = 0.0f;
		shot.isAlive = true;
		++slot;
	}
	return true;
}

// 狙う的を選ぶ
void HomingShotManager::SelectTargets(const Vector3& playerPosition,const Matrix4x4& viewProjection,const TargetManager& targets,const GoalTarget& goalTarget,std::vector<AimTarget>& outTargets) const{
	const float range = GlobalVariables::GetInstance()->GetFloatValue(paramGroup_,kRangeKey);

	// ほかの弾がもう向かっている的か(続けて撃ったときに、1発で壊れる的へ重ねて飛ばさないようにする)
	auto isTargeted = [this](int index){
		return std::any_of(shots_.begin(),shots_.end(),[index](const Shot& shot){ return shot.isAlive && !shot.target.isGoal && shot.target.index == index; });
	};
	// 範囲内にあり、画面に映っているか
	auto isInReach = [&](const Vector3& position,float& outDistance){
		outDistance = Length(position - playerPosition);
		return outDistance <= range && CameraPoseMath::IsInView(position,viewProjection);
	};

	// 狙える的を、プレイヤーからの距離と組にして集める
	std::vector<std::pair<float,AimTarget>> candidates;
	float distance = 0.0f;
	for(int i = 0; i < targets.GetTotalCount(); ++i){
		if(!targets.IsTargetAlive(i) || isTargeted(i)){
			continue;
		}
		if(isInReach(targets.GetTargetPosition(i),distance)){
			candidates.emplace_back(distance,AimTarget{i, false});
		}
	}
	// ゴールの的は体力があり何発も当てる的のため、ほかの弾が向かっていても選ぶ
	if(goalTarget.IsShootable() && isInReach(goalTarget.GetPosition(),distance)){
		candidates.emplace_back(distance,AimTarget{-1, true});
	}

	// 近い順に、1回に撃つ弾の数まで選ぶ
	std::sort(candidates.begin(),candidates.end(),[](const std::pair<float,AimTarget>& a,const std::pair<float,AimTarget>& b){ return a.first < b.first; });
	const size_t selectCount = (std::min)(candidates.size(),static_cast<size_t>(kShotsPerVolley));
	outTargets.clear();
	for(size_t i = 0; i < selectCount; ++i){
		outTargets.push_back(candidates[i].second);
	}
}

// 狙う的の今の中心の座標を取得する
const Vector3& HomingShotManager::GetAimPosition(const AimTarget& aim,const TargetManager& targets,const GoalTarget& goalTarget){
	return aim.isGoal?goalTarget.GetPosition():targets.GetTargetPosition(aim.index);
}

// 1発の進路を決める
std::array<Vector3,4> HomingShotManager::PlanPath(const LaunchFrame& frame,float slotAngle,const AimTarget& aim,const TargetManager& targets,const GoalTarget& goalTarget) const{
	const Vector3& targetPosition = GetAimPosition(aim,targets,goalTarget);
	const Vector3 toTarget = targetPosition - frame.origin;
	const float targetDistance = Length(toTarget);
	// 的がタンクと重なっているときは、前方へ向かうものとして進路を決める
	const Vector3 toTargetDirection = (targetDistance > kDirectionEpsilon)?Normalize(toTarget):-frame.backward;
	// 的の手前で向かう点を、プレイヤー側へ戻す距離(近い的では、的より後ろへ回り込みすぎないよう短くする)
	const float approachBack = (std::min)(kApproachBackDistance,targetDistance * kApproachBackMaxRate);

	std::array<Vector3,4> bestPath{};
	int bestObstacleCount = INT_MAX;
	for(float spreadScale : kDetourSpreadScales){
		for(float angleOffset : kDetourAngleOffsets){
			// 背中から見て、右を0として上へ回った向きへ開く
			const float angle = slotAngle + angleOffset;
			const Vector3 spreadDirection = frame.right * std::cos(angle) + kWorldUp * std::sin(angle);

			// 始点: 背中のタンク
			// 2番目: 背中側へ出て、開く向きへ離れた点(撃ち出した直後に向かう)
			// 3番目: 的から開く向きへずらし、プレイヤー側へ戻した点(的へ横から回り込んで向かう)
			// 終点: 狙った的の中心(的は動かないため、ここに着けば必ず当たる)
			const std::array<Vector3,4> path = {
				frame.origin,
				frame.origin + frame.backward * kLaunchBackDistance + spreadDirection * (kLaunchSpreadDistance * spreadScale),
				targetPosition + spreadDirection * (kApproachSpreadDistance * spreadScale) - toTargetDirection * approachBack,
				targetPosition
			};

			const int obstacleCount = CountPathObstacles(path,aim,targets,goalTarget);
			// ほかの的の近くを通らない進路が見つかったら、それに決める
			if(obstacleCount == 0){
				return path;
			}
			if(obstacleCount < bestObstacleCount){
				bestObstacleCount = obstacleCount;
				bestPath = path;
			}
		}
	}
	return bestPath;
}

// 進路が、狙っていない生きている的の近くを通る数を数える
int HomingShotManager::CountPathObstacles(const std::array<Vector3,4>& controlPoints,const AimTarget& aim,const TargetManager& targets,const GoalTarget& goalTarget) const{
	// 進路を決めた数の点で区切り、点と点を結んだ線分でほかの的の近くを通るかを調べる(点の間をすり抜けて見落とさないようにする)
	std::array<Vector3,kPathSampleCount + 1> samples;
	for(int i = 0; i <= kPathSampleCount; ++i){
		samples[i] = EvaluatePath(controlPoints,static_cast<float>(i) / static_cast<float>(kPathSampleCount));
	}

	// 指定した的の当たり判定(余白を足した球)の近くを通るか
	auto passesNear = [&samples](const Vector3& center,float hitRadius){
		const float avoidRadius = hitRadius + kAvoidMargin;
		const float avoidRadiusSquared = avoidRadius * avoidRadius;
		for(int s = 0; s < kPathSampleCount; ++s){
			if(DistanceSquaredToSegment(center,samples[s],samples[s + 1]) <= avoidRadiusSquared){
				return true;
			}
		}
		return false;
	};

	int obstacleCount = 0;
	for(int i = 0; i < targets.GetTotalCount(); ++i){
		if((!aim.isGoal && i == aim.index) || !targets.IsTargetAlive(i)){
			continue;
		}
		if(passesNear(targets.GetTargetPosition(i),targets.GetTargetMaxHitRadius(i))){
			++obstacleCount;
		}
	}
	// ゴールの的は動くため、撃つ瞬間の位置でよける
	if(!aim.isGoal && goalTarget.IsShootable() && passesNear(goalTarget.GetPosition(),goalTarget.GetHitRadius())){
		++obstacleCount;
	}
	return obstacleCount;
}

// 進路の進行度 t の位置を求める(3次ベジェ曲線)
Vector3 HomingShotManager::EvaluatePath(const std::array<Vector3,4>& controlPoints,float t){
	const float u = 1.0f - t;
	return controlPoints[0] * (u * u * u)
		+ controlPoints[1] * (kBezierInnerWeight * u * u * t)
		+ controlPoints[2] * (kBezierInnerWeight * u * t * t)
		+ controlPoints[3] * (t * t * t);
}

// 進路の進行度 t での進む向きを求める(3次ベジェ曲線を t で微分したもの)
Vector3 HomingShotManager::EvaluatePathTangent(const std::array<Vector3,4>& controlPoints,float t){
	const float u = 1.0f - t;
	return (controlPoints[1] - controlPoints[0]) * (kBezierInnerWeight * u * u)
		+ (controlPoints[2] - controlPoints[1]) * (kBezierTangentCrossWeight * u * t)
		+ (controlPoints[3] - controlPoints[2]) * (kBezierInnerWeight * t * t);
}

// 弾を進め、着いた弾の的を壊す
void HomingShotManager::Update(float deltaTime,TargetManager& targets,GoalTarget& goalTarget){
	const float flightTime = (std::max)(GlobalVariables::GetInstance()->GetFloatValue(paramGroup_,kFlightTimeKey),kMinFlightTime);

	for(auto& shot : shots_){
		if(!shot.isAlive){
			continue;
		}

		// ゴールの的はプレイヤーの先を並走して動くため、行き先(終点)と的の手前で向かう点を、的が動いた分だけ動かして必ず着くようにする
		// 先に壊されたときは、最後にいた位置へ向かう
		if(shot.target.isGoal && goalTarget.IsShootable()){
			const Vector3 targetMovement = goalTarget.GetPosition() - shot.controlPoints[3];
			shot.controlPoints[2] += targetMovement;
			shot.controlPoints[3] += targetMovement;
		}

		shot.timer += deltaTime;
		const float t = (std::min)(shot.timer / flightTime,1.0f);
		const Vector3 position = EvaluatePath(shot.controlPoints,t);
		// 進む向き(向きを求められないときは、的へ向かう最後の向きにする)
		Vector3 direction = EvaluatePathTangent(shot.controlPoints,t);
		if(Length(direction) < kDirectionEpsilon){
			direction = shot.controlPoints[3] - shot.controlPoints[2];
		}
		direction = (Length(direction) < kDirectionEpsilon)?kWorldUp:Normalize(direction);

		// 的に着いたら、普通の的・鍵の的は生きていれば壊す(破片などの演出と連鎖数の加算は、弾が当たって壊れたときと同じく GameScene が行う)
		// ゴールの的は体力を減らし、来た向きへしぶきを跳ね返す(体力が0になったときのクリアの処理は GameScene が行う)
		// メインの弾などに先に壊されていたら、その位置でしぶきを出して消える
		if(t >= 1.0f){
			bool isHit = false;
			if(shot.target.isGoal){
				isHit = goalTarget.IsShootable();
				if(isHit){
					goalTarget.TakeHit(GlobalVariables::GetInstance()->GetIntValue(paramGroup_,kGoalDamageKey),-direction);
				}
			} else{
				isHit = targets.DestroyTarget(shot.target.index);
			}
			if(!isHit && inkEffect_){
				inkEffect_->EmitInkSplash(position,direction,kPopSplashCount);
			}
			shot.isAlive = false;
			continue;
		}

		// 飛んでいる間は、決まった間隔で弾からインクの粒を垂らし、曲がって飛んだ跡を見せる
		if(inkEffect_){
			shot.trailTimer += deltaTime;
			while(shot.trailTimer >= kTrailInterval){
				shot.trailTimer -= kTrailInterval;
				inkEffect_->EmitBulletTrail(position,kShotColor);
			}
		}

		UpdateShotTransform(shot,position,direction);
	}
}

// 弾の描画用の行列を更新する
void HomingShotManager::UpdateShotTransform(Shot& shot,const Vector3& position,const Vector3& direction){
	if(!shot.obj){
		return;
	}
	// 球モデルのZ軸を進む向きへ回す(Y軸回転で左右、X軸回転で上下を合わせる)
	const float horizontalLength = std::sqrt(direction.x * direction.x + direction.z * direction.z);
	shot.obj->SetRotate({std::atan2(-direction.y,horizontalLength), std::atan2(direction.x,direction.z), 0.0f});
	shot.obj->SetTranslate(position);
	shot.obj->SetScale({kScale * kStretchWidthRate, kScale * kStretchWidthRate, kScale * kStretchLengthRate});
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		shot.obj->SetCamera(activeCamera);
	}
	shot.obj->Update();
}

// 飛んでいる弾を描画する
void HomingShotManager::Draw(){
	for(auto& shot : shots_){
		if(shot.isAlive && shot.obj){
			shot.obj->Draw();
		}
	}
}

// すべての弾を消す(生成した弾は消さずに、次に撃つときに使い回す)
void HomingShotManager::Clear(){
	for(auto& shot : shots_){
		shot.isAlive = false;
	}
}
