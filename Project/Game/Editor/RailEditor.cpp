#include "RailEditor.h"
#include "RailEditorInternal.h"
#include "ImGuiManager.h"
#include "imgui.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "EditorWidgets.h"
#include "EditorContext.h"
#include "EditorPicking.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <utility>

// RailEditorのうち、編集の操作(パネルのUI・制御点やレールの追加・ステージやレールの選択・Sceneビューでのドラッグ)をまとめたファイル
// 描画と管のメッシュ作りはRailEditorDraw.cpp、曲線の計算はRailEditorCurve.cpp、保存・読み込みと編集履歴はRailEditorData.cppにある

using namespace RailEditorInternal;

namespace{
	// 制御点の目印に使うモデル(上下に尖った宝石の形。半径1)
	const std::string kControlPointModelPath = "RailMarker/railMarker.obj";

	// 制御点が1つしかなく延長する向きが決まらないときに、末尾から伸ばす量
	constexpr Vector3 kDefaultExtendOffset = {5.0f, 0.0f, 0.0f};
	// 選択中の制御点と次の制御点の間に挿入するときの位置の割合(中間に置く)
	constexpr float kInsertMiddleRate = 0.5f;

	// 螺旋に入る向きが決まらない(前の制御点が無い・真上や真下にある)ときに使う向き
	constexpr Vector3 kDefaultSpiralForward = {1.0f, 0.0f, 0.0f};
	// 螺旋に入る向きとして使える水平方向の長さの下限(これ未満なら向きが決まらないとみなす)
	constexpr float kMinSpiralForwardLength = 1e-4f;
	// 螺旋の設定をImGuiでドラッグして変えるときの変化量
	constexpr float kSpiralDragSpeed = 0.1f;
	constexpr float kSpiralTurnsDragSpeed = 0.05f;
	// 制御点一覧の表示領域の高さ(制御点が増えても、下の編集欄がスクロールで隠れないよう一覧だけをスクロールさせる)
	constexpr float kPointListHeight = 150.0f;

	// 高さを変えずに制御点をドラッグするときの、ドラッグ面の法線(真上)
	constexpr Vector3 kHorizontalDragPlaneNormal = {0.0f, 1.0f, 0.0f};
}

RailEditor::RailEditor() = default;

// 破棄したエディターをCtrl+Zや保存の対象に残さないよう、登録を外す
RailEditor::~RailEditor(){
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

// 制御点の描画用のObj3Dを1つ生成する(Initialize/追加ボタン/ロードで共通利用)
std::unique_ptr<Obj3D> RailEditor::CreatePointObject(const std::string& modelPath){
	auto obj = std::make_unique<Obj3D>();
	obj->Initialize(objCommon_);
	obj->SetModel(modelPath);
	return obj;
}

// rail.pointObjectsの個数をrail.controlPointsの個数にそろえる
void RailEditor::SyncPointObjectsToControlPoints(Rail& rail){
	// 足りなければ生成、多ければ末尾を削る
	while(rail.pointObjects.size() < rail.controlPoints.size()){
		rail.pointObjects.push_back(CreatePointObject(kControlPointModelPath));
	}
	while(rail.pointObjects.size() > rail.controlPoints.size()){
		rail.pointObjects.pop_back();
	}
}

// レールの末尾を延長する位置に置く制御点を求める
// 末尾2点の差をそのまま末尾に足し、同じ向き・同じ間隔で伸ばす。回転と速度は末尾の制御点を引き継ぐ
RailEditor::ControlPoint RailEditor::ComputeExtendedPoint(const std::vector<ControlPoint>& controlPoints) const{
	if(controlPoints.empty()){
		return kDefaultControlPoint;
	}

	ControlPoint point = controlPoints.back();
	// 分岐設定とチェックポイントは延長した制御点には引き継がない
	point.branchTargetRailIndex = kDefaultControlPoint.branchTargetRailIndex;
	point.branchTargetPointIndex = kDefaultControlPoint.branchTargetPointIndex;
	point.isCheckpoint = kDefaultControlPoint.isCheckpoint;

	if(controlPoints.size() < 2){
		point.position = point.position + kDefaultExtendOffset;
		return point;
	}

	const Vector3& last = controlPoints[controlPoints.size() - 1].position;
	const Vector3& prev = controlPoints[controlPoints.size() - 2].position;
	point.position = last + (last - prev);
	return point;
}

// 指定インデックスの位置に制御点を挿入し、その制御点を選択状態にする(続けて位置を調整しやすくするため)
void RailEditor::InsertControlPoint(Rail& rail,size_t insertIndex,const ControlPoint& point){
	rail.controlPoints.insert(rail.controlPoints.begin() + insertIndex,point);
	// 制御点の目印は毎フレーム制御点の座標へ置き直すため、個数だけそろえればよい
	SyncPointObjectsToControlPoints(rail);
	selectedPointIndex_ = static_cast<int>(insertIndex);
}

// 指定した制御点の直後に、その制御点から始まる螺旋の制御点をまとめて挿入する
// 前の制御点から指定した制御点へ進んできた向きのまま螺旋に入るよう、円の中心をその向きの右(左)横に置く
// 高さは角度に比例して上げ、らせん階段のように1周ごとにrisePerTurnだけ上がる
void RailEditor::GenerateSpiral(Rail& rail,size_t startIndex){
	std::vector<ControlPoint>& controlPoints = rail.controlPoints;
	const ControlPoint startPoint = controlPoints[startIndex];

	// 螺旋に入る向き(水平)。前の制御点から指定した制御点への向きを使う
	Vector3 forward = kDefaultSpiralForward;
	if(startIndex > 0){
		Vector3 incoming = startPoint.position - controlPoints[startIndex - 1].position;
		incoming.y = 0.0f;
		if(Length(incoming) >= kMinSpiralForwardLength){
			forward = Normalize(incoming);
		}
	}
	// 進む向きの右横(水平)。右回りならこちら側に、左回りなら反対側に円の中心を置く
	const Vector3 right = {forward.z, 0.0f, -forward.x};
	const float side = spiral_.turnsRight?1.0f:-1.0f;
	const Vector3 center = startPoint.position + right * (side * spiral_.radius);

	// 螺旋の上に置く制御点の数
	const int spiralPointCount = (std::max)(1,static_cast<int>(std::lround(spiral_.turns * static_cast<float>(spiral_.pointsPerTurn))));
	// 末尾に足すときは、最後の制御点は曲線の向きを決めるだけで曲線が通らないため、螺旋の終わりまで曲線が通るよう1つ先にもう1つ足す
	const bool isAppending = (startIndex + 1 == controlPoints.size());
	const int generateCount = spiralPointCount + (isAppending?1:0);

	std::vector<ControlPoint> spiralPoints;
	spiralPoints.reserve(static_cast<size_t>(generateCount));
	for(int i = 1; i <= generateCount; ++i){
		const float turnRate = static_cast<float>(i) / static_cast<float>(spiral_.pointsPerTurn);
		const float angle = kTwoPi * turnRate;

		// 角度0で指定した制御点に重なり、進む向きへ出発して中心の周りを回る
		ControlPoint point = startPoint;
		point.position = center - right * (side * spiral_.radius * std::cos(angle)) + forward * (spiral_.radius * std::sin(angle));
		point.position.y = startPoint.position.y + spiral_.risePerTurn * turnRate;
		// 分岐設定とチェックポイントは引き継がない(速度・回転は指定した制御点のものを引き継ぐ)
		point.branchTargetRailIndex = kDefaultControlPoint.branchTargetRailIndex;
		point.branchTargetPointIndex = kDefaultControlPoint.branchTargetPointIndex;
		point.isCheckpoint = kDefaultControlPoint.isCheckpoint;
		spiralPoints.push_back(point);
	}

	controlPoints.insert(controlPoints.begin() + startIndex + 1,spiralPoints.begin(),spiralPoints.end());
	// 制御点の目印は毎フレーム制御点の座標へ置き直すため、個数だけそろえればよい
	SyncPointObjectsToControlPoints(rail);
	// 螺旋の終わりの制御点を選び、続けて延長や調整をしやすくする
	selectedPointIndex_ = static_cast<int>(startIndex) + spiralPointCount;
}

// 新しいレールを1本追加し、アクティブなレールとして切り替える
void RailEditor::AddRail(){
	AppendRail(rails_);
	activeRailIndex_ = static_cast<int>(rails_.size()) - 1;
}

// railsの末尾に、制御点1つだけの新しいレールを足す
void RailEditor::AppendRail(std::vector<Rail>& rails){
	Rail rail;
	rail.name = "Rail " + std::to_string(rails.size());
	rail.controlPoints.push_back(kDefaultControlPoint);
	rail.pointObjects.push_back(CreatePointObject(kControlPointModelPath));

	// 曲線・着地できる範囲の管はEdit中の描画でUpdateEditTubes()、レール本体の管はPlayに入ったときにBuildTubes()でレールの形から作るため、
	// ここでは制御点の目印1つだけを作る

	rails.push_back(std::move(rail));
}

// 指定ステージのレールの配列を取得(編集中のステージはrails_、それ以外は預けてある配列)
std::vector<RailEditor::Rail>& RailEditor::GetStageRails(int stageIndex){
	return (stageIndex == stageIndex_)?rails_:storedStageRails_[stageIndex];
}
const std::vector<RailEditor::Rail>& RailEditor::GetStageRails(int stageIndex) const{
	return (stageIndex == stageIndex_)?rails_:storedStageRails_[stageIndex];
}

// 編集・プレイするステージを切り替える
void RailEditor::SetStageIndex(int stageIndex){
	if(!StageInfo::IsValidStageIndex(stageIndex) || stageIndex == stageIndex_){
		return;
	}

	// 今のステージのレールを預け、切り替えた先のステージのレールを取り出す
	std::swap(rails_,storedStageRails_[stageIndex_]);
	std::swap(rails_,storedStageRails_[stageIndex]);
	stageIndex_ = stageIndex;

	// 前のステージのレール・制御点を指したままにならないよう、0番のレールにして選択を解除する
	activeRailIndex_ = 0;
	selectedPointIndex_ = -1;
	isDragging_ = false;
	// 切り替えた先のステージのレールの管を、次の描画で作り直す
	areTubesBuilt_ = false;
}

// 一覧でステージが選び直されていれば、そのステージの番号を返す(1回読むと消える)
bool RailEditor::ConsumeStageChangeRequest(int& outStageIndex){
	if(requestedStageIndex_ == kNoStageRequest){
		return false;
	}
	outStageIndex = requestedStageIndex_;
	requestedStageIndex_ = kNoStageRequest;
	return true;
}

// エディターの初期化処理
void RailEditor::Initialize(Obj3dCommon* objCommon){
	objCommon_ = objCommon;

	// 制御点の目印のモデルをロード
	ModelManager::GetInstance()->LoadModel(kControlPointModelPath);

	// 前のシーンで編集・プレイしていたステージから始める
	stageIndex_ = StageInfo::IsValidStageIndex(StageInfo::currentStageIndex)?StageInfo::currentStageIndex:0;

	// 全ステージのレールを、保存済みのJSONから読み込む(ファイルが無いステージは制御点1つだけのレールを1本用意する)
	LoadRailsFromFiles();

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

// エディターの更新処理
void RailEditor::Update(){
#ifdef USE_IMGUI
	// Playモード中はレール編集UIを出さない
	if(EditorContext::GetInstance()->IsPlayMode()){ return; }

	// 固定タイルレイアウトの左の列に縦いっぱいで配置
	EditorWidgets::BeginFixedPanel("Rail Editor",EditorWidgets::ComputeLayout().railEditor);

	// ステージの切り替えUI(ステージ → レール → 制御点の順に選ぶ)
	// 的・敵のエディターも同じステージに合わせるため、ここでは選んだことだけを伝え、切り替えはGameScene側で行う
	ImGui::Text("Stage");
	ImGui::SetNextItemWidth(-FLT_MIN);
	if(ImGui::BeginCombo("##Stage",StageInfo::GetStageLabel(stageIndex_))){
		for(int i = 0; i < StageInfo::kStageCount; ++i){
			if(ImGui::Selectable(StageInfo::GetStageLabel(i),i == stageIndex_) && i != stageIndex_){
				requestedStageIndex_ = i;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::Separator();

	// レールの切り替え・新規作成UI
	ImGui::Text("Active Rail: %s",rails_[activeRailIndex_].name.c_str());
	for(size_t i = 0; i < rails_.size(); ++i){
		ImGui::PushID(static_cast<int>(i) + 10000); // 制御点側のPushIDと衝突しないようオフセット
		// ゴールのレールは一覧に (Goal) と出す
		const std::string railLabel = rails_[i].isGoalRail?rails_[i].name + " (Goal)":rails_[i].name;
		if(ImGui::Selectable(railLabel.c_str(),static_cast<int>(i) == activeRailIndex_)){
			SwitchActiveRail(static_cast<int>(i));
		}
		ImGui::PopID();
	}
	if(ImGui::Button("New Rail")){
		AddRail();
	}

	ImGui::Separator();

	// 以降はアクティブなレールに対する編集操作
	Rail& activeRail = rails_[activeRailIndex_];

	// ゴールのレールにするか(終点で止まり、乗るとゴールの的が現れる。ステージに1本だけ置く想定)
	ImGui::Checkbox("Goal Rail",&activeRail.isGoalRail);

	// 制御点の追加ボタン(レールを延長しやすいよう、原点ではなく末尾2点の向きにそのまま伸ばした位置へ追加する)
	if(ImGui::Button("Add Control Point")){
		InsertControlPoint(activeRail,activeRail.controlPoints.size(),ComputeExtendedPoint(activeRail.controlPoints));
	}
	ImGui::TextDisabled("(末尾の2点と同じ向き・間隔で延長する)");

	ImGui::Separator();

	// JSONへの保存/読み込みボタン(ImGuiで編集した値をそのまま反映させる)
	if(ImGui::Button("Save")){
		SaveToJson();
	}
	ImGui::SameLine();
	if(ImGui::Button("Load")){
		LoadFromJson();
	}
	ImGui::SameLine();
	ImGui::Text("(%s)",GetRailFilePath(stageIndex_,activeRailIndex_).c_str());
	ImGui::TextDisabled("(Save / Load は全ステージまとめて行う)");
	EditorWidgets::ShowUnsavedMark(history_.IsModified());

	ImGui::Separator();

	// 制御点モデルの表示ON/OFF切り替え(他のオブジェクトが見づらくなるとき用)
	ImGui::Checkbox("Show Control Point Models",&activeRail.showControlPointModels);
	// レール曲線(サンプリング点列)の表示ON/OFF切り替え
	ImGui::Checkbox("Show Rail Curve",&activeRail.showCurve);
	// ジャンプしてレールに着地できる範囲の表示ON/OFF切り替え(全レールまとめて切り替わる)
	ImGui::Checkbox("Show Landing Range",&showLandingRange_);
	ImGui::TextDisabled("(着地できる水平方向の許容距離: %.2f)",landingRangeRadius_);

	// Sceneビュー上で制御点を左ドラッグして動かす設定
	ImGui::Checkbox("Drag Points In Scene",&isPointDragMode_);
	ImGui::BeginDisabled(!isPointDragMode_);
	ImGui::Checkbox("Keep Height",&keepsHeightWhileDragging_);
	ImGui::EndDisabled();
	ImGui::TextDisabled("(Sceneビューで左ドラッグして移動)");
	// 的・敵の配置エディターも同じ左ドラッグで掴むため、重なった物を一緒に掴まないよう注意を出す
	ImGui::TextDisabled("(的・敵のPlacement Modeと同時ONに注意)");

	ImGui::Separator();

	// レールの全長と走り切るまでの秒数(ステージの流れを組むときの目安)
	const RailStats stats = ComputeActiveRailStats();
	ImGui::Text("Length: %.1f  Time: %.1fs",stats.length,stats.time);
	// 発射直後の加速・空中で遅くなる分は含まない
	ImGui::TextDisabled("(railSpeed %.2f での目安)",baseRailSpeed_);

	ImGui::Separator();

	// 選択中の制御点インデックスが範囲外になっていたら選択解除する(削除・レール切り替え後など)
	if(selectedPointIndex_ >= static_cast<int>(activeRail.controlPoints.size())){
		selectedPointIndex_ = -1;
	}

	// 削除ボタンが押された制御点のインデックス(未押下なら-1のまま)
	int deleteIndex = -1;

	// 制御点一覧(Hierarchy的な一覧表示)。クリックした行を選択状態にする
	// 一覧は高さを決めた子ウィンドウの中だけでスクロールさせ、制御点が増えても下の編集欄が見えるようにする
	ImGui::Text("Control Points: %zu",activeRail.controlPoints.size());
	ImGui::TextDisabled("(ダブルクリックでカメラを寄せる)");
	ImGui::BeginChild("ControlPointList",ImVec2(0.0f,kPointListHeight),true);
	for(size_t i = 0; i < activeRail.controlPoints.size(); ++i){
		// IDの重複を防ぐためPushIDを使用
		ImGui::PushID(static_cast<int>(i));

		// 分岐先が設定されている制御点と、チェックポイントにした制御点には目印を付ける
		bool hasBranch = (activeRail.controlPoints[i].branchTargetRailIndex >= 0);
		bool isCheckpoint = activeRail.controlPoints[i].isCheckpoint;
		char label[48];
		snprintf(label,sizeof(label),"Point %zu%s%s",i,hasBranch?" (Branch)":"",isCheckpoint?" (Checkpoint)":"");

		// クリックで選択。選択中の行はハイライトされる
		if(ImGui::Selectable(label,selectedPointIndex_ == static_cast<int>(i))){
			selectedPointIndex_ = static_cast<int>(i);
		}
		// ダブルクリックした制御点にカメラを寄せる(1回目のクリックで選択済みのため、選択中の制御点に寄せればよい)
		if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
			hasFocusRequest_ = true;
		}
		ImGui::PopID();
	}
	ImGui::EndChild();

	ImGui::Separator();

	// 選択中の制御点のみ編集UIを表示する(未選択時は何も編集できない)
	if(selectedPointIndex_ >= 0 && selectedPointIndex_ < static_cast<int>(activeRail.controlPoints.size())){
		ImGui::PushID(selectedPointIndex_);
		ControlPoint& selectedPoint = activeRail.controlPoints[selectedPointIndex_];
		ImGui::Text("Point %d",selectedPointIndex_);
		// レールの始点からこの制御点までの長さと秒数(敵や的をどのあたりに置くか決める目安)
		ImGui::Text("From Start: %.1f  %.1fs",stats.lengthToSelected,stats.timeToSelected);

		// 座標、回転、速度の編集UI
		ImGui::DragFloat3("Position",&selectedPoint.position.x,0.1f);
		ImGui::DragFloat3("Rotation",&selectedPoint.rotation.x,0.1f);
		ImGui::DragFloat("Speed",&selectedPoint.speed,0.1f);

		// レール間分岐移動用の設定欄(-1で分岐なし)
		ImGui::InputInt("Branch Target Rail",&selectedPoint.branchTargetRailIndex);
		ImGui::InputInt("Branch Target Point",&selectedPoint.branchTargetPointIndex);
		ImGui::TextDisabled("(-1で分岐なし。分岐先レールIndexと対応する制御点Indexを指定)");

		// チェックポイントの設定欄(通った後にレールから落ちたら、この制御点の位置からやり直す)
		ImGui::Checkbox("Checkpoint",&selectedPoint.isCheckpoint);

		// 制御点のパラメータをデフォルト値に戻すボタン
		if(ImGui::Button("Reset")){
			selectedPoint = kDefaultControlPoint;
		}
		ImGui::SameLine();

		// 制御点の削除ボタン(最低1点は残す必要があるため、1点しかないときは押せないようにする)
		bool isOnlyPoint = (activeRail.controlPoints.size() <= 1);
		ImGui::BeginDisabled(isOnlyPoint);
		if(ImGui::Button("Delete")){
			deleteIndex = selectedPointIndex_;
		}
		ImGui::EndDisabled();

		// 選択中の制御点の直後に制御点を挿入するボタン(曲がりを細かく作りたい区間に点を足す用)
		// 次の制御点があればその中間、末尾ならAdd Control Pointと同じく延長した位置に置く
		if(ImGui::Button("Insert After")){
			const size_t insertIndex = static_cast<size_t>(selectedPointIndex_) + 1;
			ControlPoint insertPoint;
			if(insertIndex < activeRail.controlPoints.size()){
				const ControlPoint& next = activeRail.controlPoints[insertIndex];
				insertPoint = kDefaultControlPoint;
				insertPoint.position = Lerp(selectedPoint.position,next.position,kInsertMiddleRate);
				insertPoint.rotation = Lerp(selectedPoint.rotation,next.rotation,kInsertMiddleRate);
				insertPoint.speed = Lerp(selectedPoint.speed,next.speed,kInsertMiddleRate);
			} else{
				insertPoint = ComputeExtendedPoint(activeRail.controlPoints);
			}
			// 挿入でcontrolPointsが再確保されるとselectedPointの参照が無効になるため、ここ以降は使わない
			InsertControlPoint(activeRail,insertIndex,insertPoint);
		}
		ImGui::SameLine();

		// 選択中の制御点にカメラを寄せるボタン(俯瞰デバッグカメラに切り替えて、今の向きのまま近づける)
		if(ImGui::Button("Focus")){
			hasFocusRequest_ = true;
		}

		// 選択中の制御点から螺旋(らせん階段のような形)を作る
		// 制御点を1つずつ置くと手間がかかるため、半径・1周で上がる高さ・周回数を決めてまとめて作る(Ctrl+Zで戻せる)
		if(ImGui::CollapsingHeader("Spiral")){
			ImGui::DragFloat("Radius",&spiral_.radius,kSpiralDragSpeed,kMinSpiralRadius,FLT_MAX);
			ImGui::DragFloat("Rise / Turn",&spiral_.risePerTurn,kSpiralDragSpeed);
			ImGui::DragFloat("Turns",&spiral_.turns,kSpiralTurnsDragSpeed,kMinSpiralTurns,FLT_MAX);
			ImGui::SliderInt("Points / Turn",&spiral_.pointsPerTurn,kMinSpiralPointsPerTurn,kMaxSpiralPointsPerTurn);
			if(ImGui::RadioButton("Right",spiral_.turnsRight)){
				spiral_.turnsRight = true;
			}
			ImGui::SameLine();
			if(ImGui::RadioButton("Left",!spiral_.turnsRight)){
				spiral_.turnsRight = false;
			}

			// 手入力で範囲の外にされても、形が崩れないよう制限する
			spiral_.radius = (std::max)(spiral_.radius,kMinSpiralRadius);
			spiral_.turns = (std::max)(spiral_.turns,kMinSpiralTurns);
			spiral_.pointsPerTurn = std::clamp(spiral_.pointsPerTurn,kMinSpiralPointsPerTurn,kMaxSpiralPointsPerTurn);

			ImGui::TextDisabled("(この点の後ろに挿入。後ろの点は残る)");
			if(ImGui::Button("Generate Spiral")){
				// 挿入でcontrolPointsが再確保されるとselectedPointの参照が無効になるため、ここ以降は使わない
				GenerateSpiral(activeRail,static_cast<size_t>(selectedPointIndex_));
			}
		}

		ImGui::PopID();
	} else{
		ImGui::TextDisabled("Select a point above");
	}

	// 削除処理(一覧選択の状態も合わせて解除する)
	if(deleteIndex >= 0){
		activeRail.controlPoints.erase(activeRail.controlPoints.begin() + deleteIndex);
		// pointObjectsの個数もcontrolPointsに合わせる(以降Draw()でインデックスがずれないようにする)
		SyncPointObjectsToControlPoints(activeRail);
		selectedPointIndex_ = -1;
	}

	ImGui::End();

	// パネルの外(Sceneビュー上)でのマウスドラッグによる制御点の移動
	UpdateMouseDrag();

	// ImGuiの操作中・ドラッグ中は値が動いている途中のため、操作を終えてから1回の編集として履歴に積む
	// Inspectorなど他の場所からの編集もまとめて拾えるよう、変更があったかは状態を比べて判定する
	if(!ImGui::IsAnyItemActive() && !isDragging_){
		CommitEditIfChanged();
	}
#endif
}

// 指定インデックスのレールをアクティブなレールとして切り替える(Playモードでのレール乗り換えにも使用)
void RailEditor::SwitchActiveRail(int index){
	if(index < 0 || index >= static_cast<int>(rails_.size())){
		return; // 範囲外は無視
	}
	activeRailIndex_ = index;

	// レールが変わったら、別レールの制御点を指したままにならないよう選択を解除する
	selectedPointIndex_ = -1;
}

// 指定レールをアクティブにして、指定した制御点を選択する(範囲外の制御点を指定したときは未選択にする)
void RailEditor::SelectPoint(int railIndex,int pointIndex){
	SwitchActiveRail(railIndex);
	if(pointIndex >= 0 && pointIndex < static_cast<int>(rails_[activeRailIndex_].controlPoints.size())){
		selectedPointIndex_ = pointIndex;
	}
}

// 「Focus」の操作があれば、選択中の制御点の位置を返す(1回読むと消える)
bool RailEditor::ConsumeFocusRequest(Vector3& outPosition){
	if(!hasFocusRequest_){
		return false;
	}
	hasFocusRequest_ = false;

	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;
	if(selectedPointIndex_ < 0 || selectedPointIndex_ >= static_cast<int>(controlPoints.size())){
		return false;
	}
	outPosition = controlPoints[selectedPointIndex_].position;
	return true;
}

// 現在アクティブなレールのインデックスを取得(デバッグ表示用)
int RailEditor::GetActiveRailIndex() const{
	return activeRailIndex_;
}

// 読み込まれている全レールの本数を取得(デバッグ表示用)
int RailEditor::GetRailCount() const{
	return static_cast<int>(rails_.size());
}

// 指定ステージのレールの本数を取得
int RailEditor::GetStageRailCount(int stageIndex) const{
	if(!StageInfo::IsValidStageIndex(stageIndex)){
		return 0;
	}
	return static_cast<int>(GetStageRails(stageIndex).size());
}

// 今のステージの指定したレールがゴールのレールか(範囲外のレールはゴールのレールではないものとする)
bool RailEditor::IsGoalRail(int railIndex) const{
	if(railIndex < 0 || railIndex >= static_cast<int>(rails_.size())){
		return false;
	}
	return rails_[railIndex].isGoalRail;
}

// ImGui非表示時でもキー操作で表示切り替えを行うための関数(対象はアクティブなレール)
void RailEditor::ToggleShowControlPointModels(){
	rails_[activeRailIndex_].showControlPointModels = !rails_[activeRailIndex_].showControlPointModels;
}
void RailEditor::ToggleShowCurve(){
	rails_[activeRailIndex_].showCurve = !rails_[activeRailIndex_].showCurve;
}

// Sceneビュー上で制御点を左ドラッグして動かす処理
// 全レールの制御点から掴む点を探し、別のレールの点を掴んだときはそのレールをアクティブにする
void RailEditor::UpdateMouseDrag(){
#ifdef USE_IMGUI
	// ドラッグの設定がOFF、またはImGuiのウィジェットを操作中のときはドラッグ判定を行わない
	if(!isPointDragMode_ || ImGui::IsAnyItemActive()){
		isDragging_ = false;
		return;
	}

	// 左ボタンが離されていればドラッグ終了
	if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)){
		isDragging_ = false;
	}

	EditorPicking::PickRay ray{};
	if(!EditorPicking::ComputeMouseRay(ray)){
		return;
	}

	// 左ボタンを押した瞬間、レイの近くに制御点があれば掴む
	if(!isDragging_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
		int hitRailIndex = -1;
		int hitPointIndex = -1;
		float bestDistanceAlongRay = 0.0f;

		for(size_t railIndex = 0; railIndex < rails_.size(); ++railIndex){
			// 目印を隠しているレールの制御点は、見えないまま掴まないよう対象外にする
			if(!rails_[railIndex].showControlPointModels){
				continue;
			}

			const std::vector<ControlPoint>& controlPoints = rails_[railIndex].controlPoints;
			for(size_t pointIndex = 0; pointIndex < controlPoints.size(); ++pointIndex){
				const Vector3& pointPos = controlPoints[pointIndex].position;

				// レイ方向に対する射影距離。負ならカメラの後方にあるので対象外
				float distanceAlongRay = Dot(pointPos - ray.origin,ray.direction);
				if(distanceAlongRay < 0.0f){
					continue;
				}

				// レイ上の最近傍点と制御点との距離が掴める範囲に収まっているかを判定する
				Vector3 nearestOnRay = ray.origin + ray.direction * distanceAlongRay;
				if(Distance(nearestOnRay,pointPos) > kPickRadius){
					continue;
				}

				// 複数の制御点が重なって見えている場合は、手前にあるものを優先する
				if(hitRailIndex < 0 || distanceAlongRay < bestDistanceAlongRay){
					hitRailIndex = static_cast<int>(railIndex);
					hitPointIndex = static_cast<int>(pointIndex);
					bestDistanceAlongRay = distanceAlongRay;
				}
			}
		}

		if(hitRailIndex >= 0){
			if(hitRailIndex != activeRailIndex_){
				SwitchActiveRail(hitRailIndex);
			}
			selectedPointIndex_ = hitPointIndex;
			isDragging_ = true;

			// 高さを変えないときは水平な面、変えるときは掴んだ瞬間のレイ方向を法線とする面(画面に平行な面)の上で動かす
			const Vector3& pointPos = rails_[hitRailIndex].controlPoints[hitPointIndex].position;
			dragPlaneNormal_ = keepsHeightWhileDragging_?kHorizontalDragPlaneNormal:ray.direction;
			dragPlanePoint_ = pointPos;

			// 掴んだ位置と制御点の座標とのずれを保持し、掴んだ瞬間に制御点が跳ばないようにする
			Vector3 hitPos{};
			if(EditorPicking::IntersectRayPlane(ray,dragPlanePoint_,dragPlaneNormal_,hitPos)){
				dragOffset_ = pointPos - hitPos;
			} else{
				dragOffset_ = {0.0f, 0.0f, 0.0f};
			}
		}
	}

	// ドラッグ中は、掴んだときの面とレイの交点へ制御点を移動させる
	std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;
	if(isDragging_ && selectedPointIndex_ >= 0 && selectedPointIndex_ < static_cast<int>(controlPoints.size())){
		Vector3 hitPos{};
		if(EditorPicking::IntersectRayPlane(ray,dragPlanePoint_,dragPlaneNormal_,hitPos)){
			controlPoints[selectedPointIndex_].position = hitPos + dragOffset_;
		}
	}
#endif
}
