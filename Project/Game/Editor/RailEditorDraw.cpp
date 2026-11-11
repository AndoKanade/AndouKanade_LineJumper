#include "RailEditor.h"
#include "RailEditorInternal.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "EditorContext.h"
#include <algorithm>
#include <cmath>

// RailEditorのうち、見た目(制御点の目印・Edit中の曲線の管・Play中のレール本体の管・通った区間の塗り・鍵のレールの出し入れ)をまとめたファイル

using namespace RailEditorInternal;

namespace{
	// 今乗っている(アクティブな)レールの色(目立つ黄色)
	constexpr Vector4 kActiveRailColor = {1.0f, 0.9f, 0.1f, 1.0f};
	// 乗っていないレールの色(黒)
	constexpr Vector4 kInactiveRailColor = {0.0f, 0.0f, 0.0f, 1.0f};
	// 一覧で選択中の制御点の色(アクティブなレールの黄色と見分けやすい水色)
	constexpr Vector4 kSelectedPointColor = {0.1f, 0.9f, 1.0f, 1.0f};
	// チェックポイントにした制御点の色(どこからやり直すかEdit中に分かるよう、レールの色と違う緑にする)
	constexpr Vector4 kCheckpointColor = {0.2f, 1.0f, 0.4f, 1.0f};

	// Play中のレール本体の色(インクで塗った区間は自機のインクの黄色、まだ通っていない区間はピンク)
	constexpr Vector4 kPaintedRailColor = {1.0f, 0.85f, 0.1f, 1.0f};
	constexpr Vector4 kUnpaintedRailColor = {1.0f, 0.4f, 0.75f, 1.0f};
	// 出ていないレールの点線の色(まだ乗れないことが分かるよう、塗っていない区間のピンクより目立たない白)
	constexpr Vector4 kLockedRailColor = {0.85f, 0.85f, 0.9f, 1.0f};
	// Play中のレール本体の管に貼るテクスチャ(白一色。色はマテリアルの色で付ける)
	const std::string kTubeTexturePath = "resource/RailTube/white.png";
	// 管の1区間の中で、塗ったかどうかを判定する位置(区間の長さに対する割合。中央で判定する)
	constexpr float kTubeSegmentMiddleRate = 0.5f;
	// 管の輪の横向きを最初に決めるときの基準の上方向(真上)
	constexpr Vector3 kTubeFrameUp = {0.0f, 1.0f, 0.0f};
	// レールが真上・真下を向いていて真上を基準にできないときに使う基準の向き
	constexpr Vector3 kTubeFrameFallbackUp = {1.0f, 0.0f, 0.0f};
	// 管の向きが一度も定まらない(制御点がすべて重なっている)ときに使う向き
	constexpr Vector3 kDefaultTubeTangent = {0.0f, 0.0f, 1.0f};

	// 着地できる範囲を示す点列の色(レール本体と区別できるよう緑にする)
	constexpr Vector4 kLandingRangeColor = {0.2f, 1.0f, 0.4f, 1.0f};

	// 制御点の目印の表示スケール
	constexpr float kControlPointScale = 0.2f;
	// 選択中の制御点の目印の表示スケール(どの点を編集しているか一目で分かるよう、他の制御点より大きくする)
	constexpr float kSelectedPointScale = 0.35f;
	// Edit中のレール曲線の管の半径(線のように見せるため制御点より細くする)
	constexpr float kCurveTubeRadius = 0.04f;
	// Edit中の着地できる範囲の境界の管の半径
	constexpr float kLandingRangeTubeRadius = 0.06f;
	// 1本の管に必要な輪の数(始点と終点の2つ)
	constexpr size_t kMinTubeRingCount = 2;
}

// デバッグ用の描画処理
void RailEditor::Draw(){
	// Playモード中は編集用の目印(制御点の球・曲線の点列・着地範囲)を出さず、レール本体の管だけを描く
	if(EditorContext::GetInstance()->IsPlayMode()){
		// Play中はレールの形が変わらないため、管の行列はPlayに入ってから最初の1回だけ求める
		if(!areTubesBuilt_){
			BuildTubes();
		}
		DrawTubes();
		return;
	}

	// Edit中はレールが編集されうるため、次にPlayに入ったときに管の行列を求め直す
	areTubesBuilt_ = false;

	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();

	// レールの乗り換えを目視確認できるよう、アクティブなレールだけでなく全レールを描画する
	// 今乗っているレールは目立つ色、それ以外は黒で描画する
	for(size_t railIndex = 0; railIndex < rails_.size(); ++railIndex){
		Rail& rail = rails_[railIndex];

		Vector4 railColor = (static_cast<int>(railIndex) == activeRailIndex_)?kActiveRailColor:kInactiveRailColor;

		// 表示OFFのときは制御点のモデルを描画しない
		if(rail.showControlPointModels){
			// 制御点ごとに専用の3Dオブジェクトで描画
			for(size_t i = 0; i < rail.controlPoints.size(); ++i){
				// 一覧で選択中の制御点(アクティブなレールのものだけ)は、色を変えて大きく描き、編集中の点を見分けやすくする
				const bool isSelectedPoint = static_cast<int>(railIndex) == activeRailIndex_ && static_cast<int>(i) == selectedPointIndex_;
				const float pointScale = isSelectedPoint?kSelectedPointScale:kControlPointScale;

				rail.pointObjects[i]->SetTranslate(rail.controlPoints[i].position);
				// 他のオブジェクトを隠さないよう、球はレールの目印として分かる程度の小ささにする
				rail.pointObjects[i]->SetScale({pointScale, pointScale, pointScale});
				// 選択中でないチェックポイントは緑で描き、どこからやり直すか分かるようにする
				if(Model::Material* mat = rail.pointObjects[i]->GetMaterial()){
					if(isSelectedPoint){
						mat->color = kSelectedPointColor;
					} else if(rail.controlPoints[i].isCheckpoint){
						mat->color = kCheckpointColor;
					} else{
						mat->color = railColor;
					}
				}

				// アクティブカメラに毎フレーム同期
				if(activeCamera){
					rail.pointObjects[i]->SetCamera(activeCamera);
				}

				rail.pointObjects[i]->Update();
				rail.pointObjects[i]->Draw();
			}
		}

		// Catmull-Romは制御点4つ以上必要なので、それ未満のときは曲線・着地できる範囲を描画しない
		if(rail.controlPoints.size() < 4 || (!rail.showCurve && !showLandingRange_)){
			continue;
		}

		// 曲線・着地できる範囲の管を、レールの形が変わっていれば作り直す
		// 管の頂点はワールド座標で作っているため、オブジェクトの位置・向き・大きさは初期値のまま使う
		UpdateEditTubes(rail);

		// レール曲線を細い管で可視化する
		if(rail.showCurve && rail.curveModel){
			if(Model::Material* mat = rail.curveObject->GetMaterial()){
				mat->color = railColor;
			}
			if(activeCamera){
				rail.curveObject->SetCamera(activeCamera);
			}
			rail.curveObject->Draw();
		}

		// ジャンプしてレールに着地できる範囲を可視化する
		// 判定は「レール最近傍点から水平方向にlandingRangeRadius_以内」なので、
		// レールの左右へその距離ぶん離した2本の管を描いて範囲の境界を示す
		if(showLandingRange_ && rail.landingRangeModel){
			if(Model::Material* mat = rail.landingRangeObject->GetMaterial()){
				mat->color = kLandingRangeColor;
			}
			if(activeCamera){
				rail.landingRangeObject->SetCamera(activeCamera);
			}
			rail.landingRangeObject->Draw();
		}
	}
}

// 制御点数pointCountのレールで、Edit中の曲線の管の輪の数を求める
// 区間数に比例した数の輪を並べ、レールを延長しても曲線が角ばらないようにする(終点の分だけ1つ多い)
size_t RailEditor::GetCurveSampleCount(size_t pointCount) const{
	// Catmull-Romは制御点4つ以上必要なので、それ未満のときは曲線を描かない
	if(pointCount < 4){
		return 0;
	}
	return (pointCount - kCatmullRomExtraPointCount) * kCurveSamplesPerSegment + 1;
}

// Edit中の曲線と着地できる範囲の管を、レールの形に合わせて作る
// 制御点のドラッグ中などレールの形が変わったフレームだけ作り直し、変わっていなければ前に作った管をそのまま使う
void RailEditor::UpdateEditTubes(Rail& rail){
	if(rail.curveModel && rail.editTubeBuiltControlPoints == rail.controlPoints && rail.editTubeBuiltLandingRangeRadius == landingRangeRadius_){
		return;
	}

	// 曲線の上に輪の中心を並べる(着地できる範囲の管も、この点から左右へずらして作る)
	const size_t ringCount = GetCurveSampleCount(rail.controlPoints.size());
	std::vector<Vector3> curveCenters(ringCount);
	for(size_t i = 0; i < ringCount; ++i){
		const float t = static_cast<float>(i) / static_cast<float>(ringCount - 1);
		curveCenters[i] = ComputePositionOnRail(rail.controlPoints,t);
	}

	Model::ModelData curveData;
	curveData.material.textureFilePath = kTubeTexturePath;
	AppendTubeMesh(curveCenters,kCurveTubeRadius,curveData);
	ApplyEditTubeModel(rail.curveModel,rail.curveObject,std::move(curveData));

	// 着地できる範囲の境界は、曲線の各点から進行方向に垂直な水平方向へlandingRangeRadius_だけ離した左右2列の点を通す
	// 判定は水平方向(XZ平面)のみで行うため、左右の向きも水平に求める
	std::vector<Vector3> rightCenters;
	std::vector<Vector3> leftCenters;
	rightCenters.reserve(ringCount);
	leftCenters.reserve(ringCount);
	for(size_t i = 0; i < ringCount; ++i){
		// 進行方向を前後の点の差から求める(両端は片側だけを使う)
		const Vector3 forward = curveCenters[(std::min)(i + 1,ringCount - 1)] - curveCenters[(i > 0)?i - 1:0];
		Vector3 horizontalRight = {forward.z, 0.0f, -forward.x};
		if(Length(horizontalRight) < kMinTubeDirectionLength){
			// 真下・真上を向いている区間では左右が決まらないため、その点は飛ばして前後の点をつなぐ
			continue;
		}
		horizontalRight = Normalize(horizontalRight) * landingRangeRadius_;
		rightCenters.push_back(curveCenters[i] + horizontalRight);
		leftCenters.push_back(curveCenters[i] - horizontalRight);
	}

	Model::ModelData landingRangeData;
	landingRangeData.material.textureFilePath = kTubeTexturePath;
	AppendTubeMesh(rightCenters,kLandingRangeTubeRadius,landingRangeData);
	AppendTubeMesh(leftCenters,kLandingRangeTubeRadius,landingRangeData);
	ApplyEditTubeModel(rail.landingRangeModel,rail.landingRangeObject,std::move(landingRangeData));

	rail.editTubeBuiltControlPoints = rail.controlPoints;
	rail.editTubeBuiltLandingRangeRadius = landingRangeRadius_;
}

// Edit中の管のモデルを新しい頂点に差し替える
// 制御点の数が変わらなければ頂点の数もインデックスも同じため、頂点バッファを書き換えるだけにする
// (制御点のドラッグ中は毎フレーム形が変わるため、そのたびにGPUのバッファを作り直すと重くなる)
void RailEditor::ApplyEditTubeModel(std::unique_ptr<Model>& model,std::unique_ptr<Obj3D>& object,Model::ModelData&& data){
	// 頂点が無い(管にできる点が足りない)ときは描かない
	// 毎フレームGPUの処理完了を待ってから次のフレームを積むため、ここでモデルを破棄してよい
	if(data.vertices.empty()){
		model.reset();
		if(object){
			object->SetModel(nullptr);
		}
		return;
	}

	if(model && model->GetModelData().vertices.size() == data.vertices.size()){
		model->UpdateVertices(data.vertices);
		return;
	}

	model = std::make_unique<Model>();
	model->Initialize(ModelManager::GetInstance()->GetModelCommon(),std::move(data));
	if(!object){
		object = std::make_unique<Obj3D>();
		object->Initialize(objCommon_);
	}
	object->SetModel(model.get());
}

// 輪の中心ringCentersを順に通る管の頂点とインデックスを、modelDataの末尾に足す
// 輪の中心ごとにレールを囲む輪の頂点を置き、隣り合う輪どうしを三角形でつなぐ
// 頂点は輪の順に並ぶため、ある区間からある区間までのインデックスは連続した範囲になる(塗った区間だけを描くのに使う)
void RailEditor::AppendTubeMesh(const std::vector<Vector3>& ringCenters,float radius,Model::ModelData& modelData){
	if(ringCenters.size() < kMinTubeRingCount){
		return;
	}

	const uint32_t segmentCount = static_cast<uint32_t>(ringCenters.size() - 1);
	// 輪の継ぎ目でテクスチャ座標が1周して戻れるよう、輪の最初と最後に同じ位置の頂点を置く
	const uint32_t ringVertexCount = kTubeSideCount + 1;
	// 先に足した管の頂点の後ろに並べるため、インデックスはその数だけずらす
	const uint32_t baseVertex = static_cast<uint32_t>(modelData.vertices.size());

	// 輪の頂点を置く
	Vector3 tangent = kDefaultTubeTangent;
	Vector3 side = {0.0f, 0.0f, 0.0f};
	for(uint32_t i = 0; i <= segmentCount; ++i){
		// 輪の向き(レールの進む向き)は、前後の輪の中心の差から求める(両端は片側だけを使う)
		// 制御点が重なっていて向きが定まらないときは、前の輪の向きをそのまま使う
		const Vector3 direction = ringCenters[(std::min)(i + 1,segmentCount)] - ringCenters[(i > 0)?i - 1:0];
		if(Length(direction) >= kMinTubeDirectionLength){
			tangent = Normalize(direction);
		}

		// 輪の横向きは、前の輪の横向きから進む向きの成分を取り除いて引き継ぐ
		// 輪ごとに真上から決め直すと、螺旋などで向きが急に変わる所で管がねじれて見えるため
		side = side - tangent * Dot(side,tangent);
		if(Length(side) < kMinTubeDirectionLength){
			// 最初の輪と、引き継げないほど向きが変わったときは、基準の上方向から決め直す
			side = Cross(kTubeFrameUp,tangent);
			if(Length(side) < kMinTubeDirectionLength){
				side = Cross(kTubeFrameFallbackUp,tangent);
			}
		}
		side = Normalize(side);
		const Vector3 up = Cross(tangent,side);

		const float texcoordV = static_cast<float>(i) / static_cast<float>(segmentCount);
		for(uint32_t j = 0; j < ringVertexCount; ++j){
			const float sideRate = static_cast<float>(j) / static_cast<float>(kTubeSideCount);
			const float angle = kTwoPi * sideRate;
			// 輪の中心から外へ向かう向きを、そのまま頂点の法線にする
			const Vector3 normal = side * std::cos(angle) + up * std::sin(angle);
			const Vector3 position = ringCenters[i] + normal * radius;

			Model::VertexData vertex = {};
			vertex.position = {position.x, position.y, position.z, 1.0f};
			vertex.texcoord = {sideRate, texcoordV};
			vertex.normal = normal;
			modelData.vertices.push_back(vertex);
		}
	}

	// 隣り合う輪の頂点を、区間の順に四角形(三角形2つ)でつなぐ
	for(uint32_t i = 0; i < segmentCount; ++i){
		const uint32_t ringStart = baseVertex + i * ringVertexCount;
		const uint32_t nextRingStart = ringStart + ringVertexCount;
		for(uint32_t j = 0; j < kTubeSideCount; ++j){
			const uint32_t current = ringStart + j;
			const uint32_t currentNext = current + 1;
			const uint32_t forward = nextRingStart + j;
			const uint32_t forwardNext = forward + 1;
			// 管の外側から見て表になる向きで並べる
			modelData.indices.insert(modelData.indices.end(),{current, currentNext, forward, currentNext, forwardNext, forward});
		}
	}
}

// Play中のレール本体の管のモデルを作る
// レールを細かい区間に分け、区間の境目ごとにレールを囲む輪の頂点を置き、隣り合う輪どうしを三角形でつないで1本の管にする
// 頂点はレールの始点から順に並ぶため、ある区間からある区間までのインデックスは連続した範囲になる(塗った区間だけを描くのに使う)
void RailEditor::BuildTubes(){
	ModelCommon* modelCommon = ModelManager::GetInstance()->GetModelCommon();

	// ステージを選ぶスタート台の上から全ステージのレールが見えるよう、今のステージ以外のレールの管も作る
	for(int stage = 0; stage < StageInfo::kStageCount; ++stage){
		for(Rail& rail : GetStageRails(stage)){
			// Catmull-Romは制御点4つ以上必要なので、それ未満のときは管を作らない
			if(rail.controlPoints.size() < 4){
				rail.tubeModel.reset();
				rail.dashedTubeModel.reset();
				rail.tubeSegmentCount = 0;
				rail.tubeRingDistances.clear();
				rail.tubeBuiltControlPoints.clear();
				continue;
			}

			// レールの始点と終点に制御点の目印を置く
			// Catmull-Romの曲線は2番目の制御点から最後の1つ前の制御点までを通るため、その2点がレールの始点と終点になる
			// Play中は形が変わらないため、目印の行列も管と同じくここで1回だけ求める
			for(size_t pointIndex : {kRailStartPointIndex, rail.controlPoints.size() - 1 - kRailStartPointIndex}){
				Obj3D* marker = rail.pointObjects[pointIndex].get();
				marker->SetTranslate(rail.controlPoints[pointIndex].position);
				marker->SetScale({kControlPointScale, kControlPointScale, kControlPointScale});
				marker->Update();
			}

			// 前回管を作ったときから形が変わっていなければ、作り直さずにそのまま使う
			if(rail.tubeModel && rail.tubeBuiltControlPoints == rail.controlPoints){
				continue;
			}

			// 区間数に比例した数に分け、レールを延長しても管が角ばらないようにする
			const uint32_t segmentCount = static_cast<uint32_t>(rail.controlPoints.size() - kCatmullRomExtraPointCount) * static_cast<uint32_t>(kTubeSegmentsPerRailSegment);
			// 輪の継ぎ目でテクスチャ座標が1周して戻れるよう、輪の最初と最後に同じ位置の頂点を置く(頂点の数を見積もるのに使う)
			const uint32_t ringVertexCount = kTubeSideCount + 1;

			// 輪の中心(区間の境目)の座標を先に求めておき、輪の向きを求めるときに同じ座標を何度も計算しないようにする
			std::vector<Vector3> ringCenters(segmentCount + 1);
			for(uint32_t i = 0; i <= segmentCount; ++i){
				ringCenters[i] = ComputePositionOnRail(rail.controlPoints,static_cast<float>(i) / static_cast<float>(segmentCount));
			}

			// 始点から各輪までの長さを、隣り合う輪の中心の間の距離を足していって求める
			// 区間は進行度で等分しているため長さがそろっておらず、管を一定の速さで伸ばすには長さで描く区間を決める必要がある
			rail.tubeRingDistances.assign(segmentCount + 1,0.0f);
			for(uint32_t i = 1; i <= segmentCount; ++i){
				rail.tubeRingDistances[i] = rail.tubeRingDistances[i - 1] + Distance(ringCenters[i],ringCenters[i - 1]);
			}

			Model::ModelData modelData;
			modelData.material.textureFilePath = kTubeTexturePath;
			modelData.vertices.reserve(static_cast<size_t>(segmentCount + 1) * ringVertexCount);
			modelData.indices.reserve(static_cast<size_t>(segmentCount) * kTubeSideCount * kTubeIndicesPerQuad);

			// 輪の中心を通る管の頂点とインデックスを作る
			AppendTubeMesh(ringCenters,kTubeRadius,modelData);

			// 出ていないレールの点線は、管と同じ頂点のうち、点線の区間の三角形だけを持つ別のモデルにする
			// 点線の区間ごとに描く範囲を分けると描画の命令が線の本数だけ増えるため、点線の区間だけを詰めて1回で描けるようにする
			constexpr uint32_t kIndicesPerSegment = kTubeSideCount * kTubeIndicesPerQuad;
			Model::ModelData dashedModelData;
			dashedModelData.material.textureFilePath = kTubeTexturePath;
			dashedModelData.vertices = modelData.vertices;
			dashedModelData.indices.reserve(static_cast<size_t>(CountDashedSegmentsBefore(segmentCount)) * kIndicesPerSegment);
			for(uint32_t i = 0; i < segmentCount; ++i){
				if(i % kDashPeriodSegments >= kDashDrawSegments){
					continue;
				}
				const auto segmentBegin = modelData.indices.begin() + static_cast<ptrdiff_t>(i) * kIndicesPerSegment;
				dashedModelData.indices.insert(dashedModelData.indices.end(),segmentBegin,segmentBegin + kIndicesPerSegment);
			}

			rail.tubeModel = std::make_unique<Model>();
			rail.tubeModel->Initialize(modelCommon,std::move(modelData));
			rail.dashedTubeModel = std::make_unique<Model>();
			rail.dashedTubeModel->Initialize(modelCommon,std::move(dashedModelData));
			rail.tubeSegmentCount = segmentCount;
			rail.tubeBuiltControlPoints = rail.controlPoints;

			// 管を描くオブジェクトは初めて管を作るときだけ用意し、色もそのときに決める(以降は作り直した管のモデルを渡すだけ)
			// 管の頂点はワールド座標で作っているため、オブジェクトの位置・向き・大きさは初期値のまま使う
			if(!rail.tubeObject){
				rail.tubeObject = std::make_unique<Obj3D>();
				rail.tubeObject->Initialize(objCommon_);
				if(Model::Material* mat = rail.tubeObject->GetMaterial()){
					mat->color = kUnpaintedRailColor;
				}
			}
			if(!rail.paintedTubeObject){
				rail.paintedTubeObject = std::make_unique<Obj3D>();
				rail.paintedTubeObject->Initialize(objCommon_);
				if(Model::Material* mat = rail.paintedTubeObject->GetMaterial()){
					mat->color = kPaintedRailColor;
				}
			}
			if(!rail.dashedTubeObject){
				rail.dashedTubeObject = std::make_unique<Obj3D>();
				rail.dashedTubeObject->Initialize(objCommon_);
				if(Model::Material* mat = rail.dashedTubeObject->GetMaterial()){
					mat->color = kLockedRailColor;
				}
			}
			rail.tubeObject->SetModel(rail.tubeModel.get());
			rail.paintedTubeObject->SetModel(rail.tubeModel.get());
			rail.dashedTubeObject->SetModel(rail.dashedTubeModel.get());
		}
	}

	areTubesBuilt_ = true;
}

// Play中のレール本体を管として描画する
// 管のモデルはBuildTubes()で作り済みのため、ここでは塗った区間に合わせて描くインデックスの範囲とカメラだけを毎フレーム反映する
// 塗った区間は連続しているため、「塗っていない区間・塗った区間・塗っていない区間」の最大3回の描画で済む
// 出ていないレールは点線だけを描き、出ている途中のレールは、伸びた所までを管で、その先を点線で描く
void RailEditor::DrawTubes(){
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
	// 区間1つ分のインデックスの数(どの区間も同じ数の三角形でできている)
	constexpr uint32_t kIndicesPerSegment = kTubeSideCount * kTubeIndicesPerQuad;

	// 管の区間番号beginSegmentからendSegmentの手前までを描く(範囲が空なら描かない)
	auto drawSegments = [activeCamera](Obj3D* tube,uint32_t beginSegment,uint32_t endSegment){
		if(beginSegment >= endSegment){
			return;
		}
		tube->SetDrawIndexRange(beginSegment * kIndicesPerSegment,(endSegment - beginSegment) * kIndicesPerSegment);
		if(activeCamera){
			tube->SetCamera(activeCamera);
		}
		tube->Draw();
	};

	// ステージを選ぶスタート台の上から全ステージのレールが見えるよう、今のステージ以外のレールも描く
	for(int stage = 0; stage < StageInfo::kStageCount; ++stage){
		for(size_t railIndex = 0; railIndex < GetStageRails(stage).size(); ++railIndex){
			Rail& rail = GetStageRails(stage)[railIndex];

			// 管を作ったレールだけ、レールの始点と終点の制御点の目印を描画する(色はEdit中と同じく今乗っているレールかどうかで分ける)
			// 出ていないレールは点線だけにして、まだ乗れないことが分かるようにする
			if(rail.controlPoints.size() >= 4 && !rail.isLocked){
				const bool isRidingRail = stage == stageIndex_ && static_cast<int>(railIndex) == activeRailIndex_;
				Vector4 markerColor = isRidingRail?kActiveRailColor:kInactiveRailColor;
				for(size_t pointIndex : {kRailStartPointIndex, rail.controlPoints.size() - 1 - kRailStartPointIndex}){
					Obj3D* marker = rail.pointObjects[pointIndex].get();
					if(Model::Material* mat = marker->GetMaterial()){
						mat->color = markerColor;
					}
					if(activeCamera){
						marker->SetCamera(activeCamera);
					}
					marker->Draw();
				}
			}

			// 管を作っていないレール(制御点4つ未満)は描かない
			if(!rail.tubeModel){
				continue;
			}

			// 区間の中央の進行度が塗った範囲に入っている区間を、インクの色で描く区間にする
			// 区間iの中央の進行度は (i + 0.5) / 区間数 のため、塗った範囲の両端の進行度から区間番号の範囲を逆算する
			const uint32_t segmentCount = rail.tubeSegmentCount;
			uint32_t paintedBegin = 0;
			uint32_t paintedEnd = 0;
			if(rail.isPainted){
				const float segmentCountF = static_cast<float>(segmentCount);
				const float firstSegment = std::ceil(rail.paintStartT * segmentCountF - kTubeSegmentMiddleRate);
				const float lastSegment = std::floor(rail.paintEndT * segmentCountF - kTubeSegmentMiddleRate);
				paintedBegin = static_cast<uint32_t>(std::clamp(firstSegment,0.0f,segmentCountF));
				paintedEnd = (std::max)(paintedBegin,static_cast<uint32_t>(std::clamp(lastSegment + 1.0f,0.0f,segmentCountF)));
			}

			// 管を伸ばし終えた区間の数(出ていないレールは0、伸ばし終えたレールは全区間)
			// 始点から伸ばした長さより手前で始まる区間を、伸ばし終えた区間とする
			uint32_t grownEnd = 0;
			if(!rail.isLocked){
				const auto segmentStarts = rail.tubeRingDistances.begin();
				grownEnd = static_cast<uint32_t>(std::lower_bound(segmentStarts,segmentStarts + segmentCount,rail.unlockGrownLength) - segmentStarts);
			}

			drawSegments(rail.tubeObject.get(),0,(std::min)(paintedBegin,grownEnd));
			drawSegments(rail.paintedTubeObject.get(),paintedBegin,(std::min)(paintedEnd,grownEnd));
			drawSegments(rail.tubeObject.get(),paintedEnd,grownEnd);

			// まだ管が伸びていない区間は点線で描く(点線のモデルは点線の区間だけを詰めて持つため、区間番号を点線の数に直して描く)
			if(grownEnd < segmentCount && rail.dashedTubeObject){
				drawSegments(rail.dashedTubeObject.get(),CountDashedSegmentsBefore(grownEnd),CountDashedSegmentsBefore(segmentCount));
			}
		}
	}
}

// 管の区間番号segmentより手前にある、点線の区間の数を求める
// 点線は kDashPeriodSegments 区間ごとに、先頭の kDashDrawSegments 区間を描く
uint32_t RailEditor::CountDashedSegmentsBefore(uint32_t segment){
	return (segment / kDashPeriodSegments) * kDashDrawSegments + (std::min)(segment % kDashPeriodSegments,kDashDrawSegments);
}

// アクティブなレールの、進行度tの位置までを塗った区間に含める
// 初めて乗ったときはその位置から塗り始め、以降は進んだ分だけ塗る範囲を広げる
void RailEditor::PaintActiveRail(float t){
	if(activeRailIndex_ < 0 || activeRailIndex_ >= static_cast<int>(rails_.size())){
		return;
	}

	Rail& rail = rails_[activeRailIndex_];
	if(!rail.isPainted){
		rail.isPainted = true;
		rail.paintStartT = t;
		rail.paintEndT = t;
		return;
	}

	if(t < rail.paintStartT){
		rail.paintStartT = t;
	}
	if(t > rail.paintEndT){
		rail.paintEndT = t;
	}
}

// 全ステージの全レールの塗った区間を消す(今のステージ以外のレールも描くため、前に遊んだステージの塗りも消す)
void RailEditor::ClearPaint(){
	for(int stage = 0; stage < StageInfo::kStageCount; ++stage){
		for(Rail& rail : GetStageRails(stage)){
			rail.isPainted = false;
			rail.paintStartT = 0.0f;
			rail.paintEndT = 0.0f;
		}
	}
}

// 鍵の的が残っていて出ていないレールを反映する
// 出ていなかったレールが出たら、始点から一定の速さで管を伸ばし始める
void RailEditor::UpdateRailLocks(const std::vector<bool>& lockedRails,float growSpeed,float deltaTime){
	const size_t count = (std::min)(rails_.size(),lockedRails.size());
	for(size_t i = 0; i < count; ++i){
		Rail& rail = rails_[i];
		if(lockedRails[i]){
			rail.isLocked = true;
			rail.unlockGrownLength = 0.0f;
		} else if(rail.isLocked){
			rail.isLocked = false;
			rail.unlockGrownLength = 0.0f;
		} else if(rail.unlockGrownLength < kFullyGrownLength){
			// 伸ばしきった後も足し続けると値が大きくなり続けるため、全長を超えたら伸ばしきった状態にする
			rail.unlockGrownLength += growSpeed * deltaTime;
			if(rail.tubeRingDistances.empty() || rail.unlockGrownLength >= rail.tubeRingDistances.back()){
				rail.unlockGrownLength = kFullyGrownLength;
			}
		}
	}
}

// 管を伸ばす演出を飛ばして、出ていないレールをすぐに反映する
void RailEditor::ResetRailLocks(const std::vector<bool>& lockedRails){
	ResetStageRailLocks(stageIndex_,lockedRails);
}

// 指定ステージのレールに、出ていないレールをすぐに反映する
void RailEditor::ResetStageRailLocks(int stageIndex,const std::vector<bool>& lockedRails){
	if(!StageInfo::IsValidStageIndex(stageIndex)){
		return;
	}
	std::vector<Rail>& rails = GetStageRails(stageIndex);
	const size_t count = (std::min)(rails.size(),lockedRails.size());
	for(size_t i = 0; i < count; ++i){
		rails[i].isLocked = lockedRails[i];
		rails[i].unlockGrownLength = kFullyGrownLength;
	}
}
