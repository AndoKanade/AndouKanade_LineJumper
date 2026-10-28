#include "TargetEditor.h"
#include "ImGuiManager.h"
#include "imgui.h"
#include "CameraManager.h"
#include "Camera.h"
#include "EditorWidgets.h"
#include "EditorContext.h"
#include "WinAPI.h"
#include "externals/json.hpp"
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <string>

namespace{
	// 的の配置データの保存先ファイルパス(RailEditorと同じくresource配下の相対パス)
	const std::string kTargetSaveFilePath = "resource/target/target.json";

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

TargetEditor::TargetEditor() = default;
// 破棄したエディターをCtrl+Zや保存の対象に残さないよう、登録を外す
TargetEditor::~TargetEditor(){
	// 保存していない変更があれば、次に作ったエディターで戻せるよう取っておく(変更が無ければ前に取っておいたものも消す)
	// 操作の途中でまだ履歴に積んでいない変更も含めるため、先に確定してから判定する
	CommitEditIfChanged();
	if(history_.IsModified()){
		unsavedStash_ = targets_;
	} else{
		unsavedStash_.reset();
	}

	EditorDocumentManager::GetInstance()->Unregister(this);
}

// 初期化処理(保存済みJSONがあれば読み込む)
void TargetEditor::Initialize(){
	LoadTargetsFromFile();

	// 読み込んだ状態を編集の基準(保存済み)にし、Ctrl+Z・すべて保存の対象に登録する
	history_.Reset(targets_);
	EditorDocumentManager::GetInstance()->Register(this);

	// 前のシーンで保存せずに残していた変更があれば戻す
	// 読み込んだファイルの状態からの1回の編集として積み、未保存のままにする(Ctrl+Zで読み込んだファイルの状態に戻せる)
	if(unsavedStash_){
		targets_ = *unsavedStash_;
		unsavedStash_.reset();
		CommitEditIfChanged();
	}
}

// 更新処理(ImGuiでの編集UIとマウスドラッグによる移動)
void TargetEditor::Update(){
#ifdef USE_IMGUI
	// Playモード中は配置編集を行わない(ドラッグ状態も解除しておく)
	if(EditorContext::GetInstance()->IsPlayMode()){
		isDragging_ = false;
		return;
	}

	// 選択中のインデックスが範囲外になっていたら選択解除する(削除・読み込み後など)
	if(selectedIndex_ >= static_cast<int>(targets_.size())){
		selectedIndex_ = -1;
	}

	// 下段の選択パネルで選ばれているときだけ右の列に出す
	// 選ばれていないときは、Sceneビューでのドラッグ・ショートカットも受け付けない(敵などと一緒に掴まないようにする)
	// 選ばれていなくても、Sceneビューで的をクリックしたらTarget Editorへ切り替えられるよう報告する
	if(!EditorWidgets::BeginToolPanel(kPanelName)){
		isDragging_ = false;
		ReportScenePickWhileInactive();
		return;
	}

	// 選んでいないときにSceneビューでクリックした的があれば、切り替わったこのフレームで選ぶ
	if(pendingPickIndex_ >= static_cast<int>(targets_.size())){
		pendingPickIndex_ = -1;
	}
	if(pendingPickIndex_ >= 0){
		selectedIndex_ = pendingPickIndex_;
	}

	// 配置モードのON/OFF切り替え(ONの間だけSceneビュー上のドラッグ移動を受け付ける)
	ImGui::Checkbox("Placement Mode",&isPlacementMode_);
	ImGui::BeginDisabled(!isPlacementMode_);
	// ドラッグ中に高さを保つか(ONなら水平に動かし、高さはShift+ドラッグで変える)
	ImGui::Checkbox("Keep Height",&keepsHeightWhileDragging_);
	ImGui::SameLine();
	// ドラッグで動かした座標をグリッドに合わせるか
	ImGui::Checkbox("Snap",&isSnapEnabled_);
	ImGui::SameLine();
	ImGui::BeginDisabled(!isSnapEnabled_);
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * kSnapSizeFieldWidthInFonts);
	ImGui::DragFloat("##SnapSize",&snapSize_,kSnapSizeDragSpeed,kMinSnapSize,FLT_MAX,"%.2f");
	ImGui::EndDisabled();
	ImGui::EndDisabled();
	ImGui::TextDisabled("(左ドラッグ: 移動 / Shift+ドラッグ: 高さ)");
	ImGui::TextDisabled("(何も無い所をクリック: 選択解除)");
	ImGui::TextDisabled("(Del: 削除 / Ctrl+D: 複製 / F: Focus)");

	ImGui::Separator();

	// 的の追加ボタン(カメラの前方に1個生成する)
	if(ImGui::Button("Add Target")){
		AddTargetInFrontOfCamera();
	}
	ImGui::SameLine();

	// 選択中の的の複製・削除ボタン(未選択のときは押せないようにする)
	ImGui::BeginDisabled(!HasSelection());
	if(ImGui::Button("Duplicate")){
		DuplicateSelectedTarget();
	}
	ImGui::SameLine();
	if(ImGui::Button("Delete Target")){
		DeleteSelectedTarget();
	}
	ImGui::EndDisabled();

	ImGui::Separator();

	// JSONへの保存/再読込ボタン
	if(ImGui::Button("Save")){
		SaveToJson();
	}
	ImGui::SameLine();
	if(ImGui::Button("Load")){
		LoadFromJson();
	}
	ImGui::SameLine();
	ImGui::Text("(%s)",kTargetSaveFilePath.c_str());
	EditorWidgets::ShowUnsavedMark(history_.IsModified());

	ImGui::Separator();

	// 現在の配置状況の表示(選択・ドラッグ中かどうかの確認用)
	ImGui::Text("Targets: %d",static_cast<int>(targets_.size()));
	ImGui::Text("Selected: %d",selectedIndex_);
	ImGui::Text("Dragging: %s",isDragging_?"true":"false");

	ImGui::Separator();

	// 選択中の的の座標・壊すと出るレールの編集(Inspectorまで行かなくてもここで直せるようにする)
	DrawSelectedTargetUI();

	ImGui::Separator();

	// 配置されている的の一覧
	DrawTargetList();

	ImGui::End();

	// パネルの外(Sceneビュー上)でのマウスドラッグによる移動処理
	UpdateMouseDrag();
	// 切り替わったときに選んだ的は、掴み終えたので忘れる
	pendingPickIndex_ = -1;

	// キーボードショートカット(Delete・Ctrl+D・F)
	UpdateShortcuts();

	// ImGuiの操作中・ドラッグ中は値が動いている途中のため、操作を終えてから1回の編集として履歴に積む
	// Inspectorなど他の場所からの編集もまとめて拾えるよう、変更があったかは状態を比べて判定する
	if(!ImGui::IsAnyItemActive() && !isDragging_){
		CommitEditIfChanged();
	}
#endif
}

// 現在のカメラの前方に的を1個追加する(Add Targetボタン用)
void TargetEditor::AddTargetInFrontOfCamera(){
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if(!camera){
		// カメラが無い場合は原点に置く
		AddTargetAt({0.0f, 0.0f, 0.0f});
		return;
	}

	// ワールド行列の3行目がカメラの前方ベクトルにあたる
	const Matrix4x4& world = camera->GetWorldMatrix();
	Vector3 forward = Normalize(Vector3{world.m[2][0], world.m[2][1], world.m[2][2]});

	AddTargetAt(camera->GetTranslate() + forward * kSpawnDistance);
}

// 指定座標に的を1個追加する(初回起動時の初期配置生成用)
void TargetEditor::AddTargetAt(const Vector3& position){
	TargetPoint point{};
	point.position = position;
	targets_.push_back(point);

	// 追加した的をそのまま選択状態にして、すぐ動かせるようにする
	selectedIndex_ = static_cast<int>(targets_.size()) - 1;
}

// 選択中の的を、カメラの右方向へ少しずらして複製する(Duplicateボタン・Ctrl+D用)
void TargetEditor::DuplicateSelectedTarget(){
	if(!HasSelection()){
		return;
	}

	// 壊すと出るレールの設定も引き継ぐ(同じレールの鍵の的を並べて置きやすくする)
	TargetPoint copy = targets_[selectedIndex_];

	// 画面上で元の的の真横に出るよう、カメラの右方向へずらす(ワールド行列の1行目がカメラの右方向)
	if(Camera* camera = CameraManager::GetInstance()->GetActiveCamera()){
		const Matrix4x4& world = camera->GetWorldMatrix();
		Vector3 right = Normalize(Vector3{world.m[0][0], world.m[0][1], world.m[0][2]});
		copy.position = copy.position + right * kDuplicateOffsetDistance;
	}

	targets_.push_back(copy);

	// 複製した的を選択して、続けて動かせるようにする
	selectedIndex_ = static_cast<int>(targets_.size()) - 1;
	isDragging_ = false;
}

// 選択中の的を削除する(Delete Targetボタン・Deleteキー用)
void TargetEditor::DeleteSelectedTarget(){
	if(!HasSelection()){
		return;
	}
	targets_.erase(targets_.begin() + selectedIndex_);
	selectedIndex_ = -1;
	isDragging_ = false;
}

// 選択中の的があるか
bool TargetEditor::HasSelection() const{
	return selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(targets_.size());
}

// 選択中の的の座標・壊すと出るレールを編集するUIを表示する
void TargetEditor::DrawSelectedTargetUI(){
#ifdef USE_IMGUI
	// 未選択のときは編集対象が無いことを表示するだけにする
	if(!HasSelection()){
		ImGui::TextDisabled("Select a target");
		return;
	}

	TargetPoint& target = targets_[selectedIndex_];

	ImGui::Text("Target %d",selectedIndex_);
	ImGui::SameLine();
	// 選択中の的に俯瞰デバッグカメラを寄せる
	if(ImGui::SmallButton("Focus")){
		hasFocusRequest_ = true;
	}

	// ドラッグで細かく、ダブルクリックで数値を直接入力できる
	ImGui::DragFloat3("Position",&target.position.x,kPositionDragSpeed);

	// 壊すと出るレール(-1より小さい値は意味が無いため-1に揃える)
	if(ImGui::InputInt("Unlock Rail",&target.unlockRailIndex)){
		if(target.unlockRailIndex < kNoUnlockRailIndex){
			target.unlockRailIndex = kNoUnlockRailIndex;
		}
	}
	ImGui::TextDisabled("(-1で鍵の的ではない。壊すと出るレールのIndexを指定)");

	// 配置データ以外の、的を管理する側の項目(生存フラグなど)
	if(selectedTargetExtraUI_){
		selectedTargetExtraUI_(selectedIndex_);
	}
#endif
}

// 配置されている的の一覧を表示する(クリックで選択、ダブルクリックでFocus)
void TargetEditor::DrawTargetList(){
#ifdef USE_IMGUI
	ImGui::TextDisabled("(クリック: 選択 / ダブルクリック: Focus)");

	// 的が増えても下のUIが押し出されないよう、高さを決めた子ウィンドウの中でスクロールさせる
	ImGui::BeginChild("TargetList",ImVec2(0.0f,kTargetListHeight),ImGuiChildFlags_Borders);
	for(int i = 0; i < static_cast<int>(targets_.size()); ++i){
		ImGui::PushID(i);

		// 行ラベル(鍵の的は出すレールの番号を付ける)
		char label[64];
		if(targets_[i].unlockRailIndex >= 0){
			snprintf(label,sizeof(label),"Target %d (Key: Rail %d)",i,targets_[i].unlockRailIndex);
		} else{
			snprintf(label,sizeof(label),"Target %d",i);
		}

		if(ImGui::Selectable(label,selectedIndex_ == i,ImGuiSelectableFlags_AllowDoubleClick)){
			selectedIndex_ = i;
			// ダブルクリックで、選んだ的に俯瞰デバッグカメラを寄せる
			if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
				hasFocusRequest_ = true;
			}
		}

		// Sceneビューで選んだ的が一覧の見えない所にあっても分かるよう、選択中の行までスクロールする
		if(selectedIndex_ == i && isDragging_){
			ImGui::SetScrollHereY();
		}

		ImGui::PopID();
	}
	ImGui::EndChild();
#endif
}

// Sceneビュー操作中のキーボードショートカット(Delete・Ctrl+D・F)の処理
void TargetEditor::UpdateShortcuts(){
#ifdef USE_IMGUI
	// 配置モードOFFのとき・文字入力中・ImGuiの操作中・ドラッグ中は、意図しない編集にならないよう受け付けない
	const ImGuiIO& io = ImGui::GetIO();
	if(!isPlacementMode_ || !HasSelection() || io.WantTextInput || ImGui::IsAnyItemActive() || isDragging_){
		return;
	}

	if(ImGui::IsKeyPressed(ImGuiKey_Delete,false)){
		DeleteSelectedTarget();
	} else if(ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_D)){
		DuplicateSelectedTarget();
	} else if(!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F,false)){
		hasFocusRequest_ = true;
	}
#endif
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

// 配置されている的の個数を取得
int TargetEditor::GetTargetCount() const{
	return static_cast<int>(targets_.size());
}

// 指定インデックスの的の座標を取得
Vector3 TargetEditor::GetTargetPosition(int index) const{
	if(index < 0 || index >= static_cast<int>(targets_.size())){
		return {0.0f, 0.0f, 0.0f};
	}
	return targets_[index].position;
}

// 指定インデックスの的を壊すと出るレールのインデックスを取得
int TargetEditor::GetUnlockRailIndex(int index) const{
	if(index < 0 || index >= static_cast<int>(targets_.size())){
		return -1;
	}
	return targets_[index].unlockRailIndex;
}

// 選択中の的のインデックスを取得(-1は未選択)
int TargetEditor::GetSelectedIndex() const{
	return selectedIndex_;
}

// 選択中の的のインデックスを設定する(-1で選択解除)
void TargetEditor::SetSelectedIndex(int index){
	if(index < -1 || index >= static_cast<int>(targets_.size())){
		return; // 範囲外は無視
	}
	selectedIndex_ = index;
}

// 選択中の的の編集欄の下に、配置データ以外の項目のUIを足す(引数は選択中の的のインデックス)
void TargetEditor::SetSelectedTargetExtraUI(std::function<void(int)> drawer){
	selectedTargetExtraUI_ = std::move(drawer);
}

// 「Focus」の操作があれば、選択中の的の位置を返す(1回読むと消える)
bool TargetEditor::ConsumeFocusRequest(Vector3& outPosition){
	if(!hasFocusRequest_){
		return false;
	}
	hasFocusRequest_ = false;

	if(!HasSelection()){
		return false;
	}
	outPosition = targets_[selectedIndex_].position;
	return true;
}

// 配置内容をJSONファイルに保存する
void TargetEditor::SaveToJson(){
	// まだ履歴に積んでいない編集があれば先に確定し、保存した状態を保存済みとして記録できるようにする
	CommitEditIfChanged();

	// ルート要素。targetsは読み込み時の簡易フォーマットチェックにも使う
	nlohmann::json root;

	nlohmann::json targetsJson = nlohmann::json::array();
	for(const auto& target : targets_){
		nlohmann::json tj;
		tj["position"] = {target.position.x, target.position.y, target.position.z};
		tj["unlockRailIndex"] = target.unlockRailIndex;
		targetsJson.push_back(tj);
	}
	root["targets"] = targetsJson;

	// 保存先フォルダが無ければ作成しておく
	std::filesystem::path savePath(kTargetSaveFilePath);
	if(savePath.has_parent_path()){
		std::filesystem::create_directories(savePath.parent_path());
	}

	// ファイルへ書き出し(setwで人が読める整形出力にする)
	std::ofstream file(kTargetSaveFilePath);
	if(!file.is_open()){
		return;
	}
	file << std::setw(4) << root << std::endl;
	history_.MarkSaved();
}

// 配置内容をJSONファイルから読み込む(Loadボタン用。ファイルが無ければ何もしない)
// 読み込みも1回の編集として履歴に積み、Ctrl+Zで読み込む前に戻せるようにする
void TargetEditor::LoadFromJson(){
	if(!LoadTargetsFromFile()){
		return;
	}
	CommitEditIfChanged();
	history_.MarkSaved();
}

// 配置内容をJSONファイルから読み込む(ファイルが無い・形式が違うときは現状維持でfalse)
bool TargetEditor::LoadTargetsFromFile(){
	std::ifstream file(kTargetSaveFilePath);
	if(!file.is_open()){
		// ファイルがまだ無い(初回起動など)ときは現状維持
		return false;
	}

	nlohmann::json root;
	file >> root;

	// 最低限のフォーマットチェック。想定外なら読み込みを中止して現状維持
	if(!root.is_object() || !root.contains("targets") || !root["targets"].is_array()){
		return false;
	}

	// 読み込んだ内容で配置を置き換える
	targets_.clear();
	for(const auto& tj : root["targets"]){
		TargetPoint point{};
		point.position.x = tj["position"][0].get<float>();
		point.position.y = tj["position"][1].get<float>();
		point.position.z = tj["position"][2].get<float>();
		// 壊すと出るレール(旧フォーマットのJSONには無いため、value()で既定値-1を補う)
		point.unlockRailIndex = tj.value("unlockRailIndex",-1);
		targets_.push_back(point);
	}

	// 読み込みで的の個数が変わるため、選択とドラッグ状態は解除する
	selectedIndex_ = -1;
	isDragging_ = false;
	return true;
}

// 配置内容が、最後に確定した状態から変わっていれば確定して履歴に積む
void TargetEditor::CommitEditIfChanged(){
	if(targets_ == history_.GetCommitted()){
		return;
	}
	history_.Commit(targets_);
	EditorDocumentManager::GetInstance()->RecordEdit(this);
}

// 1つ前の状態に戻す(Ctrl+Z)
bool TargetEditor::UndoEdit(){
	if(!history_.Undo(targets_)){
		return false;
	}
	// 戻した結果的が減っていれば、選択が範囲外を指さないよう解除する
	if(selectedIndex_ >= static_cast<int>(targets_.size())){
		selectedIndex_ = -1;
	}
	isDragging_ = false;
	return true;
}

// Undoで戻した状態をやり直す(Ctrl+Y)
bool TargetEditor::RedoEdit(){
	if(!history_.Redo(targets_)){
		return false;
	}
	// やり直した結果的が減っていれば、選択が範囲外を指さないよう解除する
	if(selectedIndex_ >= static_cast<int>(targets_.size())){
		selectedIndex_ = -1;
	}
	isDragging_ = false;
	return true;
}
