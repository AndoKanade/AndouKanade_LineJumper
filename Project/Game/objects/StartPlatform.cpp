#include "StartPlatform.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "Easing.h"
#include "PaintManager.h"
#include "objects/Player.h"
#include <algorithm>
#include <cmath>
#include <iterator>

namespace{
	// 台に使うモデル(-1〜1の立方体。スケールに台の大きさの半分を掛けて使う)
	// ゲートの柱・梁・ランプ・遮断棒も同じ立方体で作る
	const std::string kPlatformModelPath = "StartPlatform/startPlatform.obj";
	// 台の本体に使う、インクで塗れるよう面ごとに UV を展開した立方体(-1〜1。tools/generate_paint_box_model.py で生成する)
	const std::string kPaintablePlatformModelPath = "StartPlatform/startPlatformPaint.obj";
	// ゲートの柱・梁に使う、インクで塗れるよう面ごとに UV を展開した立方体(同じスクリプトで生成する)
	const std::string kGatePillarModelPath = "StartPlatform/gatePillarPaint.obj";
	const std::string kGateBeamModelPath = "StartPlatform/gateBeamPaint.obj";
	// 台・柱・梁の塗りテクスチャの名前(シーンを作り直しても同じ塗りテクスチャを使い回す。柱・梁は後ろにゲートの番号を付ける)
	const std::string kInkCanvasName = "StartPlatform";
	const std::string kCorridorInkCanvasName = "StartPlatformCorridor";
	const std::string kGatePillarCanvasNames[] = {"StartGatePillarLeft", "StartGatePillarRight"};
	const std::string kGateBeamCanvasName = "StartGateBeam";
	// 台の上面に塗るインクの向き(上向きの面)
	constexpr Vector3 kTopNormal = {0.0f, 1.0f, 0.0f};
	// 目印・出発の輪に使う、輪の画像を貼った板(-1〜1で上向き。tools/generate_start_stage_assets.py で生成する)
	const std::string kRingModelPath = "StartPlatform/ring.obj";

	// 台の色(レールのピンクや雲の白と見分けやすい灰色)
	constexpr Vector4 kPlatformColor = {0.55f, 0.6f, 0.68f, 1.0f};

	// 左右の柱を並べる向き(左が -1、右が +1)
	constexpr float kPillarSides[] = {-1.0f, 1.0f};
	constexpr int kPillarCount = static_cast<int>(std::size(kPillarSides));
	static_assert(std::size(kGatePillarCanvasNames) == kPillarCount,"柱の塗りテクスチャの名前と柱の数が合っていない");
	// 大きさから、中心から面までの距離や立方体(-1〜1)に掛けるスケールを求める倍率(半分)
	constexpr float kHalf = 0.5f;
	// レイとの交差を調べる軸の数(右・上・前)
	constexpr int kAxisCount = 3;
	// レイが面と平行とみなす、向きの成分の大きさ(0 で割らないようにする)
	constexpr float kParallelEpsilon = 1.0e-6f;
	// 輪の板の半径(板は -1〜1 なので1)
	constexpr float kRingPlaneRadius = 1.0f;
	// 半透明の板は完全に透明なピクセルだけ捨てる
	constexpr float kTranslucentAlphaCutoff = 0.0f;
	// sin波(-1〜1)に1を足してから掛け、0〜1に変換する倍率
	constexpr float kWaveToRateScale = 0.5f;

	// 光源の影響を受けずに、設定した色のまま光って見えるようにする
	void SetUnlit(Obj3D& obj){
		if(Model::Material* material = obj.GetMaterial()){
			material->enableLighting = 0;
		}
	}

	// 2つの色を補間する
	Vector4 LerpColor(const Vector4& from,const Vector4& to,float t){
		return {Lerp(from.x,to.x,t), Lerp(from.y,to.y,t), Lerp(from.z,to.z,t), Lerp(from.w,to.w,t)};
	}

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

	// 3Dオブジェクトの色を設定する
	void SetColor(Obj3D& obj,const Vector4& color){
		if(Model::Material* material = obj.GetMaterial()){
			material->color = color;
		}
	}

	// 描画に使うカメラを今のカメラに合わせてから描く(カメラとの合成はDraw()の中で毎回行われる)
	void DrawWithActiveCamera(Obj3D& obj){
		if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
			obj.SetCamera(activeCamera);
		}
		obj.Draw();
	}
}

StartPlatform::StartPlatform() = default;
StartPlatform::~StartPlatform() = default;

// 初期化処理
void StartPlatform::Initialize(Obj3dCommon* objCommon){
	ModelManager::GetInstance()->LoadModel(kPlatformModelPath);
	ModelManager::GetInstance()->LoadModel(kPaintablePlatformModelPath);
	ModelManager::GetInstance()->LoadModel(kGatePillarModelPath);
	ModelManager::GetInstance()->LoadModel(kGateBeamModelPath);
	ModelManager::GetInstance()->LoadModel(kRingModelPath);

	// 色は変わらないものが多いため、生成時に一度設定しておく
	auto createObj = [objCommon](const std::string& modelPath,const Vector4& color){
		auto obj = std::make_unique<Obj3D>();
		obj->Initialize(objCommon);
		obj->SetModel(modelPath);
		SetColor(*obj,color);
		return obj;
	};
	// 目印・出発の輪は半透明の画像なので、透明な部分も捨てずに描いて縁をなめらかにする
	auto createRing = [&createObj](){
		auto ring = createObj(kRingModelPath,kInkColor);
		SetUnlit(*ring);
		if(Model::Material* material = ring->GetMaterial()){
			material->alphaCutoff = kTranslucentAlphaCutoff;
		}
		return ring;
	};

	// 台の本体(正方形の部分と通路)はインクで塗れるようにする(前のシーンで塗ったインクが残っているため消しておく)
	PaintManager* paintManager = PaintManager::GetInstance();
	obj_ = createObj(kPaintablePlatformModelPath,kPlatformColor);
	inkCanvas_ = paintManager->GetCanvas(kInkCanvasName,kInkCanvasSize);
	inkCanvas_->Clear();
	obj_->SetPaintCanvas(inkCanvas_);
	corridorObj_ = createObj(kPaintablePlatformModelPath,kPlatformColor);
	corridorInkCanvas_ = paintManager->GetCanvas(kCorridorInkCanvasName,kCorridorInkCanvasSize);
	corridorInkCanvas_->Clear();
	corridorObj_->SetPaintCanvas(corridorInkCanvas_);

	for(int gateIndex = 0; gateIndex < kGateCount; ++gateIndex){
		Gate& gate = gates_[gateIndex];
		const std::string canvasSuffix = std::to_string(gateIndex);

		// 左右の柱と上の梁(ゲートを開けるときにインクで塗り上げるため、塗れるようにする)
		for(int i = 0; i < kPillarCount; ++i){
			gate.frames.push_back(createObj(kGatePillarModelPath,kFrameColor));
			gate.frameCanvases.push_back(paintManager->GetCanvas(kGatePillarCanvasNames[i] + canvasSuffix,kGatePillarCanvasSize));
		}
		gate.frames.push_back(createObj(kGateBeamModelPath,kFrameColor));
		gate.frameCanvases.push_back(paintManager->GetCanvas(kGateBeamCanvasName + canvasSuffix,kGateBeamCanvasSize));
		for(size_t i = 0; i < gate.frames.size(); ++i){
			gate.frameCanvases[i]->Clear();
			gate.frames[i]->SetPaintCanvas(gate.frameCanvases[i]);
		}

		// ランプは光って見えるよう、光源の影響を受けないようにする
		for(int i = 0; i < kPillarCount * kLampCountPerPillar; ++i){
			auto lamp = createObj(kPlatformModelPath,kLampOffColor);
			SetUnlit(*lamp);
			gate.lamps.push_back(std::move(lamp));
		}

		gate.bar = createObj(kPlatformModelPath,kBarColor);
		gate.marker = createRing();
		gate.departurePad = createRing();
	}
	// 通路の壁は、どのステージの壁か分かるようステージごとの色にする
	for(int i = 0; i < kBarrierCount; ++i){
		barriers_[i] = createObj(kPlatformModelPath,kBarrierColors[i]);
	}
	launchRing_ = createRing();

	// 台はワールドに固定するため、ここで1回だけ置く
	PlaceParts();
	ResetGate();
}

// 台の上面の中央・正面の向きから、台の本体とゲートの部品を置く
void StartPlatform::PlaceParts(){
	// 台は水平に置くため、正面の向きは上下の成分を除いた向きにする
	forwardXZ_ = Normalize(Vector3{kFrontDirection.x, 0.0f, kFrontDirection.z});
	rightXZ_ = {forwardXZ_.z, 0.0f, -forwardXZ_.x};
	yaw_ = std::atan2(forwardXZ_.x,forwardXZ_.z);
	topCenter_ = kTopCenter;

	// 台の本体は動かないため、ワールド行列はここで求めておく(カメラとの合成はDraw()の中で毎回行われる)
	// モデルの中心は上面から厚みの半分だけ下になる。通路は正方形の部分の後ろの辺に付ける
	obj_->SetTranslate(ToWorld({0.0f, -kHalfHeight, 0.0f}));
	obj_->SetRotate({0.0f, yaw_, 0.0f});
	obj_->SetScale({kSquareHalfSize, kHalfHeight, kSquareHalfSize});
	obj_->Update();
	corridorObj_->SetTranslate(ToWorld({0.0f, -kHalfHeight, -(kSquareHalfSize + kCorridorLength * kHalf)}));
	corridorObj_->SetRotate({0.0f, yaw_, 0.0f});
	corridorObj_->SetScale({kCorridorHalfWidth, kHalfHeight, kCorridorLength * kHalf});
	corridorObj_->Update();

	// ゲートは、台の向きで決めた向きの辺(後ろは通路の先の辺)から kGateEdgeInset だけ手前に立てる
	const float pillarOffsetX = kGateHalfWidth - kPillarSize.x * kHalf;
	for(int gateIndex = 0; gateIndex < kGateCount; ++gateIndex){
		Gate& gate = gates_[gateIndex];
		const Vector3& localDirection = kGateLocalDirections[gateIndex];
		gate.forward = rightXZ_ * localDirection.x + forwardXZ_ * localDirection.z;
		gate.right = {gate.forward.z, 0.0f, -gate.forward.x};
		gate.yaw = std::atan2(gate.forward.x,gate.forward.z);
		gate.edgeDistance = (gateIndex == StageInfo::kFinalStageIndex)?kSquareHalfSize + kCorridorLength:kSquareHalfSize;
		gate.origin = topCenter_ + gate.forward * (gate.edgeDistance - kGateEdgeInset);

		// ゲートの柱と梁も動かないため、ここで行列を求めておく
		// 柱はゲートの幅の両端に立て、梁は柱の外側の端から端まで渡す
		for(int i = 0; i < kPillarCount; ++i){
			UpdatePart(*gate.frames[i],gate,{pillarOffsetX * kPillarSides[i], kPillarSize.y * kHalf, 0.0f},kPillarSize);
		}
		UpdatePart(*gate.frames.back(),gate,{0.0f, kPillarSize.y + kBeamHeight * kHalf, 0.0f},{kGateHalfWidth + kGateHalfWidth, kBeamHeight, kBeamDepth});
	}

	// 通路の壁も動かないため、ここで行列を求めておく(通路の幅いっぱいをふさぐ)
	for(int i = 0; i < kBarrierCount; ++i){
		barriers_[i]->SetTranslate(GetBarrierCenter(i));
		barriers_[i]->SetRotate({0.0f, yaw_, 0.0f});
		barriers_[i]->SetScale({kCorridorHalfWidth, kBarrierHeight * kHalf, kBarrierHalfThickness});
		barriers_[i]->Update();
	}
}

// 指定したゲートの先にあるステージの、レールの始点を置く位置
Vector3 StartPlatform::GetRailStartPosition(int gateIndex) const{
	const Gate& gate = gates_[std::clamp(gateIndex,0,kGateCount - 1)];
	return topCenter_ + gate.forward * (gate.edgeDistance - kRailOverlapLength);
}

// 指定したゲートから出ていく向き
Vector3 StartPlatform::GetGateDirection(int gateIndex) const{
	return gates_[std::clamp(gateIndex,0,kGateCount - 1)].forward;
}

// ゲートを閉じた状態に戻す
void StartPlatform::ResetGate(){
	selectedGateIndex_ = StageInfo::IsValidStageIndex(StageInfo::currentStageIndex)?StageInfo::currentStageIndex:0;
	gateOpenTimer_ = 0.0f;
	isGateOpening_ = false;
	isBarBurstTriggered_ = false;
	markerTimer_ = 0.0f;
	launchRingTimer_ = 0.0f;
	isLaunchRingActive_ = false;
}

// 指定したゲートを選んで開け始める
void StartPlatform::BeginOpenGate(int gateIndex){
	selectedGateIndex_ = std::clamp(gateIndex,0,kGateCount - 1);
	gateOpenTimer_ = 0.0f;
	isGateOpening_ = true;
	// 出発の操作をした目印の上にインクを塗り、そこから柱へインクが走り出すようにする
	PaintInk(GateToWorld(gates_[selectedGateIndex_],{0.0f, 0.0f, -kMarkerDistanceFromGate}),kGateStampRadius);
}

// ゲートを、開ける演出を出さずにすぐ開ききった状態にする
// 開き終わった時間にしておくため、インクの流し込み・遮断棒が跳ね上がる瞬間の演出は起きない
// ゲートが閉じたままだと、レールから落ちたときにゲートより手前へ押し戻されてしまうため、レールの上から始めるときは開けておく
void StartPlatform::OpenGateImmediately(){
	isGateOpening_ = true;
	gateOpenTimer_ = kGateOpenedTime;
	isBarBurstTriggered_ = false;
}

// ゲートが開ききったか
bool StartPlatform::IsGateOpened() const{
	return isGateOpening_ && gateOpenTimer_ >= kGateOpenedTime;
}

// カウントダウンで今出している数
// 最初の合図から数えて何回目の合図を過ぎたかで、3 から1つずつ減らす
int StartPlatform::GetCountdownNumber() const{
	if(!isGateOpening_ || gateOpenTimer_ < kFirstCountTime || gateOpenTimer_ >= kBarStartTime){
		return 0;
	}
	const int passedCount = static_cast<int>((gateOpenTimer_ - kFirstCountTime) / kCountdownInterval) + 1;
	return kCountdownCount + 1 - passedCount;
}

// 遮断棒が跳ね上がるときにインクを飛び散らせる位置
Vector3 StartPlatform::GetGateBurstPosition() const{
	return GateToWorld(gates_[selectedGateIndex_],{0.0f, kBarClosedHeight, 0.0f});
}

// 選んでいるゲートの中央の根元を原点にした座標をワールド座標に変換する
Vector3 StartPlatform::GateLocalToWorld(const Vector3& local) const{
	return GateToWorld(gates_[selectedGateIndex_],local);
}

// 選んでいるゲートの出発台の中央の座標
Vector3 StartPlatform::GetDeparturePadPosition() const{
	return GateToWorld(gates_[selectedGateIndex_],{0.0f, 0.0f, kDeparturePadDistance});
}

// 指定したステージの壁を、クリアしていても壊さずに立たせておく
void StartPlatform::HoldBarrier(int stageIndex){
	heldBarrierIndex_ = (stageIndex >= 0 && stageIndex < kBarrierCount)?stageIndex:-1;
}

// 立たせておいた壁を壊れた状態にする
void StartPlatform::ReleaseBarrierHold(){
	heldBarrierIndex_ = -1;
}

// 指定したステージの壁が立っているか
bool StartPlatform::IsBarrierStanding(int stageIndex) const{
	if(stageIndex < 0 || stageIndex >= kBarrierCount){
		return false;
	}
	return !StageInfo::clearedStages[stageIndex] || stageIndex == heldBarrierIndex_;
}

// 指定したステージの壁の中心の座標(台の上面から壁の高さの半分だけ上)
Vector3 StartPlatform::GetBarrierCenter(int stageIndex) const{
	const int index = std::clamp(stageIndex,0,kBarrierCount - 1);
	return ToWorld({0.0f, kBarrierHeight * kHalf, -kBarrierDistances[index]});
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

// 出発の合図として、足元から輪を広げる
void StartPlatform::EmitLaunchRing(const Vector3& position){
	launchRingPosition_ = position;
	launchRingTimer_ = 0.0f;
	isLaunchRingActive_ = true;
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

// ランプ・遮断棒・目印・輪の動きを進め、行列を更新する
void StartPlatform::Update(float deltaTime){
	// このフレームで遮断棒が跳ね上がり始めたかは、毎フレーム調べ直す
	isBarBurstTriggered_ = false;
	if(isGateOpening_){
		const float previousTimer = gateOpenTimer_;
		gateOpenTimer_ = (std::min)(gateOpenTimer_ + deltaTime,kGateOpenedTime);
		PaintGateInk(previousTimer);
		isBarBurstTriggered_ = previousTimer < kBarStartTime && gateOpenTimer_ >= kBarStartTime;
	}
	markerTimer_ += deltaTime;

	// 目印はゆっくり点滅させて、ここに立てばよいことを伝える(出発台の目印も同じように点滅させる)
	const float blinkRate = (std::sin(markerTimer_ * kMarkerBlinkSpeed) + 1.0f) * kWaveToRateScale;
	const Vector4 blinkColor = {kInkColor.x, kInkColor.y, kInkColor.z, kMarkerMinAlpha + (1.0f - kMarkerMinAlpha) * blinkRate};
	const float pillarOffsetX = kGateHalfWidth - kPillarSize.x * kHalf;
	const float lampDistance = -(kPillarSize.z + kLampSize.z) * kHalf;
	const float barHalfLength = pillarOffsetX - kPillarSize.x * kHalf;
	const float departurePadScale = kDeparturePadRadius / kRingPlaneRadius;
	const float markerScale = kMarkerRadius / kRingPlaneRadius;

	for(int gateIndex = 0; gateIndex < kGateCount; ++gateIndex){
		Gate& gate = gates_[gateIndex];
		const bool isOpeningThisGate = isGateOpening_ && gateIndex == selectedGateIndex_;

		// 開けていないゲートのランプは、クリアしたステージなら点いたときの色、まだ開けられないなら赤、それ以外は消えている色にする
		// (ゲートの番号とステージの番号は同じ)
		Vector4 idleLampColor = kLampOffColor;
		if(StageInfo::clearedStages[gateIndex]){
			idleLampColor = kLampOnColor;
		} else if(!IsGateSelectable(gateIndex)){
			idleLampColor = kLampLockedColor;
		}

		// ランプは柱の手前側(プレイヤーが近づいてくる側)の面に貼り付け、開けているゲートではカウントダウンの合図のたびに下から1段ずつ点ける
		// (合図の瞬間に、塗り上がってきたインクがちょうどランプを覆う)
		// 点いた瞬間は明るく光らせ、少しずつ点いたときの色に落ち着かせる
		for(int side = 0; side < kPillarCount; ++side){
			for(int row = 0; row < kLampCountPerPillar; ++row){
				Obj3D& lamp = *gate.lamps[side * kLampCountPerPillar + row];
				const float lampHeight = kLampBottomHeight + kLampSpacing * static_cast<float>(row);
				const float litTime = GetCountTime(row);
				Vector4 lampColor = isOpeningThisGate?kLampOffColor:idleLampColor;
				if(isOpeningThisGate && gateOpenTimer_ >= litTime){
					// 点いてからの時間で、光った色から点いたときの色へ戻していく
					lampColor = LerpColor(kLampFlashColor,kLampOnColor,std::clamp((gateOpenTimer_ - litTime) / kLampFlashDuration,0.0f,1.0f));
				}
				SetColor(lamp,lampColor);
				UpdatePart(lamp,gate,{pillarOffsetX * kPillarSides[side], lampHeight, lampDistance},kLampSize);
			}
		}

		// 遮断棒は左右の柱の内側の面の間をふさぐ長さにし、開けているゲートでは梁まで塗り終わってから跳ね上がり、少し行き過ぎてから戻って止まるように上げる
		float barProgress = 0.0f;
		if(isOpeningThisGate){
			barProgress = std::clamp((gateOpenTimer_ - kBarStartTime) / kBarOpenDuration,0.0f,1.0f);
		}
		const float barHeight = Lerp(kBarClosedHeight,kBarOpenedHeight,Easing::EaseOutBack(barProgress));
		UpdatePart(*gate.bar,gate,{0.0f, barHeight, 0.0f},{barHalfLength + barHalfLength, kBarThickness, kBarThickness});

		SetColor(*gate.marker,blinkColor);
		SetColor(*gate.departurePad,blinkColor);
		gate.departurePad->SetTranslate(GateToWorld(gate,{0.0f, kDecalLift, kDeparturePadDistance}));
		gate.departurePad->SetRotate({0.0f, gate.yaw, 0.0f});
		gate.departurePad->SetScale({departurePadScale, departurePadScale, departurePadScale});
		gate.departurePad->Update();
		gate.marker->SetTranslate(GateToWorld(gate,{0.0f, kDecalLift, -kMarkerDistanceFromGate}));
		gate.marker->SetRotate({0.0f, gate.yaw, 0.0f});
		gate.marker->SetScale({markerScale, markerScale, markerScale});
		gate.marker->Update();
	}

	// 出発の輪は勢いよく広がりながら薄くなって消える
	if(isLaunchRingActive_){
		launchRingTimer_ += deltaTime;
		float ringProgress = launchRingTimer_ / kLaunchRingDuration;
		if(ringProgress >= 1.0f){
			isLaunchRingActive_ = false;
		} else{
			float ringScale = Lerp(kLaunchRingStartRadius,kLaunchRingEndRadius,Easing::EaseOutCubic(ringProgress)) / kRingPlaneRadius;
			SetColor(*launchRing_,{kInkColor.x, kInkColor.y, kInkColor.z, 1.0f - ringProgress});
			launchRing_->SetTranslate({launchRingPosition_.x, launchRingPosition_.y + kDecalLift, launchRingPosition_.z});
			launchRing_->SetRotate({0.0f, yaw_, 0.0f});
			launchRing_->SetScale({ringScale, ringScale, ringScale});
			launchRing_->Update();
		}
	}
}

// 台とゲートの描画処理
void StartPlatform::Draw(){
	if(!obj_){
		return;
	}

	DrawWithActiveCamera(*obj_);
	DrawWithActiveCamera(*corridorObj_);
	// 通路の壁は、立っているものだけ描く
	for(int i = 0; i < kBarrierCount; ++i){
		if(IsBarrierStanding(i)){
			DrawWithActiveCamera(*barriers_[i]);
		}
	}
	for(Gate& gate : gates_){
		for(auto& frame : gate.frames){
			DrawWithActiveCamera(*frame);
		}
		for(auto& lamp : gate.lamps){
			DrawWithActiveCamera(*lamp);
		}
		DrawWithActiveCamera(*gate.bar);
	}
}

// 足元の目印と出発の輪の描画処理
void StartPlatform::DrawEffects(){
	// 目印は、どれかのゲートを開け始めたら役目を終えるので消す。まだ開けられないゲートの目印は出さない
	if(!isGateOpening_){
		for(int gateIndex = 0; gateIndex < kGateCount; ++gateIndex){
			if(IsGateSelectable(gateIndex)){
				DrawWithActiveCamera(*gates_[gateIndex].marker);
			}
		}
	}
	if(launchRing_ && isLaunchRingActive_){
		DrawWithActiveCamera(*launchRing_);
	}
	// 出発台の目印は、開けたゲートの先にだけ出す
	if(isDeparturePadVisible_){
		DrawWithActiveCamera(*gates_[selectedGateIndex_].departurePad);
	}
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

// 指定したゲートを開けられるか
// 最後のステージは、ほかのステージをすべてクリアしていても、壁を壊す演出で壁が壊れるまでは開けられない
bool StartPlatform::IsGateSelectable(int gateIndex) const{
	if(!StageInfo::IsStageSelectable(gateIndex)){
		return false;
	}
	if(gateIndex != StageInfo::kFinalStageIndex){
		return true;
	}
	for(int i = 0; i < kBarrierCount; ++i){
		if(IsBarrierStanding(i)){
			return false;
		}
	}
	return true;
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

// 台の本体(正方形の部分と通路)とレイの交差を調べ、先に当たる方の距離を返す
float StartPlatform::Raycast(const Vector3& origin,const Vector3& direction,float maxDistance,float radius) const{
	// 台の本体の中心は上面から厚みの半分だけ下になる。通路は正方形の部分の後ろの辺に付いている
	const float squareDistance = RaycastBox({0.0f, -kHalfHeight, 0.0f},{kSquareHalfSize, kHalfHeight, kSquareHalfSize},origin,direction,maxDistance,radius);
	const float corridorDistance = RaycastBox({0.0f, -kHalfHeight, -(kSquareHalfSize + kCorridorLength * kHalf)},{kCorridorHalfWidth, kHalfHeight, kCorridorLength * kHalf},origin,direction,maxDistance,radius);
	return (std::min)(squareDistance,corridorDistance);
}

// 台の向きでの中心・大きさの直方体とレイの交差を調べる
// レイを台の向きの座標(x: 右、y: 上、z: 前)に直し、軸ごとの面の間に入っている区間が3軸すべてで重なる最初の距離を求める
float StartPlatform::RaycastBox(const Vector3& localCenter,const Vector3& halfSizeLocal,const Vector3& origin,const Vector3& direction,float maxDistance,float radius) const{
	const Vector3 diff = origin - ToWorld(localCenter);
	const float localOrigin[kAxisCount] = {Dot(diff,rightXZ_), diff.y, Dot(diff,forwardXZ_)};
	const float localDirection[kAxisCount] = {Dot(direction,rightXZ_), direction.y, Dot(direction,forwardXZ_)};
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

// 台の上面の中央を原点にした、台の向きでの座標をワールド座標に変換する
Vector3 StartPlatform::ToWorld(const Vector3& local) const{
	return topCenter_ + rightXZ_ * local.x + Vector3{0.0f, local.y, 0.0f} + forwardXZ_ * local.z;
}

// ゲートの中央の根元を原点にした、ゲートの向きでの座標をワールド座標に変換する
Vector3 StartPlatform::GateToWorld(const Gate& gate,const Vector3& local) const{
	return gate.origin + gate.right * local.x + Vector3{0.0f, local.y, 0.0f} + gate.forward * local.z;
}

// ゲートの部品の位置・回転・大きさを設定して行列を更新する
// 立方体のモデルは -1〜1 なので、各辺の長さの半分をスケールにする
void StartPlatform::UpdatePart(Obj3D& obj,const Gate& gate,const Vector3& local,const Vector3& size){
	obj.SetTranslate(GateToWorld(gate,local));
	obj.SetRotate({0.0f, gate.yaw, 0.0f});
	obj.SetScale(size * kHalf);
	obj.Update();
}
