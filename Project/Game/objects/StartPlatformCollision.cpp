#include "StartPlatform.h"
#include "StartPlatformInternal.h"
#include "objects/Player.h"
#include <algorithm>
#include <cmath>
#include <utility>

// StartPlatformのうち、台の上の範囲の判定・台の外やゲートの先へ出ないための押し戻し・カメラのめり込み防止に使うレイ判定をまとめたファイル

using namespace StartPlatformInternal;

namespace{
	// レイとの交差を調べる軸の数(右・上・前)
	constexpr int kAxisCount = 3;
	// レイが面と平行とみなす、向きの成分の大きさ(0 で割らないようにする)
	constexpr float kParallelEpsilon = 1.0e-6f;
}

// 水平方向(XZ平面)で見て、指定した座標が正方形の部分の上面の範囲に入っているか
// 台の中心からの差を台の前方向・右方向の成分に分け、それぞれが大きさの半分以内かで判定する
bool StartPlatform::IsInsideSquareTop(const Vector3& position) const{
	Vector3 diff = {position.x - topCenter_.x, 0.0f, position.z - topCenter_.z};
	return std::abs(Dot(diff,forwardXZ_)) <= kSquareHalfSize && std::abs(Dot(diff,rightXZ_)) <= kSquareHalfSize;
}

// 水平方向(XZ平面)で見て、指定した座標が通路の上面の範囲に入っているか
bool StartPlatform::IsInsideCorridorTop(const Vector3& position) const{
	Vector3 diff = {position.x - topCenter_.x, 0.0f, position.z - topCenter_.z};
	const float forwardAmount = Dot(diff,forwardXZ_);
	return forwardAmount <= -kSquareHalfSize && forwardAmount >= -(kSquareHalfSize + kCorridorLength) && std::abs(Dot(diff,rightXZ_)) <= kCorridorHalfWidth;
}

// 水平方向(XZ平面)で見て、指定した座標が台の上面の範囲(正方形の部分か通路)に入っているか
bool StartPlatform::IsInsideTop(const Vector3& position) const{
	return IsInsideSquareTop(position) || IsInsideCorridorTop(position);
}

// 台の上で、選んでいるゲートの出発台の手前の線を越えたか
// ゲートの根元からの差の、ゲートから出ていく向きの成分が、線の位置を越えたかで判定する
bool StartPlatform::IsPastDepartureLine(const Vector3& position) const{
	// 線を越えた時点では、まだ歩いてレールに乗れる範囲に入らない位置に線を引く
	static_assert(kGateEdgeInset - kRailOverlapLength - (kDeparturePadDistance - kDeparturePadRadius) > Player::kOnRailHorizontalThreshold,
		"出発の演出を始める前に、歩いてレールへ乗れてしまう位置に出発台がある");

	if(!IsInsideTop(position)){
		return false;
	}
	const Gate& gate = gates_[selectedGateIndex_];
	Vector3 diff = {position.x - gate.origin.x, 0.0f, position.z - gate.origin.z};
	return Dot(diff,gate.forward) >= kDeparturePadDistance - kDeparturePadRadius;
}

// ゲートがどれも開いていないときに、どのゲートの前の目印の上に立っているか
int StartPlatform::FindGateArea(const Vector3& position) const{
	if(isGateOpening_ || std::abs(position.y - topCenter_.y) > kGateAreaHeightRange){
		return -1;
	}
	for(int gateIndex = 0; gateIndex < kGateCount; ++gateIndex){
		// まだ開けられないゲート(ほかのステージをクリアしていない最後のステージ)の前では受け付けない
		if(!IsGateSelectable(gateIndex)){
			continue;
		}
		const Vector3 markerPosition = GateToWorld(gates_[gateIndex],{0.0f, 0.0f, -kMarkerDistanceFromGate});
		const Vector3 diffXZ = {position.x - markerPosition.x, 0.0f, position.z - markerPosition.z};
		if(Length(diffXZ) <= kGateAreaRadius){
			return gateIndex;
		}
	}
	return -1;
}

// プレイ会用の設定で閉じているゲートの、すぐ手前に立っているか
// ゲートの根元から見た、出ていく向きの距離と横の距離で判定する(ジャンプ中でも、ゲートの手前にいれば出す)
bool StartPlatform::IsInFrontOfPlaySessionClosedGate(const Vector3& position) const{
	for(int gateIndex = 0; gateIndex < kGateCount; ++gateIndex){
		if(!StageInfo::IsClosedForPlaySession(gateIndex)){
			continue;
		}
		const Gate& gate = gates_[gateIndex];
		const Vector3 diff = {position.x - gate.origin.x, 0.0f, position.z - gate.origin.z};
		const float forwardAmount = Dot(diff,gate.forward);
		if(forwardAmount <= 0.0f && forwardAmount >= -kLockedNoticeDepth && std::abs(Dot(diff,gate.right)) <= kGateHalfWidth){
			return true;
		}
	}
	return false;
}

// 閉じているゲートより先へ進めないように押し戻す
// ゲートごとに、ゲートの根元から見た出ていく向きの距離が、ゲートの手前の線を越えた分だけ手前へ戻す
Vector3 StartPlatform::ClampBehindClosedGate(const Vector3& position) const{
	// 押し戻す線の上にいても、まだレールの真上(歩いて乗れる範囲)には入らない位置にゲートを置く
	static_assert(kGateEdgeInset - kRailOverlapLength + kGateWallMargin > Player::kOnRailHorizontalThreshold,
		"ゲートが閉じている間にレールへ乗れてしまう位置にゲートがある");

	Vector3 result = position;
	for(int gateIndex = 0; gateIndex < kGateCount; ++gateIndex){
		// 開け始めたゲートの先へは進める
		if(isGateOpening_ && gateIndex == selectedGateIndex_){
			continue;
		}
		const Gate& gate = gates_[gateIndex];
		const Vector3 diff = {result.x - gate.origin.x, 0.0f, result.z - gate.origin.z};
		const float overshoot = Dot(diff,gate.forward) + kGateWallMargin;
		if(overshoot > 0.0f){
			result = result - gate.forward * overshoot;
		}
	}

	// 通路に立っている壁より奥へも進めない(壁の手前の面から、ゲートと同じ距離だけ手前へ戻す)
	// 通路の幅の外は台の外で、そこから回り込んでも台に乗れずに落ちるため、通路の幅の中にいるときだけ押し戻す
	const Vector3 backward = -forwardXZ_;
	for(int i = 0; i < kBarrierCount; ++i){
		if(!IsBarrierStanding(i)){
			continue;
		}
		const Vector3 diff = {result.x - topCenter_.x, 0.0f, result.z - topCenter_.z};
		if(std::abs(Dot(diff,rightXZ_)) > kCorridorHalfWidth){
			continue;
		}
		const float overshoot = Dot(diff,backward) - (kBarrierDistances[i] - kBarrierHalfThickness) + kGateWallMargin;
		if(overshoot > 0.0f){
			result = result - backward * overshoot;
		}
	}
	return result;
}

// 台の上を歩いている間に端から落ちないよう、台の上面の範囲に押し戻す
// 台の中心からの差を台の前方向・右方向の成分に分け、正方形の部分と通路のそれぞれについて、端から少し内側までの範囲に収めた位置を求める
// 2つのうち元の位置に近い方へ戻す(通路の範囲は正方形の部分の中まで伸ばしておき、つなぎ目で引っかからずに行き来できるようにする)
Vector3 StartPlatform::ClampInsideTop(const Vector3& position) const{
	const Vector3 diff = {position.x - topCenter_.x, 0.0f, position.z - topCenter_.z};
	const float forwardAmount = Dot(diff,forwardXZ_);
	const float rightAmount = Dot(diff,rightXZ_);

	// 正方形の部分の中に収めた位置
	const float squareLimit = kSquareHalfSize - kEdgeMargin;
	const float squareForward = std::clamp(forwardAmount,-squareLimit,squareLimit);
	const float squareRight = std::clamp(rightAmount,-squareLimit,squareLimit);
	// 通路の中に収めた位置(前の端は正方形の部分の中まで伸ばす)
	const float corridorRightLimit = kCorridorHalfWidth - kEdgeMargin;
	const float corridorForward = std::clamp(forwardAmount,-(kSquareHalfSize + kCorridorLength - kEdgeMargin),squareLimit);
	const float corridorRight = std::clamp(rightAmount,-corridorRightLimit,corridorRightLimit);

	// 元の位置からの距離を比べ、近い方を使う(距離の2乗で比べる)
	const float squareMove = (forwardAmount - squareForward) * (forwardAmount - squareForward) + (rightAmount - squareRight) * (rightAmount - squareRight);
	const float corridorMove = (forwardAmount - corridorForward) * (forwardAmount - corridorForward) + (rightAmount - corridorRight) * (rightAmount - corridorRight);
	const bool usesSquare = squareMove <= corridorMove;
	const Vector3 clamped = ToWorld({usesSquare?squareRight:corridorRight, 0.0f, usesSquare?squareForward:corridorForward});
	return {clamped.x, position.y, clamped.z};
}

// 台の本体(正方形の部分と通路)とレイの交差を調べ、先に当たる方の距離を返す
float StartPlatform::Raycast(const Vector3& origin,const Vector3& direction,float maxDistance,float radius) const{
	// 台の本体の中心は上面から厚みの半分だけ下になる。通路は正方形の部分の後ろの辺に付いている
	const float squareDistance = RaycastBox({0.0f, -kHalfHeight, 0.0f},{kSquareHalfSize, kHalfHeight, kSquareHalfSize},origin,direction,maxDistance,radius);
	const float corridorDistance = RaycastBox({0.0f, -kHalfHeight, -(kSquareHalfSize + kCorridorLength * kHalf)},{kCorridorHalfWidth, kHalfHeight, kCorridorLength * kHalf},origin,direction,maxDistance,radius);
	const float barrierDistance = RaycastBarriers(origin,direction,maxDistance,radius);
	const float gateDistance = RaycastGates(origin,direction,maxDistance,radius);
	return (std::min)((std::min)(squareDistance,corridorDistance),(std::min)(barrierDistance,gateDistance));
}

// ゲートの柱・梁・遮断棒とレイの交差を調べ、一番先に当たる距離を返す
// 部品は細く、プレイヤーが柱のそばに立つこともあるため、余白は radius より小さい kGateCameraMargin までにする
// (余白が大きいと始点が余白の範囲に入り、当たらない扱いになってしまう。近平面より大きくしておけば、部品の中は見えない)
float StartPlatform::RaycastGates(const Vector3& origin,const Vector3& direction,float maxDistance,float radius) const{
	const float margin = (std::min)(radius,kGateCameraMargin);
	const float pillarOffsetX = kGateHalfWidth - kPillarSize.x * kHalf;
	const float barHalfLength = pillarOffsetX - kPillarSize.x * kHalf;
	float result = maxDistance;
	for(const Gate& gate : gates_){
		// 左右の柱
		for(int i = 0; i < kPillarCount; ++i){
			const Vector3 center = GateToWorld(gate,{pillarOffsetX * kPillarSides[i], kPillarSize.y * kHalf, 0.0f});
			result = RaycastOrientedBox(center,gate.right,gate.forward,kPillarSize * kHalf,origin,direction,result,margin);
		}
		// 上の梁
		const Vector3 beamCenter = GateToWorld(gate,{0.0f, kPillarSize.y + kBeamHeight * kHalf, 0.0f});
		result = RaycastOrientedBox(beamCenter,gate.right,gate.forward,Vector3{kGateHalfWidth + kGateHalfWidth, kBeamHeight, kBeamDepth} * kHalf,origin,direction,result,margin);
		// 遮断棒(今の高さで調べる)
		const Vector3 barCenter = GateToWorld(gate,{0.0f, gate.barHeight, 0.0f});
		result = RaycastOrientedBox(barCenter,gate.right,gate.forward,Vector3{barHalfLength + barHalfLength, kBarThickness, kBarThickness} * kHalf,origin,direction,result,margin);
	}
	return result;
}

// 通路に立っている壁の、台の中央側の面とレイの交差を調べる
// プレイヤーは立っている壁より奥へは進めないため、レイの始点(カメラの回転の中心)はいつも壁の手前にある。そこで手前の面だけを調べる
// プレイヤーは壁の面から kGateWallMargin まで近づけ、radius より近いことがあるため、箱の判定(始点が箱の中なら当たらない扱い)は使わない
// 面から radius だけ手前の位置で止め、始点がそれより面に近いときは始点の位置(距離 0)で止める
float StartPlatform::RaycastBarriers(const Vector3& origin,const Vector3& direction,float maxDistance,float radius) const{
	const Vector3 backward = -forwardXZ_;
	const Vector3 diff = origin - topCenter_;
	// 始点と向きの、通路の奥へ向かう成分
	const float originDepth = Dot(diff,backward);
	const float directionDepth = Dot(direction,backward);
	// 通路の奥へ向かわないレイは、手前の面に当たらない
	if(directionDepth < kParallelEpsilon){
		return maxDistance;
	}

	float result = maxDistance;
	for(int i = 0; i < kBarrierCount; ++i){
		if(!IsBarrierStanding(i)){
			continue;
		}
		const float faceDepth = kBarrierDistances[i] - kBarrierHalfThickness;
		// 壁より奥にある始点からは調べない(壊れていない壁の奥へは行けないため、念のため)
		if(originDepth > faceDepth){
			continue;
		}

		// 面に届く距離で、当たった点が壁の幅・高さ(radius だけ広げた範囲)に入っているかを調べる
		const float faceDistance = (faceDepth - originDepth) / directionDepth;
		if(faceDistance >= result){
			continue;
		}
		const Vector3 hitDiff = diff + direction * faceDistance;
		const bool isWithinWidth = std::abs(Dot(hitDiff,rightXZ_)) <= kCorridorHalfWidth + radius;
		const bool isWithinHeight = hitDiff.y <= kBarrierHeight + radius;
		if(!isWithinWidth || !isWithinHeight){
			continue;
		}
		result = (std::max)((faceDepth - radius - originDepth) / directionDepth,0.0f);
	}
	return result;
}

// 台の向きでの中心・大きさの直方体とレイの交差を調べる
// レイを台の向きの座標(x: 右、y: 上、z: 前)に直し、軸ごとの面の間に入っている区間が3軸すべてで重なる最初の距離を求める
float StartPlatform::RaycastBox(const Vector3& localCenter,const Vector3& halfSizeLocal,const Vector3& origin,const Vector3& direction,float maxDistance,float radius) const{
	return RaycastOrientedBox(ToWorld(localCenter),rightXZ_,forwardXZ_,halfSizeLocal,origin,direction,maxDistance,radius);
}

// 中心・向きを決めた直方体とレイの交差を調べる
// レイを直方体の向きの座標(x: 右、y: 上、z: 前)に直し、軸ごとの面の間に入っている区間が3軸すべてで重なる最初の距離を求める
float StartPlatform::RaycastOrientedBox(const Vector3& worldCenter,const Vector3& right,const Vector3& forward,const Vector3& halfSizeLocal,const Vector3& origin,const Vector3& direction,float maxDistance,float radius) const{
	const Vector3 diff = origin - worldCenter;
	const float localOrigin[kAxisCount] = {Dot(diff,right), diff.y, Dot(diff,forward)};
	const float localDirection[kAxisCount] = {Dot(direction,right), direction.y, Dot(direction,forward)};
	const float halfSize[kAxisCount] = {halfSizeLocal.x + radius, halfSizeLocal.y + radius, halfSizeLocal.z + radius};

	// 3軸すべての面の間に入っている区間(入る距離と出る距離)
	float enterDistance = 0.0f;
	float exitDistance = maxDistance;
	for(int i = 0; i < kAxisCount; ++i){
		// レイがこの軸の面と平行なときは、始点が面の間に入っていなければ当たらない
		if(std::abs(localDirection[i]) < kParallelEpsilon){
			if(std::abs(localOrigin[i]) > halfSize[i]){
				return maxDistance;
			}
			continue;
		}
		float nearDistance = (-halfSize[i] - localOrigin[i]) / localDirection[i];
		float farDistance = (halfSize[i] - localOrigin[i]) / localDirection[i];
		if(nearDistance > farDistance){
			std::swap(nearDistance,farDistance);
		}
		enterDistance = (std::max)(enterDistance,nearDistance);
		exitDistance = (std::min)(exitDistance,farDistance);
		if(enterDistance > exitDistance){
			return maxDistance;
		}
	}

	// 始点が範囲の中にあるときは、どちらへ押し出すか決まらないため当たっていないものとする
	if(enterDistance <= 0.0f){
		return maxDistance;
	}
	return enterDistance;
}
