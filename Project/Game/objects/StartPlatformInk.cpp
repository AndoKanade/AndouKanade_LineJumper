#include "StartPlatform.h"
#include "StartPlatformInternal.h"
#include "Obj3D.h"
#include "Easing.h"
#include "PaintManager.h"
#include <algorithm>
#include <cmath>

// StartPlatformのうち、台の上面とゲートの柱・梁にインクを塗る処理をまとめたファイル

using namespace StartPlatformInternal;

namespace{
	// 台の上面に塗るインクの向き(上向きの面)
	constexpr Vector3 kTopNormal = {0.0f, 1.0f, 0.0f};

	// 間隔 spacing ごとに並べた点(spacing, 2×spacing, ...)のうち、from より先で to までにある点の距離を順に渡す
	// 前のフレームから今のフレームまでに進んだ区間の点だけを渡すため、同じ点に二度インクを塗らない
	template<class Function>
	void ForEachPointInRange(float from,float to,float spacing,Function function){
		const int first = static_cast<int>(std::floor(from / spacing)) + 1;
		const int last = static_cast<int>(std::floor(to / spacing));
		for(int i = first; i <= last; ++i){
			function(static_cast<float>(i) * spacing);
		}
	}
}

// 台の上面にインクを1滴塗る(通路の上なら通路の塗りテクスチャ、それ以外は正方形の部分の塗りテクスチャに塗る)
void StartPlatform::PaintInk(const Vector3& position,float radius){
	PaintCanvas* canvas = (IsInsideCorridorTop(position) && !IsInsideSquareTop(position))?corridorInkCanvas_:inkCanvas_;
	if(canvas){
		canvas->AddSplat(position,radius,kTopNormal,kInkColor);
	}
}

// 落ちてくるインクの粒が台の上面に着いたら、着いた位置にインクを塗る
// 移動前は上面より上、移動後は上面以下にいるときだけ、移動の線分と上面の高さの平面が交わる点を求める
bool StartPlatform::TryLandInk(const Vector3& from,const Vector3& to,float radius,Vector3* landedPosition){
	const float topHeight = topCenter_.y;
	if(from.y < topHeight || to.y >= topHeight){
		return false;
	}
	// 移動前から上面までの高さの差が、移動全体の高さの差のどれだけの割合か
	float rate = (from.y - topHeight) / (from.y - to.y);
	Vector3 landPosition = Lerp(from,to,rate);
	if(!IsInsideTop(landPosition)){
		return false;
	}
	landPosition.y = topHeight;
	PaintInk(landPosition,radius);
	if(landedPosition){
		*landedPosition = landPosition;
	}
	return true;
}

// 台・柱・梁に塗ったインクをすべて消す
void StartPlatform::ClearInk(){
	for(PaintCanvas* canvas : {inkCanvas_, corridorInkCanvas_}){
		if(canvas){
			canvas->Clear();
		}
	}
	for(Gate& gate : gates_){
		for(PaintCanvas* canvas : gate.frameCanvases){
			canvas->Clear();
		}
	}
	// クリアしたステージのゲートの前の床は、消した後すぐに塗り直す
	PaintClearedGateFloors();
}

// クリアしたステージのゲートの前の床をインクで塗る
// ゲートの番号とステージの番号は同じ
void StartPlatform::PaintClearedGateFloors(){
	for(int gateIndex = 0; gateIndex < kGateCount; ++gateIndex){
		if(!StageInfo::clearedStages[gateIndex]){
			continue;
		}
		for(int i = 0; i < kClearedFloorSplatCount; ++i){
			PaintInk(GateToWorld(gates_[gateIndex],kClearedFloorSplatPositions[i]),kClearedFloorSplatRadii[i]);
		}
	}
}

// 予約したインクを台・柱・梁の塗りテクスチャに描き込む
void StartPlatform::FlushInk(){
	if(inkCanvas_ && obj_){
		inkCanvas_->Flush(*obj_);
	}
	if(corridorInkCanvas_ && corridorObj_){
		corridorInkCanvas_->Flush(*corridorObj_);
	}
	for(Gate& gate : gates_){
		for(size_t i = 0; i < gate.frames.size(); ++i){
			gate.frameCanvases[i]->Flush(*gate.frames[i]);
		}
	}
}

// ゲートを開けている間に、インクが進んだ分だけ床・柱・梁にインクを塗る
// 各段階の進み具合を前のフレームと今のフレームで求め、その間に進んだ区間にだけ一定の間隔でインクの粒を置く
// (フレームの長さが変わっても、粒の間隔は変わらない)
void StartPlatform::PaintGateInk(float previousTimer){
	// 経過時間を、指定した段階の進み具合(0〜1)に直す
	auto stageProgress = [](float timer,float startTime,float duration){
		return std::clamp((timer - startTime) / duration,0.0f,1.0f);
	};
	const float beamStartTime = kBarStartTime - kBeamFillDuration;

	// 開けているゲートの柱・梁の、手前側(プレイヤーが近づいてくる側)の面を塗る
	const Gate& gate = gates_[selectedGateIndex_];
	const Vector3 frontNormal = -gate.forward;
	const float pillarOffsetX = kGateHalfWidth - kPillarSize.x * kHalf;
	const float pillarFrontZ = -kPillarSize.z * kHalf;
	const float beamCenterHeight = kPillarSize.y + kBeamHeight * kHalf;
	const float beamFrontZ = -kBeamDepth * kHalf;
	const Vector3 markerLocal = {0.0f, 0.0f, -kMarkerDistanceFromGate};
	PaintCanvas* beamCanvas = gate.frameCanvases.back();

	for(int i = 0; i < kPillarCount; ++i){
		const float side = kPillarSides[i];

		// 1. 目印から柱の根元へ、床の上を走る
		// 進む向きと垂直な向きに少しずつ揺らし、液体が流れたような筋にする
		const Vector3 runVector = Vector3{pillarOffsetX * side, 0.0f, pillarFrontZ} - markerLocal;
		const float runLength = Length(runVector);
		const Vector3 runDirection = runVector / runLength;
		const Vector3 runSide = {runDirection.z, 0.0f, -runDirection.x};
		ForEachPointInRange(stageProgress(previousTimer,0.0f,kInkRunDuration) * runLength,stageProgress(gateOpenTimer_,0.0f,kInkRunDuration) * runLength,kFloorInkSpacing,
			[&](float distance){
				const float wobble = std::sin(distance * kFloorInkWobbleFrequency) * kFloorInkWobbleAmplitude;
				PaintInk(GateToWorld(gate,markerLocal + runDirection * distance + runSide * wobble),kFloorInkRadius);
			});

		// 2. 柱の手前の面を、下から上へ塗り上がる
		PaintCanvas* pillarCanvas = gate.frameCanvases[i];
		ForEachPointInRange(GetInkClimbHeight(previousTimer),GetInkClimbHeight(gateOpenTimer_),kClimbInkSpacing,
			[&](float height){
				pillarCanvas->AddSplat(GateToWorld(gate,{pillarOffsetX * side, height, pillarFrontZ}),kClimbInkRadius,frontNormal,kInkColor);
			});

		// 3. 柱の上から梁の中央へ向かって、梁の手前の面を塗る(左右から塗り進めて中央で合わせる)
		ForEachPointInRange(stageProgress(previousTimer,beamStartTime,kBeamFillDuration) * pillarOffsetX,stageProgress(gateOpenTimer_,beamStartTime,kBeamFillDuration) * pillarOffsetX,kBeamInkSpacing,
			[&](float distance){
				beamCanvas->AddSplat(GateToWorld(gate,{(pillarOffsetX - distance) * side, beamCenterHeight, beamFrontZ}),kBeamInkRadius,frontNormal,kInkColor);
			});
	}
}

// 柱を塗り上がったインクの高さ
// 3・2・1 の合図のたびに、合図の瞬間にちょうど届くよう少し前から加速しながら、その段のランプを覆う高さまで一気に塗り上がる
// 最後は 0 の合図で梁を塗り終えられるよう、梁を塗り始める時刻に柱の上まで塗り上がる
float StartPlatform::GetInkClimbHeight(float timer){
	float height = 0.0f;
	for(int i = 0; i <= kCountdownCount; ++i){
		const bool isTop = i == kCountdownCount;
		const float targetHeight = isTop?kPillarSize.y:kLampBottomHeight + kLampSpacing * static_cast<float>(i) + kLampSize.y * kHalf;
		const float arriveTime = isTop?kBarStartTime - kBeamFillDuration:GetCountTime(i);
		const float surgeRate = std::clamp((timer - (arriveTime - kInkSurgeDuration)) / kInkSurgeDuration,0.0f,1.0f);
		height = Lerp(height,targetHeight,Easing::EaseInCubic(surgeRate));
		// まだ塗り上がりきっていない段より上へは進めない
		if(surgeRate < 1.0f){
			break;
		}
	}
	return height;
}
