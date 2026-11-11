#include "RailEditor.h"
#include "RailEditorInternal.h"
#include <algorithm>
#include <cmath>

// RailEditorのうち、曲線の計算(レール上の位置・向き・速さ、一番近いレール、距離で進める処理)と、制御点・チェックポイント・統計の問い合わせをまとめたファイル

using namespace RailEditorInternal;

namespace{
	// 走り切るまでの秒数を求めるとき、細かく分けた1区間の中で速さを読む位置の割合(中央で読む)
	constexpr float kStatsSpeedSampleRate = 0.5f;
}

// 進行度tと制御点配列から、レール上の座標を計算する(GetPositionOnRail・全レール描画で共用)
Vector3 RailEditor::ComputePositionOnRail(const std::vector<ControlPoint>& controlPoints,float t) const{
	// Catmull-Rom補間には最低4点必要
	if(controlPoints.size() < 4){
		return {0.0f, 0.0f, 0.0f};
	}

	// tを区間数でスケールし、現在の区間(segment)を算出
	size_t numSegments = controlPoints.size() - 3;
	float scaledT = t * static_cast<float>(numSegments);
	size_t segment = static_cast<size_t>(scaledT);

	if(segment >= numSegments){
		segment = numSegments - 1;
	}

	// 区間内でのローカルな進行度(0〜1)
	float localT = scaledT - static_cast<float>(segment);

	const Vector3& p0 = controlPoints[segment].position;
	const Vector3& p1 = controlPoints[segment + 1].position;
	const Vector3& p2 = controlPoints[segment + 2].position;
	const Vector3& p3 = controlPoints[segment + 3].position;

	return CatmullRom(p0,p1,p2,p3,localT);
}

// 進行度t(0〜1)からレール上の座標を取得(対象はアクティブなレール)
Vector3 RailEditor::GetPositionOnRail(float t) const{
	return ComputePositionOnRail(rails_[activeRailIndex_].controlPoints,t);
}

// 指定ワールド座標に最も近いレール上の進行度tを計算する
// 手順: まずkNearestSearchSampleCount分割で粗くサンプリングして最も近い点を求め、
// その前後の区間だけをkNearestRefineIterationCount回の三分探索で絞り込み、精度を上げる
float RailEditor::ComputeNearestTOnRail(const std::vector<ControlPoint>& controlPoints,const Vector3& worldPos) const{
	// Catmull-Rom補間には最低4点必要
	if(controlPoints.size() < 4){
		return 0.0f;
	}

	// 粗いサンプリングで最も近い点を探す
	float bestT = 0.0f;
	float bestDistSq = -1.0f;
	for(int i = 0; i <= kNearestSearchSampleCount; ++i){
		float t = static_cast<float>(i) / static_cast<float>(kNearestSearchSampleCount);
		Vector3 pos = ComputePositionOnRail(controlPoints,t);
		Vector3 diff = pos - worldPos;
		float distSq = Dot(diff,diff);
		if(bestDistSq < 0.0f || distSq < bestDistSq){
			bestDistSq = distSq;
			bestT = t;
		}
	}

	// 粗探索で見つけた点の前後の区間を三分探索でさらに絞り込む
	float rangeWidth = 1.0f / static_cast<float>(kNearestSearchSampleCount);
	float lo = bestT - rangeWidth;
	float hi = bestT + rangeWidth;
	if(lo < 0.0f) lo = 0.0f;
	if(hi > 1.0f) hi = 1.0f;

	for(int i = 0; i < kNearestRefineIterationCount; ++i){
		float m1 = lo + (hi - lo) / 3.0f;
		float m2 = hi - (hi - lo) / 3.0f;

		Vector3 diff1 = ComputePositionOnRail(controlPoints,m1) - worldPos;
		Vector3 diff2 = ComputePositionOnRail(controlPoints,m2) - worldPos;

		if(Dot(diff1,diff1) < Dot(diff2,diff2)){
			hi = m2;
		} else{
			lo = m1;
		}
	}

	return (lo + hi) * 0.5f;
}

// 全レールの中から、指定したワールド座標に最も近いレール・進行度・距離を求める(着地先レールの探索用)
RailEditor::NearestRailResult RailEditor::FindNearestRail(const Vector3& worldPos,int excludeRailIndex) const{
	NearestRailResult result;
	float bestDistance = -1.0f;

	for(size_t i = 0; i < rails_.size(); ++i){
		// 除外指定されたレールと、鍵の的が残っていて出ていないレールは探索しない
		if(static_cast<int>(i) == excludeRailIndex || rails_[i].isLocked){
			continue;
		}

		float t = ComputeNearestTOnRail(rails_[i].controlPoints,worldPos);
		Vector3 nearestPos = ComputePositionOnRail(rails_[i].controlPoints,t);
		float distance = Distance(nearestPos,worldPos);

		if(bestDistance < 0.0f || distance < bestDistance){
			bestDistance = distance;
			result.railIndex = static_cast<int>(i);
			result.t = t;
			result.distance = distance;
			result.position = nearestPos;
		}
	}

	return result;
}

// 進行度t(0〜1)からレール上の回転(オイラー角)を取得(対象はアクティブなレール)
Vector3 RailEditor::GetRotationOnRail(float t) const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	Vector3 forward = GetForwardOnRail(t);

	// Y成分をasinの定義域[-1, 1]にクランプ
	float clampedY = forward.y;
	if(clampedY > 1.0f) clampedY = 1.0f;
	if(clampedY < -1.0f) clampedY = -1.0f;

	// 進行方向からピッチ(X軸回転)とヨー(Y軸回転)を逆算
	float pitch = -std::asin(clampedY);
	float yaw = std::atan2(forward.x,forward.z);

	// ロール(Z軸回転)は制御点間の値を補間して使用(バンク演出用)
	// 位置と同じ区間分けで、区間の始点・終点にあたる制御点(GetPositionOnRailで言うp1・p2)の値を補間し、制御点の位置でその値になるようにする
	float roll = 0.0f;
	if(controlPoints.size() >= 4){
		size_t segment = 0;
		float localT = 0.0f;
		ComputeCurveSegment(controlPoints.size(),t,segment,localT);
		roll = Lerp(controlPoints[segment + kRailStartPointIndex].rotation.z,controlPoints[segment + kRailStartPointIndex + 1].rotation.z,localT);
	}

	return {pitch, yaw, roll};
}

// 進行度t(0〜1)における進行方向(接線ベクトル)を取得(対象はアクティブなレール)
Vector3 RailEditor::GetForwardOnRail(float t) const{
	const float kEpsilon = 0.001f;
	float t0 = t - kEpsilon;
	float t1 = t + kEpsilon;

	if(t0 < 0.0f) t0 = 0.0f;
	if(t1 > 1.0f) t1 = 1.0f;

	Vector3 p0 = GetPositionOnRail(t0);
	Vector3 p1 = GetPositionOnRail(t1);
	Vector3 diff = p1 - p0;

	float length = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

	// 差分がほぼ0の場合は正面向きを返す
	if(length < 1e-5f){
		return {0.0f, 0.0f, 1.0f};
	}

	return {diff.x / length, diff.y / length, diff.z / length};
}

// 進行度t(0〜1)からレール上の速度(各制御点のSpeed値を補間したもの)を取得(対象はアクティブなレール)
float RailEditor::GetSpeedOnRail(float t) const{
	return ComputeSpeedOnRail(rails_[activeRailIndex_].controlPoints,t);
}

// 進行度tと制御点配列から、レール上の速度(各制御点のSpeed値を補間したもの)を計算する
float RailEditor::ComputeSpeedOnRail(const std::vector<ControlPoint>& controlPoints,float t) const{
	// 制御点が無い場合は既定速度を返す
	if(controlPoints.empty()){
		return kDefaultControlPoint.speed;
	}
	// 曲線を作れない(制御点4つ未満)ときは補間できないため、先頭の制御点のSpeed値をそのまま返す
	if(controlPoints.size() < 4){
		return controlPoints[0].speed;
	}

	// GetRotationOnRailのロール補間と同じく、位置と同じ区間分けで区間の始点・終点にあたる制御点のSpeed値を補間する
	size_t segment = 0;
	float localT = 0.0f;
	ComputeCurveSegment(controlPoints.size(),t,segment,localT);
	return Lerp(controlPoints[segment + kRailStartPointIndex].speed,controlPoints[segment + kRailStartPointIndex + 1].speed,localT);
}

// 進行度tから、Catmull-Romの区間番号と区間内の進行度を求める(ComputePositionOnRailと同じ区間分け。制御点4つ以上が前提)
void RailEditor::ComputeCurveSegment(size_t pointCount,float t,size_t& outSegment,float& outLocalT) const{
	size_t numSegments = pointCount - kCatmullRomExtraPointCount;
	float scaledT = t * static_cast<float>(numSegments);
	size_t segment = static_cast<size_t>(scaledT);

	if(segment >= numSegments){
		segment = numSegments - 1;
	}

	outSegment = segment;
	outLocalT = scaledT - static_cast<float>(segment);
}

// 進行度tの位置からレールに沿ってdistanceだけ進んだ先の進行度を取得(対象はアクティブなレール)
// 進行度tは区間ごとに等分されているため、制御点間が長い区間ほど進行度1あたりの移動距離が大きくなる
// そこで現在位置での「進行度1あたりの移動距離」で割り、進む距離を進行度の増加量に変換する
float RailEditor::AdvanceTByDistance(float t,float distance) const{
	// 終端付近では後ろ側の差分を使い、進行度が範囲外にならないようにする
	float t0 = t;
	float t1 = t + kArcLengthSampleDelta;
	if(t1 > 1.0f){
		t0 = 1.0f - kArcLengthSampleDelta;
		t1 = 1.0f;
	}

	// 進行度1あたりの移動距離(1フレームの移動量は小さいため、現在位置の値で直線近似する)
	float distancePerT = Distance(GetPositionOnRail(t1),GetPositionOnRail(t0)) / kArcLengthSampleDelta;
	if(distancePerT < kMinDistancePerT){
		return t;
	}

	return t + distance / distancePerT;
}

// アクティブなレールの制御点数を取得
int RailEditor::GetControlPointCount() const{
	return static_cast<int>(rails_[activeRailIndex_].controlPoints.size());
}

// 進行度tから、直近に通過した制御点のインデックスを取得(対象はアクティブなレール)
// GetPositionOnRailと同じ区間分割を用い、区間の始点(制御点)のインデックスを返す
int RailEditor::GetControlPointIndexFromT(float t) const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	// Catmull-Rom補間には最低4点必要
	if(controlPoints.size() < 4){
		return 0;
	}

	size_t numSegments = controlPoints.size() - 3;
	float scaledT = t * static_cast<float>(numSegments);
	size_t segment = static_cast<size_t>(scaledT);

	if(segment >= numSegments){
		segment = numSegments - 1;
	}

	// 区間の始点(GetPositionOnRailで言うp1)のインデックスが「直近に通過した制御点」
	return static_cast<int>(segment) + 1;
}

// アクティブなレールの、指定した制御点を通る位置の進行度を取得
// 制御点iは、ComputePositionOnRailの区間(i - 1)の始点にあたる
// 0番目は曲線より手前、最後の制御点は曲線より先にあり曲線が通らないため、曲線の始点・終点の進行度に丸める
float RailEditor::GetTOfControlPoint(int index) const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	// Catmull-Rom補間には最低4点必要
	if(controlPoints.size() < 4){
		return 0.0f;
	}

	const float numSegments = static_cast<float>(controlPoints.size() - kCatmullRomExtraPointCount);
	const float t = (static_cast<float>(index) - static_cast<float>(kRailStartPointIndex)) / numSegments;
	return std::clamp(t,0.0f,1.0f);
}

// アクティブなレールで、進行度tまでに通ったチェックポイントのうち、最も先にある制御点のインデックスを取得
// レールの途中に着地した場合も、着地した位置より手前のチェックポイントは通ったものとして扱う
int RailEditor::FindPassedCheckpoint(float t) const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	// 先にある制御点から順に見て、最初に見つかった通過済みのチェックポイントを返す
	for(int i = static_cast<int>(controlPoints.size()) - 1; i >= 0; --i){
		if(controlPoints[i].isCheckpoint && GetTOfControlPoint(i) <= t){
			return i;
		}
	}
	return -1;
}

// アクティブなレールの全長と走り切るまでの秒数、始点から選択中の制御点までの長さと秒数を求める
// 曲線の点列と同じ細かさでレールを分け、分けた区間ごとに「長さ÷その区間の速さ」を足していく
// 速さはPlayerと同じく、制御点のSpeed値に全体の速さ(railSpeed)を掛けたもの
RailEditor::RailStats RailEditor::ComputeActiveRailStats() const{
	RailStats stats;
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	// Catmull-Rom補間には最低4点必要
	if(controlPoints.size() < 4){
		return stats;
	}

	const size_t curveSegmentCount = controlPoints.size() - kCatmullRomExtraPointCount;
	const size_t sampleCount = curveSegmentCount * kCurveSamplesPerSegment;

	// 選択中の制御点までに含まれる、分けた区間の数(制御点が曲線を通る位置は区間の境目にあたるため、整数で数えられる)
	size_t samplesToSelected = 0;
	if(selectedPointIndex_ >= 0 && selectedPointIndex_ < static_cast<int>(controlPoints.size())){
		const int curveSegmentIndex = std::clamp(selectedPointIndex_ - static_cast<int>(kRailStartPointIndex),0,static_cast<int>(curveSegmentCount));
		samplesToSelected = static_cast<size_t>(curveSegmentIndex) * kCurveSamplesPerSegment;
	}

	// 区間の終点は次の区間の始点になるため、前の区間の終点を使い回して座標の計算を1回で済ませる
	Vector3 start = ComputePositionOnRail(controlPoints,0.0f);
	for(size_t i = 0; i < sampleCount; ++i){
		const float startT = static_cast<float>(i) / static_cast<float>(sampleCount);
		const float endT = static_cast<float>(i + 1) / static_cast<float>(sampleCount);
		const Vector3 end = ComputePositionOnRail(controlPoints,endT);

		const float length = Distance(start,end);
		const float speed = ComputeSpeedOnRail(controlPoints,Lerp(startT,endT,kStatsSpeedSampleRate)) * baseRailSpeed_;
		const float time = (speed >= kMinStatsSpeed)?length / speed:0.0f;

		stats.length += length;
		stats.time += time;
		if(i < samplesToSelected){
			stats.lengthToSelected += length;
			stats.timeToSelected += time;
		}

		start = end;
	}

	return stats;
}

// アクティブなレールの全制御点を囲む範囲の中心座標を取得
Vector3 RailEditor::GetControlPointsCenter() const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	if(controlPoints.empty()){
		return {0.0f, 0.0f, 0.0f};
	}

	Vector3 minPos = controlPoints[0].position;
	Vector3 maxPos = controlPoints[0].position;

	for(const auto& point : controlPoints){
		minPos.x = (point.position.x < minPos.x)?point.position.x:minPos.x;
		minPos.y = (point.position.y < minPos.y)?point.position.y:minPos.y;
		minPos.z = (point.position.z < minPos.z)?point.position.z:minPos.z;

		maxPos.x = (point.position.x > maxPos.x)?point.position.x:maxPos.x;
		maxPos.y = (point.position.y > maxPos.y)?point.position.y:maxPos.y;
		maxPos.z = (point.position.z > maxPos.z)?point.position.z:maxPos.z;
	}

	return {
		(minPos.x + maxPos.x) * 0.5f,
		(minPos.y + maxPos.y) * 0.5f,
		(minPos.z + maxPos.z) * 0.5f
	};
}

// アクティブなレールの全制御点を囲む範囲のおおよその半径を取得
float RailEditor::GetControlPointsRadius() const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	if(controlPoints.empty()){
		return 0.0f;
	}

	Vector3 center = GetControlPointsCenter();
	float maxDistSq = 0.0f;

	for(const auto& point : controlPoints){
		float dx = point.position.x - center.x;
		float dz = point.position.z - center.z;
		float distSq = dx * dx + dz * dz; // XZ平面での距離の2乗

		if(distSq > maxDistSq){
			maxDistSq = distSq;
		}
	}

	return std::sqrt(maxDistSq);
}
