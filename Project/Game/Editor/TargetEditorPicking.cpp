#include "TargetEditor.h"
#include "ImGuiManager.h"
#include "imgui.h"
#include "CameraManager.h"
#include "Camera.h"
#include "EditorContext.h"
#include "WinAPI.h"
#include <cmath>

// TargetEditorのうち、Sceneビューでのマウス操作(マウスのレイ・的を掴む判定・ドラッグでの移動・スナップ)をまとめたファイル

namespace{
	// レイと平面が平行かどうかを判定するしきい値(内積の絶対値がこの値未満なら平行とみなす)
	constexpr float kRayPlaneEpsilon = 1e-6f;

	// 高さを保って動かすときのドラッグ面の法線(水平な面)
	constexpr Vector3 kHorizontalDragPlaneNormal = {0.0f, 1.0f, 0.0f};

	// 座標に行列を掛け、同次座標のwで割る(NDCからワールド座標への逆変換に使用)
	Vector3 TransformWithDivide(const Vector3& v,const Matrix4x4& m){
		float x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0];
		float y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1];
		float z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2];
		float w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];

		// wが0に近い異常値のときは割らずにそのまま返す
		if(std::fabs(w) < kRayPlaneEpsilon){
			return {x, y, z};
		}
		return {x / w, y / w, z / w};
	}

	// レイと平面の交点を求める(平行、または平面がレイの後方にある場合はfalse)
	bool IntersectRayPlane(const Vector3& origin,const Vector3& direction,const Vector3& planePoint,const Vector3& planeNormal,Vector3& outPos){
		float denom = Dot(direction,planeNormal);
		if(std::fabs(denom) < kRayPlaneEpsilon){
			return false;
		}

		float t = Dot(planePoint - origin,planeNormal) / denom;
		if(t < 0.0f){
			return false;
		}

		outPos = origin + direction * t;
		return true;
	}
}

// スナップがONなら、座標をグリッドに合わせる(高さを保つ移動中は高さはそのままにする)
Vector3 TargetEditor::ApplySnap(const Vector3& position,bool keepsHeight) const{
	if(!isSnapEnabled_ || snapSize_ < kMinSnapSize){
		return position;
	}

	Vector3 snapped = position;
	snapped.x = std::round(position.x / snapSize_) * snapSize_;
	snapped.z = std::round(position.z / snapSize_) * snapSize_;
	if(!keepsHeight){
		snapped.y = std::round(position.y / snapSize_) * snapSize_;
	}
	return snapped;
}

// Editモード中にゲーム画面が実際に描かれている矩形(レターボックス適用後)を求める
// Application.cppのビューポート計算と同じ手順で求めることで、マウス座標と描画結果を一致させる
bool TargetEditor::ComputeGameViewportRect(float& outX,float& outY,float& outW,float& outH) const{
	const EditorRect& sceneRect = EditorContext::GetInstance()->GetSceneViewRect();
	if(sceneRect.w <= 0.0f || sceneRect.h <= 0.0f){
		return false;
	}

	const float targetAspect = static_cast<float>(WinAPI::kClientWidth) / static_cast<float>(WinAPI::kClientHeight);
	const float rectAspect = sceneRect.w / sceneRect.h;

	float viewWidth = 0.0f;
	float viewHeight = 0.0f;
	if(rectAspect > targetAspect){
		// 領域が横長 → 高さを合わせて左右をレターボックス
		viewHeight = sceneRect.h;
		viewWidth = viewHeight * targetAspect;
	} else{
		// 領域が縦長 → 幅を合わせて上下をレターボックス
		viewWidth = sceneRect.w;
		viewHeight = viewWidth / targetAspect;
	}

	outX = sceneRect.x + (sceneRect.w - viewWidth) * 0.5f;
	outY = sceneRect.y + (sceneRect.h - viewHeight) * 0.5f;
	outW = viewWidth;
	outH = viewHeight;
	return true;
}

// 現在のマウス位置から伸びるレイを求める(画面外・カメラ未取得のときはfalse)
bool TargetEditor::ComputeMouseRay(PickRay& outRay) const{
#ifdef USE_IMGUI
	float viewX = 0.0f, viewY = 0.0f, viewW = 0.0f, viewH = 0.0f;
	if(!ComputeGameViewportRect(viewX,viewY,viewW,viewH)){
		return false;
	}

	// ゲーム画面の外にマウスがあるときは対象外
	ImVec2 mousePos = ImGui::GetMousePos();
	if(mousePos.x < viewX || mousePos.x > viewX + viewW || mousePos.y < viewY || mousePos.y > viewY + viewH){
		return false;
	}

	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if(!camera){
		return false;
	}

	// ビューポート内のマウス位置を正規化デバイス座標(NDC)へ変換する
	float ndcX = (mousePos.x - viewX) / viewW * 2.0f - 1.0f;
	float ndcY = 1.0f - (mousePos.y - viewY) / viewH * 2.0f;

	// ビュープロジェクション行列の逆行列で、NDCの near/far 2点をワールド座標へ戻す
	// DirectXの深度範囲は 0(ニアクリップ面)〜1(ファークリップ面)
	Matrix4x4 invViewProjection = Inverse(camera->GetViewProjectionMatrix());
	Vector3 nearPos = TransformWithDivide({ndcX, ndcY, 0.0f},invViewProjection);
	Vector3 farPos = TransformWithDivide({ndcX, ndcY, 1.0f},invViewProjection);

	outRay.origin = nearPos;
	outRay.direction = Normalize(farPos - nearPos);
	return true;
#else
	(void)outRay;
	return false;
#endif
}

// レイに最も近い的のインデックスを求める(掴める距離に無ければ-1)
int TargetEditor::PickTargetIndex(const PickRay& ray) const{
	int bestIndex = -1;
	float bestDistanceAlongRay = 0.0f;

	for(size_t i = 0; i < targets_.size(); ++i){
		Vector3 toTarget = targets_[i].position - ray.origin;

		// レイ方向に対する射影距離。負ならカメラの後方にあるので対象外
		float distanceAlongRay = Dot(toTarget,ray.direction);
		if(distanceAlongRay < 0.0f){
			continue;
		}

		// レイ上の最近傍点と的との距離が掴める範囲に収まっているかを判定する
		Vector3 nearestOnRay = ray.origin + ray.direction * distanceAlongRay;
		if(Distance(nearestOnRay,targets_[i].position) > kPickRadius){
			continue;
		}

		// 複数の的が重なって見えている場合は、手前にあるものを優先する
		if(bestIndex < 0 || distanceAlongRay < bestDistanceAlongRay){
			bestIndex = static_cast<int>(i);
			bestDistanceAlongRay = distanceAlongRay;
		}
	}

	return bestIndex;
}

// マウスドラッグによる的の移動処理
void TargetEditor::UpdateMouseDrag(){
#ifdef USE_IMGUI
	// 配置モードOFF、またはImGuiのウィジェットを操作中のときはドラッグ判定を行わない
	if(!isPlacementMode_ || ImGui::IsAnyItemActive()){
		isDragging_ = false;
		return;
	}

	// 左ボタンが離されていればドラッグ終了
	if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)){
		isDragging_ = false;
	}

	PickRay ray{};
	if(!ComputeMouseRay(ray)){
		return;
	}

	// 左ボタンを押した瞬間、レイ上に的があれば掴む
	int grabIndex = -1;
	if(!isDragging_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
		int hitIndex = PickTargetIndex(ray);
		if(hitIndex < 0 && !ImGui::IsAnyItemHovered()){
			// 何も無い所(敵やレールの制御点を含む)をクリックしたら選択を解除する
			// 他のエディターの物を選んだ後に、Deleteキーなどで的を消してしまわないようにする
			selectedIndex_ = -1;
		}
		if(hitIndex >= 0){
			// 選んでいるエディターが掴んだことを伝え、後ろにある敵などのエディターへ切り替わらないようにする
			EditorContext::GetInstance()->ReportScenePick(kPanelName,Dot(targets_[hitIndex].position - ray.origin,ray.direction));
			grabIndex = hitIndex;
		}
	} else if(!isDragging_ && pendingPickIndex_ >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)){
		// 選んでいないときにクリックして切り替わった的は、ボタンを押したままならそのまま掴んで動かせるようにする
		grabIndex = pendingPickIndex_;
	}
	if(grabIndex >= 0){
		StartDrag(grabIndex,ray);
	}

	// ドラッグ中は、掴んだときの平面とレイの交点へ的を移動させる
	if(isDragging_ && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(targets_.size())){
		Vector3 hitPos{};
		if(IntersectRayPlane(ray.origin,ray.direction,dragPlanePoint_,dragPlaneNormal_,hitPos)){
			Vector3 newPosition = ApplySnap(hitPos + dragOffset_,isDraggingKeepsHeight_);
			// 高さだけのドラッグでは、縦の面の上で横にずれた分は捨てて、掴んだときの水平位置に留める
			if(isDraggingHeightOnly_){
				newPosition.x = dragPlanePoint_.x;
				newPosition.z = dragPlanePoint_.z;
			}
			targets_[selectedIndex_].position = newPosition;
		}
	}
#endif
}

// 指定の的を掴んで、マウスドラッグでの移動を始める
void TargetEditor::StartDrag(int index,const PickRay& ray){
#ifdef USE_IMGUI
	selectedIndex_ = index;
	isDragging_ = true;
	isDraggingHeightOnly_ = ImGui::GetIO().KeyShift;
	isDraggingKeepsHeight_ = keepsHeightWhileDragging_ && !isDraggingHeightOnly_;

	// 動かす面を決める
	// 高さだけ: カメラの方を向いた縦の面(レイ方向の水平成分を法線にする。真下を見ているときは画面に平行な面にする)
	// 高さを保つ: 水平な面 / それ以外: 掴んだ瞬間のレイ方向を法線とする面(画面に平行な移動になる)
	if(isDraggingHeightOnly_){
		Vector3 horizontalDirection = {ray.direction.x, 0.0f, ray.direction.z};
		dragPlaneNormal_ = (Length(horizontalDirection) > kRayPlaneEpsilon)?Normalize(horizontalDirection):ray.direction;
	} else if(isDraggingKeepsHeight_){
		dragPlaneNormal_ = kHorizontalDragPlaneNormal;
	} else{
		dragPlaneNormal_ = ray.direction;
	}
	dragPlanePoint_ = targets_[index].position;

	// 掴んだ位置と的の座標とのずれを保持し、掴んだ瞬間に的が跳ばないようにする
	Vector3 hitPos{};
	if(IntersectRayPlane(ray.origin,ray.direction,dragPlanePoint_,dragPlaneNormal_,hitPos)){
		dragOffset_ = targets_[index].position - hitPos;
	} else{
		dragOffset_ = {0.0f, 0.0f, 0.0f};
	}
#else
	(void)index;
	(void)ray;
#endif
}

// 選んでいないときに、Sceneビューで的をクリックしたかを調べ、掴めたらTarget Editorへの切り替えの候補として報告する
void TargetEditor::ReportScenePickWhileInactive(){
#ifdef USE_IMGUI
	pendingPickIndex_ = -1;
	if(!isPlacementMode_ || ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
		return;
	}

	PickRay ray{};
	if(!ComputeMouseRay(ray)){
		return;
	}
	int hitIndex = PickTargetIndex(ray);
	if(hitIndex < 0){
		return;
	}

	// 切り替わった次のフレームで、この的を選ぶ
	pendingPickIndex_ = hitIndex;
	EditorContext::GetInstance()->ReportScenePick(kPanelName,Dot(targets_[hitIndex].position - ray.origin,ray.direction));
#endif
}
