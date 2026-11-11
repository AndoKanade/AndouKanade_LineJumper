#include "StartPlatform.h"
#include "StartPlatformInternal.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "Easing.h"
#include "PaintManager.h"
#include <algorithm>
#include <cmath>
#include <iterator>

// StartPlatformのうち、台とゲートの本体(部品の生成と配置・ゲートの開閉・通路の壁・毎フレームの動きと描画)をまとめたファイル
// インクを塗る処理はStartPlatformInk.cpp、範囲の判定とカメラ用のレイ判定はStartPlatformCollision.cppにある

using namespace StartPlatformInternal;

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
	static_assert(std::size(kGatePillarCanvasNames) == kPillarCount,"柱の塗りテクスチャの名前と柱の数が合っていない");
	// 目印・出発の輪に使う、輪の画像を貼った板(-1〜1で上向き。tools/generate_start_stage_assets.py で生成する)
	const std::string kRingModelPath = "StartPlatform/ring.obj";

	// 台の色(レールのピンクや雲の白と見分けやすい灰色)
	constexpr Vector4 kPlatformColor = {0.55f, 0.6f, 0.68f, 1.0f};

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

// 出発の合図として、足元から輪を広げる
void StartPlatform::EmitLaunchRing(const Vector3& position){
	launchRingPosition_ = position;
	launchRingTimer_ = 0.0f;
	isLaunchRingActive_ = true;
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
		// カメラのめり込み防止で、今の高さの遮断棒を調べるのに使う
		gate.barHeight = barHeight;

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
