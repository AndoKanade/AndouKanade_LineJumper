#include "TargetEditor.h"
#include "ImGuiManager.h"
#include "imgui.h"
#include "CameraManager.h"
#include "Camera.h"
#include "EditorWidgets.h"
#include "EditorContext.h"
#include <cfloat>
#include <cstdio>
#include <string>

// TargetEditorのうち、パネルのUIと的の追加・複製・削除、選択中の的の取得・設定をまとめたファイル
// Sceneビューでのマウス操作はTargetEditorPicking.cpp、保存・読み込みと編集履歴はTargetEditorData.cppにある

TargetEditor::TargetEditor() = default;
// 破棄したエディターをCtrl+Zや保存の対象に残さないよう、登録を外す
TargetEditor::~TargetEditor(){
	// 保存していない変更があれば、次に作ったエディターで戻せるよう取っておく(変更が無ければ前に取っておいたものも消す)
	// 操作の途中でまだ履歴に積んでいない変更も含めるため、先に確定してから判定する
	CommitEditIfChanged();
	if(history_.IsModified()){
		unsavedStash_ = MakeSnapshot();
	} else{
		unsavedStash_.reset();
	}

	EditorDocumentManager::GetInstance()->Unregister(this);
}

// 初期化処理(保存済みJSONがあれば読み込む)
void TargetEditor::Initialize(){
	// 前のシーンで編集・プレイしていたステージから始める
	stageIndex_ = StageInfo::IsValidStageIndex(StageInfo::currentStageIndex)?StageInfo::currentStageIndex:0;

	// 全ステージの配置を、保存済みのJSONから読み込む
	for(int stage = 0; stage < StageInfo::kStageCount; ++stage){
		LoadTargetsFromFile(stage);
	}

	// 読み込んだ状態を編集の基準(保存済み)にし、Ctrl+Z・すべて保存の対象に登録する
	history_.Reset(MakeSnapshot());
	EditorDocumentManager::GetInstance()->Register(this);

	// 前のシーンで保存せずに残していた変更があれば戻す
	// 読み込んだファイルの状態からの1回の編集として積み、未保存のままにする(Ctrl+Zで読み込んだファイルの状態に戻せる)
	if(unsavedStash_){
		ApplySnapshot(*unsavedStash_);
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
	ImGui::Text("(%s)",GetSaveFilePath(stageIndex_).c_str());
	ImGui::TextDisabled("(Save / Load は全ステージまとめて行う)");
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
