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
	// 台・柱・梁の塗りテクスチャの名前(シーンを作り直しても同じ塗りテクスチャを使い回す)
	const std::string kInkCanvasName = "StartPlatform";
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

	// 台の本体はインクで塗れるようにする(前のシーンで塗ったインクが残っているため消しておく)
	obj_ = createObj(kPaintablePlatformModelPath,kPlatformColor);
	inkCanvas_ = PaintManager::GetInstance()->GetCanvas(kInkCanvasName,kInkCanvasSize);
	inkCanvas_->Clear();
	obj_->SetPaintCanvas(inkCanvas_);

	// 左右の柱と上の梁(ゲートを開けるときにインクで塗り上げるため、塗れるようにする)
	PaintManager* paintManager = PaintManager::GetInstance();
	for(int i = 0; i < kPillarCount; ++i){
		gateFrames_.push_back(createObj(kGatePillarModelPath,kFrameColor));
		gateFrameCanvases_.push_back(paintManager->GetCanvas(kGatePillarCanvasNames[i],kGatePillarCanvasSize));
	}
	gateFrames_.push_back(createObj(kGateBeamModelPath,kFrameColor));
	gateFrameCanvases_.push_back(paintManager->GetCanvas(kGateBeamCanvasName,kGateBeamCanvasSize));
	for(size_t i = 0; i < gateFrames_.size(); ++i){
		gateFrameCanvases_[i]->Clear();
		gateFrames_[i]->SetPaintCanvas(gateFrameCanvases_[i]);
	}

	// ランプは光って見えるよう、光源の影響を受けないようにする
	for(int i = 0; i < kPillarCount * kLampCountPerPillar; ++i){
		auto lamp = createObj(kPlatformModelPath,kLampOffColor);
		SetUnlit(*lamp);
		lamps_.push_back(std::move(lamp));
	}

	bar_ = createObj(kPlatformModelPath,kBarColor);

	// 目印・出発の輪は半透明の画像なので、透明な部分も捨てずに描いて縁をなめらかにする
	marker_ = createObj(kRingModelPath,kInkColor);
	launchRing_ = createObj(kRingModelPath,kInkColor);
	for(Obj3D* ring : {marker_.get(), launchRing_.get()}){
		SetUnlit(*ring);
		if(Model::Material* material = ring->GetMaterial()){
			material->alphaCutoff = kTranslucentAlphaCutoff;
		}
	}

	ResetGate();
}

// レールの始点に合わせて台を置く
// 上面の高さはレールの始点にそろえ、前の端がレールの始点よりkRailOverlapLengthだけ先に出るよう、進行方向の手前側に置く
void StartPlatform::Place(const Vector3& railStartPosition,const Vector3& railStartForward){
	// 台は水平に置くため、進行方向は上下の成分を除いた向きにする
	forwardXZ_ = Normalize(Vector3{railStartForward.x, 0.0f, railStartForward.z});
	rightXZ_ = {forwardXZ_.z, 0.0f, -forwardXZ_.x};
	yaw_ = std::atan2(forwardXZ_.x,forwardXZ_.z);

	// 上面の中央は、前の端から台の長さの半分だけ手前になる
	topCenter_ = railStartPosition - forwardXZ_ * (kHalfLength - kRailOverlapLength);
	// プレイヤーは上面の中央より後ろに立たせ、ゲートまで歩いて向かわせる
	spawnPosition_ = topCenter_ - forwardXZ_ * kSpawnBackOffset;

	if(!obj_){
		return;
	}

	// モデルの中心は上面から厚みの半分だけ下になる
	obj_->SetTranslate({topCenter_.x, topCenter_.y - kHalfHeight, topCenter_.z});
	// モデルの+Z方向を台の前方向へ向ける
	obj_->SetRotate({0.0f, yaw_, 0.0f});
	obj_->SetScale({kHalfWidth, kHalfHeight, kHalfLength});
	// Play中は台が動かないため、ワールド行列はここで求めておく(カメラとの合成はDraw()の中で毎回行われる)
	obj_->Update();

	// ゲートの柱と梁も動かないため、ここで行列を求めておく
	// 柱は台の左右の端に立て、梁は柱の外側の端から端まで渡す
	const float pillarOffsetX = kHalfWidth - kPillarSize.x * kHalf;
	for(int i = 0; i < kPillarCount; ++i){
		UpdatePart(*gateFrames_[i],{pillarOffsetX * kPillarSides[i], kPillarSize.y * kHalf, kGateDistance},kPillarSize);
	}
	UpdatePart(*gateFrames_.back(),{0.0f, kPillarSize.y + kBeamHeight * kHalf, kGateDistance},{kHalfWidth + kHalfWidth, kBeamHeight, kBeamDepth});
}

// ゲートを閉じた状態に戻す
void StartPlatform::ResetGate(){
	gateOpenTimer_ = 0.0f;
	isGateOpening_ = false;
	isBarBurstTriggered_ = false;
	markerTimer_ = 0.0f;
	launchRingTimer_ = 0.0f;
	isLaunchRingActive_ = false;
}

// ゲートを開け始める
void StartPlatform::BeginOpenGate(){
	gateOpenTimer_ = 0.0f;
	isGateOpening_ = true;
	// 出発の操作をした目印の上にインクを塗り、そこから柱へインクが走り出すようにする
	PaintInk(ToWorld({0.0f, 0.0f, kGateDistance - kMarkerDistanceFromGate}),kGateStampRadius);
}

// ゲートが開ききったか
bool StartPlatform::IsGateOpened() const{
	return isGateOpening_ && gateOpenTimer_ >= kGateOpenedTime;
}

// 遮断棒が跳ね上がるときにインクを飛び散らせる位置
Vector3 StartPlatform::GetGateBurstPosition() const{
	return ToWorld({0.0f, kBarClosedHeight, kGateDistance});
}

// ゲートの中央の根元を原点にした座標をワールド座標に変換する
Vector3 StartPlatform::GateLocalToWorld(const Vector3& local) const{
	return ToWorld({local.x, local.y, local.z + kGateDistance});
}

// 出発の合図として、足元から輪を広げる
void StartPlatform::EmitLaunchRing(const Vector3& position){
	launchRingPosition_ = position;
	launchRingTimer_ = 0.0f;
	isLaunchRingActive_ = true;
}

// 台の上面にインクを1滴塗る
void StartPlatform::PaintInk(const Vector3& position,float radius){
	if(inkCanvas_){
		inkCanvas_->AddSplat(position,radius,kTopNormal,kInkColor);
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
	if(inkCanvas_){
		inkCanvas_->Clear();
	}
	for(PaintCanvas* canvas : gateFrameCanvases_){
		canvas->Clear();
	}
}

// 予約したインクを台・柱・梁の塗りテクスチャに描き込む
void StartPlatform::FlushInk(){
	if(inkCanvas_ && obj_){
		inkCanvas_->Flush(*obj_);
	}
	for(size_t i = 0; i < gateFrames_.size(); ++i){
		gateFrameCanvases_[i]->Flush(*gateFrames_[i]);
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
	const float beamStartTime = kInkRunDuration + kInkClimbDuration;

	// 柱・梁は手前側(プレイヤーが近づいてくる側)の面を塗る
	const Vector3 frontNormal = -forwardXZ_;
	const float pillarOffsetX = kHalfWidth - kPillarSize.x * kHalf;
	const float pillarFrontZ = kGateDistance - kPillarSize.z * kHalf;
	const float beamCenterHeight = kPillarSize.y + kBeamHeight * kHalf;
	const float beamFrontZ = kGateDistance - kBeamDepth * kHalf;
	const Vector3 markerLocal = {0.0f, 0.0f, kGateDistance - kMarkerDistanceFromGate};
	PaintCanvas* beamCanvas = gateFrameCanvases_.back();

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
				PaintInk(ToWorld(markerLocal + runDirection * distance + runSide * wobble),kFloorInkRadius);
			});

		// 2. 柱の手前の面を、下から上へ塗り上がる
		PaintCanvas* pillarCanvas = gateFrameCanvases_[i];
		ForEachPointInRange(stageProgress(previousTimer,kInkRunDuration,kInkClimbDuration) * kPillarSize.y,stageProgress(gateOpenTimer_,kInkRunDuration,kInkClimbDuration) * kPillarSize.y,kClimbInkSpacing,
			[&](float height){
				pillarCanvas->AddSplat(ToWorld({pillarOffsetX * side, height, pillarFrontZ}),kClimbInkRadius,frontNormal,kInkColor);
			});

		// 3. 柱の上から梁の中央へ向かって、梁の手前の面を塗る(左右から塗り進めて中央で合わせる)
		ForEachPointInRange(stageProgress(previousTimer,beamStartTime,kBeamFillDuration) * pillarOffsetX,stageProgress(gateOpenTimer_,beamStartTime,kBeamFillDuration) * pillarOffsetX,kBeamInkSpacing,
			[&](float distance){
				beamCanvas->AddSplat(ToWorld({(pillarOffsetX - distance) * side, beamCenterHeight, beamFrontZ}),kBeamInkRadius,frontNormal,kInkColor);
			});
	}
}

// 柱を塗り上がったインクの高さ
float StartPlatform::GetInkClimbHeight() const{
	if(!isGateOpening_){
		return 0.0f;
	}
	return std::clamp((gateOpenTimer_ - kInkRunDuration) / kInkClimbDuration,0.0f,1.0f) * kPillarSize.y;
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

	// ランプは柱の手前側(プレイヤーが近づいてくる側)の面に貼り付け、柱を塗り上がるインクがランプの高さに来たものから点ける
	// 点いた瞬間は明るく光らせ、少しずつ点いたときの色に落ち着かせる
	const float pillarOffsetX = kHalfWidth - kPillarSize.x * kHalf;
	const float lampDistance = kGateDistance - (kPillarSize.z + kLampSize.z) * kHalf;
	const float inkClimbHeight = GetInkClimbHeight();
	const float inkClimbSpeed = kPillarSize.y / kInkClimbDuration;
	for(int side = 0; side < kPillarCount; ++side){
		for(int row = 0; row < kLampCountPerPillar; ++row){
			Obj3D& lamp = *lamps_[side * kLampCountPerPillar + row];
			const float lampHeight = kLampBottomHeight + kLampSpacing * static_cast<float>(row);
			Vector4 lampColor = kLampOffColor;
			if(isGateOpening_ && inkClimbHeight >= lampHeight){
				// インクがランプの高さを過ぎてからの時間で、光った色から点いたときの色へ戻していく
				const float timeSinceLit = (inkClimbHeight - lampHeight) / inkClimbSpeed;
				lampColor = LerpColor(kLampFlashColor,kLampOnColor,std::clamp(timeSinceLit / kLampFlashDuration,0.0f,1.0f));
			}
			SetColor(lamp,lampColor);
			UpdatePart(lamp,{pillarOffsetX * kPillarSides[side], lampHeight, lampDistance},kLampSize);
		}
	}

	// 遮断棒は左右の柱の内側の面の間をふさぐ長さにし、梁まで塗り終わってから跳ね上がり、少し行き過ぎてから戻って止まるように上げる
	float barProgress = 0.0f;
	if(isGateOpening_){
		barProgress = std::clamp((gateOpenTimer_ - kBarStartTime) / kBarOpenDuration,0.0f,1.0f);
	}
	const float barHalfLength = pillarOffsetX - kPillarSize.x * kHalf;
	float barHeight = Lerp(kBarClosedHeight,kBarOpenedHeight,Easing::EaseOutBack(barProgress));
	UpdatePart(*bar_,{0.0f, barHeight, kGateDistance},{barHalfLength + barHalfLength, kBarThickness, kBarThickness});

	// 目印はゆっくり点滅させて、ここに立てばよいことを伝える
	float blinkRate = (std::sin(markerTimer_ * kMarkerBlinkSpeed) + 1.0f) * kWaveToRateScale;
	SetColor(*marker_,{kInkColor.x, kInkColor.y, kInkColor.z, kMarkerMinAlpha + (1.0f - kMarkerMinAlpha) * blinkRate});
	const float markerScale = kMarkerRadius / kRingPlaneRadius;
	marker_->SetTranslate(ToWorld({0.0f, kDecalLift, kGateDistance - kMarkerDistanceFromGate}));
	marker_->SetRotate({0.0f, yaw_, 0.0f});
	marker_->SetScale({markerScale, markerScale, markerScale});
	marker_->Update();

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
	for(auto& frame : gateFrames_){
		DrawWithActiveCamera(*frame);
	}
	for(auto& lamp : lamps_){
		DrawWithActiveCamera(*lamp);
	}
	DrawWithActiveCamera(*bar_);
}

// 足元の目印と出発の輪の描画処理
void StartPlatform::DrawEffects(){
	// 目印はゲートを開け始めたら役目を終えるので消す
	if(marker_ && !isGateOpening_){
		DrawWithActiveCamera(*marker_);
	}
	if(launchRing_ && isLaunchRingActive_){
		DrawWithActiveCamera(*launchRing_);
	}
}

// 水平方向(XZ平面)で見て、指定した座標が台の上面の範囲に入っているか
// 台の中心からの差を台の前方向・右方向の成分に分け、それぞれが大きさの半分以内かで判定する
bool StartPlatform::IsInsideTop(const Vector3& position) const{
	Vector3 diff = {position.x - topCenter_.x, 0.0f, position.z - topCenter_.z};
	return std::abs(Dot(diff,forwardXZ_)) <= kHalfLength && std::abs(Dot(diff,rightXZ_)) <= kHalfWidth;
}

// ゲートが閉じていて、ゲートの前の目印の上に立っているか
bool StartPlatform::IsInGateArea(const Vector3& position) const{
	if(isGateOpening_ || std::abs(position.y - topCenter_.y) > kGateAreaHeightRange){
		return false;
	}
	Vector3 markerPosition = ToWorld({0.0f, 0.0f, kGateDistance - kMarkerDistanceFromGate});
	Vector3 diffXZ = {position.x - markerPosition.x, 0.0f, position.z - markerPosition.z};
	return Length(diffXZ) <= kGateAreaRadius;
}

// ゲートが閉じている間は、ゲートより先へ進めないように押し戻す
// 台の上面の中央から見た前方向の距離が、ゲートの手前の線を越えた分だけ後ろへ戻す
Vector3 StartPlatform::ClampBehindClosedGate(const Vector3& position) const{
	// 押し戻す線の上にいても、まだレールの真上(歩いて乗れる範囲)には入らない位置にゲートを置く
	static_assert(kHalfLength - kRailOverlapLength - (kGateDistance - kGateWallMargin) > Player::kOnRailHorizontalThreshold,
		"ゲートが閉じている間にレールへ乗れてしまう位置にゲートがある");

	if(isGateOpening_){
		return position;
	}
	Vector3 diff = {position.x - topCenter_.x, 0.0f, position.z - topCenter_.z};
	float overshoot = Dot(diff,forwardXZ_) - (kGateDistance - kGateWallMargin);
	if(overshoot <= 0.0f){
		return position;
	}
	return position - forwardXZ_ * overshoot;
}

// 台の上を歩いている間に端から落ちないよう、台の上面の範囲に押し戻す
// 台の中心からの差を台の前方向・右方向の成分に分け、それぞれを端から少し内側までの範囲に収めてから座標に戻す
Vector3 StartPlatform::ClampInsideTop(const Vector3& position) const{
	Vector3 diff = {position.x - topCenter_.x, 0.0f, position.z - topCenter_.z};
	const float limitForward = kHalfLength - kEdgeMargin;
	const float limitRight = kHalfWidth - kEdgeMargin;
	const float forwardAmount = std::clamp(Dot(diff,forwardXZ_),-limitForward,limitForward);
	const float rightAmount = std::clamp(Dot(diff,rightXZ_),-limitRight,limitRight);
	return {
		topCenter_.x + forwardXZ_.x * forwardAmount + rightXZ_.x * rightAmount,
		position.y,
		topCenter_.z + forwardXZ_.z * forwardAmount + rightXZ_.z * rightAmount
	};
}

// 台の上面の中央を原点にした、台の向きでの座標をワールド座標に変換する
Vector3 StartPlatform::ToWorld(const Vector3& local) const{
	return topCenter_ + rightXZ_ * local.x + Vector3{0.0f, local.y, 0.0f} + forwardXZ_ * local.z;
}

// ゲートの部品の位置・回転・大きさを設定して行列を更新する
// 立方体のモデルは -1〜1 なので、各辺の長さの半分をスケールにする
void StartPlatform::UpdatePart(Obj3D& obj,const Vector3& local,const Vector3& size){
	obj.SetTranslate(ToWorld(local));
	obj.SetRotate({0.0f, yaw_, 0.0f});
	obj.SetScale(size * kHalf);
	obj.Update();
}
