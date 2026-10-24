#include "RailEditor.h"
#include "ImGuiManager.h"
#include "imgui.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "EditorWidgets.h"
#include "EditorContext.h"
#include "EditorPicking.h"
#include "Easing.h"
#include "externals/json.hpp"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iomanip>

namespace{
	// 制御点データの保存先ディレクトリ・拡張子(LevelManagerと同じくresource配下の相対パス)
	const std::string kRailSaveDir = "resource/rail/";
	// 0番目のレールのみ、複数化対応前から存在するファイルパスと互換性を保つ
	const std::string kFirstRailSaveFilePath = "resource/rail/rail.json";
	// 制御点リセット用のデフォルト値(初期制御点/Add Control Pointボタンと同じ初期値)
	constexpr RailEditor::ControlPoint kDefaultControlPoint = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f};

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
	// レールの始点になる制御点のインデックス(0番目は曲線の向きを決めるだけで、曲線は1番目から始まる)
	// 終点も同じく、最後の制御点から同じ数だけ手前の点になる
	constexpr size_t kRailStartPointIndex = 1;

	// 着地できる範囲を示す点列の色(レール本体と区別できるよう緑にする)
	constexpr Vector4 kLandingRangeColor = {0.2f, 1.0f, 0.4f, 1.0f};

	// 制御点の目印に使うモデル(上下に尖った宝石の形。半径1)
	const std::string kControlPointModelPath = "RailMarker/railMarker.obj";
	// 曲線の点列・着地できる範囲の点に使うモデル(半径1の球)
	const std::string kSampleModelPath = "Sphere/sphere.obj";

	// 制御点の目印の表示スケール
	constexpr float kControlPointScale = 0.2f;
	// 選択中の制御点の目印の表示スケール(どの点を編集しているか一目で分かるよう、他の制御点より大きくする)
	constexpr float kSelectedPointScale = 0.35f;
	// レール曲線のサンプリング点の表示スケール(線のように見せるため制御点より小さくする)
	constexpr float kCurveScale = 0.04f;
	// 着地できる範囲のサンプリング点の表示スケール
	constexpr float kLandingRangeScale = 0.06f;

	// 進行方向を前後の座標の差分で求めるときに使う、進行度tの刻み幅
	constexpr float kForwardSampleDelta = 0.01f;

	// 制御点が1つしかなく延長する向きが決まらないときに、末尾から伸ばす量
	constexpr Vector3 kDefaultExtendOffset = {5.0f, 0.0f, 0.0f};
	// 選択中の制御点と次の制御点の間に挿入するときの位置の割合(中間に置く)
	constexpr float kInsertMiddleRate = 0.5f;

	// 1周の角度(ラジアン)
	constexpr float kTwoPi = 3.14159265f * 2.0f;
	// 螺旋に入る向きが決まらない(前の制御点が無い・真上や真下にある)ときに使う向き
	constexpr Vector3 kDefaultSpiralForward = {1.0f, 0.0f, 0.0f};
	// 螺旋に入る向きとして使える水平方向の長さの下限(これ未満なら向きが決まらないとみなす)
	constexpr float kMinSpiralForwardLength = 1e-4f;
	// 螺旋の設定をImGuiでドラッグして変えるときの変化量
	constexpr float kSpiralDragSpeed = 0.1f;
	constexpr float kSpiralTurnsDragSpeed = 0.05f;
	// 制御点一覧の表示領域の高さ(制御点が増えても、下の編集欄がスクロールで隠れないよう一覧だけをスクロールさせる)
	constexpr float kPointListHeight = 150.0f;
	// Catmull-Romの区間数(制御点数から引く数。始点と終点の制御点は曲線の向きを決めるだけで、曲線は通らないため)
	constexpr size_t kCatmullRomExtraPointCount = 3;

	// 高さを変えずに制御点をドラッグするときの、ドラッグ面の法線(真上)
	constexpr Vector3 kHorizontalDragPlaneNormal = {0.0f, 1.0f, 0.0f};
	// 走り切るまでの秒数を求めるとき、細かく分けた1区間の中で速さを読む位置の割合(中央で読む)
	constexpr float kStatsSpeedSampleRate = 0.5f;
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

// 制御点・曲線の点列・着地範囲の描画用のObj3Dを1つ生成する(Initialize/追加ボタン/ロードで共通利用)
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

// 描画用オブジェクトがcount個に満たなければ、足りない分だけ生成する
// 曲線の点列はレールの区間数で必要な数が変わるため、延長して足りなくなったときだけ増やす
void RailEditor::EnsureObjectCount(std::vector<std::unique_ptr<Obj3D>>& objects,size_t count,const std::string& modelPath){
	if(objects.size() >= count){
		return;
	}
	objects.reserve(count);
	while(objects.size() < count){
		objects.push_back(CreatePointObject(modelPath));
	}
}

// 制御点数pointCountのレールで、Edit中の曲線の点列に使う点の数を求める
// 区間数に比例した数の点を並べ、レールを延長しても点の間隔が粗くならないようにする(終点の分だけ1つ多い)
size_t RailEditor::GetCurveSampleCount(size_t pointCount) const{
	// Catmull-Romは制御点4つ以上必要なので、それ未満のときは曲線を描かない
	if(pointCount < 4){
		return 0;
	}
	return (pointCount - kCatmullRomExtraPointCount) * kCurveSamplesPerSegment + 1;
}

// Edit中の曲線の点列・着地できる範囲の点を描くオブジェクトを、全レール共通の置き場から1つ取り出す
// 置き場の先頭から順に使い、足りなければ増やす。New Railや制御点の追加で一度にたくさん作ると画面が止まるため、
// 1フレームに作る数を制限し、作りきれなかった点は描かずに次のフレーム以降に回す(数フレームで全部描かれる)
Obj3D* RailEditor::AcquireSampleObject(){
	if(usedSampleObjectCount_ >= sampleObjects_.size()){
		if(createdSampleObjectCount_ >= kMaxSampleObjectCreatePerFrame){
			return nullptr;
		}
		sampleObjects_.push_back(CreatePointObject(kSampleModelPath));
		++createdSampleObjectCount_;
	}
	return sampleObjects_[usedSampleObjectCount_++].get();
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
	Rail rail;
	rail.name = "Rail " + std::to_string(rails_.size());
	rail.controlPoints.push_back(kDefaultControlPoint);
	rail.pointObjects.push_back(CreatePointObject(kControlPointModelPath));

	// 曲線の点列・着地できる範囲の点は全レール共通の置き場(sampleObjects_)から使い、
	// 管はPlayに入ったときにBuildTubes()でレールの形から作る
	// (New Railで一度にたくさん作ると画面が止まるため、ここでは制御点の目印1つだけを作る)

	rails_.push_back(std::move(rail));
	activeRailIndex_ = static_cast<int>(rails_.size()) - 1;
}

// レールのインデックスから保存先ファイルパスを求める(0番目のみ既存の互換パスを使う)
std::string RailEditor::GetRailFilePath(int index) const{
	if(index <= 0){
		return kFirstRailSaveFilePath;
	}
	return kRailSaveDir + "rail" + std::to_string(index) + ".json";
}

// エディターの初期化処理
void RailEditor::Initialize(Obj3dCommon* objCommon){
	objCommon_ = objCommon;

	// 制御点の目印と、曲線の点列・着地範囲の点のモデルをロード
	ModelManager::GetInstance()->LoadModel(kControlPointModelPath);
	ModelManager::GetInstance()->LoadModel(kSampleModelPath);

	// 最初のレールを1本用意する
	AddRail();

	// 保存済みのJSONがあれば読み込んで初期状態を上書きする(ホットロードの起動時オート読込)
	LoadRailsFromFiles();

	// 編集中に描画用オブジェクトを作って画面が止まらないよう、読み込んだレールに必要な分を起動時に作っておく
	// 曲線の点列は延長に備えて多めに作る
	size_t sampleObjectCount = kSampleObjectHeadroom;
	for(const Rail& rail : rails_){
		sampleObjectCount += GetCurveSampleCount(rail.controlPoints.size());
	}
	EnsureObjectCount(sampleObjects_,sampleObjectCount,kSampleModelPath);

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

	// 固定タイルレイアウトの左・下段に配置
	EditorWidgets::BeginFixedPanel("Rail Editor",EditorWidgets::ComputeLayout().railEditor);

	// レールの切り替え・新規作成UI
	ImGui::Text("Active Rail: %s",rails_[activeRailIndex_].name.c_str());
	for(size_t i = 0; i < rails_.size(); ++i){
		ImGui::PushID(static_cast<int>(i) + 10000); // 制御点側のPushIDと衝突しないようオフセット
		if(ImGui::Selectable(rails_[i].name.c_str(),static_cast<int>(i) == activeRailIndex_)){
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
	ImGui::Text("(%s)",GetRailFilePath(activeRailIndex_).c_str());
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

	// 曲線の点列・着地できる範囲の点のオブジェクトは、毎フレーム置き場の先頭から使い直す
	usedSampleObjectCount_ = 0;
	createdSampleObjectCount_ = 0;

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

		// レール曲線をサンプリング点列(小さい球)で可視化する簡易版
		// Catmull-Romは制御点4つ以上必要なので、それ未満のときは何も描画しない
		if(rail.showCurve && rail.controlPoints.size() >= 4){
			const size_t curveSampleCount = GetCurveSampleCount(rail.controlPoints.size());
			for(size_t i = 0; i < curveSampleCount; ++i){
				// オブジェクトを作りきれていなければ、残りの点は次のフレーム以降に描く
				Obj3D* curveObject = AcquireSampleObject();
				if(!curveObject){
					break;
				}

				float t = static_cast<float>(i) / static_cast<float>(curveSampleCount - 1);
				Vector3 pos = ComputePositionOnRail(rail.controlPoints,t);

				curveObject->SetTranslate(pos);
				// 制御点よりさらに小さくして、線のように見せる
				curveObject->SetScale({kCurveScale, kCurveScale, kCurveScale});
				if(Model::Material* mat = curveObject->GetMaterial()){
					mat->color = railColor;
				}

				if(activeCamera){
					curveObject->SetCamera(activeCamera);
				}

				curveObject->Update();
				curveObject->Draw();
			}
		}

		// ジャンプしてレールに着地できる範囲を可視化する
		// 判定は「レール最近傍点から水平方向にlandingRangeRadius_以内」なので、
		// レールの左右へその距離ぶん離した2列の点列を描いて範囲の境界を示す
		if(showLandingRange_ && rail.controlPoints.size() >= 4){
			for(int i = 0; i < kLandingRangeSampleCount; ++i){
				float t = static_cast<float>(i) / static_cast<float>(kLandingRangeSampleCount - 1);
				Vector3 pos = ComputePositionOnRail(rail.controlPoints,t);

				// 進行方向を前後の座標の差分から求める(端では範囲外へ出ないようtを丸める)
				float tPrev = (t - kForwardSampleDelta < 0.0f)?0.0f:t - kForwardSampleDelta;
				float tNext = (t + kForwardSampleDelta > 1.0f)?1.0f:t + kForwardSampleDelta;
				Vector3 forward = ComputePositionOnRail(rail.controlPoints,tNext) - ComputePositionOnRail(rail.controlPoints,tPrev);

				// 判定は水平方向(XZ平面)のみで行うため、進行方向に垂直な水平ベクトルを求める
				Vector3 horizontalRight = {forward.z, 0.0f, -forward.x};
				if(Length(horizontalRight) <= 0.0f){
					// 真下・真上を向いている区間では左右が決まらないため描画を飛ばす
					continue;
				}
				horizontalRight = Normalize(horizontalRight) * landingRangeRadius_;

				// 左右1組のオブジェクトを取り出す(作りきれていなければ、残りの点は次のフレーム以降に描く)
				Obj3D* rightObject = AcquireSampleObject();
				Obj3D* leftObject = AcquireSampleObject();
				if(!rightObject || !leftObject){
					break;
				}

				rightObject->SetTranslate(pos + horizontalRight);
				leftObject->SetTranslate(pos - horizontalRight);

				for(Obj3D* rangeObject : {rightObject, leftObject}){
					rangeObject->SetScale({kLandingRangeScale, kLandingRangeScale, kLandingRangeScale});
					if(Model::Material* mat = rangeObject->GetMaterial()){
						mat->color = kLandingRangeColor;
					}

					if(activeCamera){
						rangeObject->SetCamera(activeCamera);
					}

					rangeObject->Update();
					rangeObject->Draw();
				}
			}
		}
	}
}

// Play中のレール本体の管のモデルを作る
// レールを細かい区間に分け、区間の境目ごとにレールを囲む輪の頂点を置き、隣り合う輪どうしを三角形でつないで1本の管にする
// 頂点はレールの始点から順に並ぶため、ある区間からある区間までのインデックスは連続した範囲になる(塗った区間だけを描くのに使う)
void RailEditor::BuildTubes(){
	ModelCommon* modelCommon = ModelManager::GetInstance()->GetModelCommon();

	for(Rail& rail : rails_){
		// Catmull-Romは制御点4つ以上必要なので、それ未満のときは管を作らない
		if(rail.controlPoints.size() < 4){
			rail.tubeModel.reset();
			rail.dashedTubeModel.reset();
			rail.tubeSegmentCount = 0;
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
		// 輪の継ぎ目でテクスチャ座標が1周して戻れるよう、輪の最初と最後に同じ位置の頂点を置く
		const uint32_t ringVertexCount = kTubeSideCount + 1;

		// 輪の中心(区間の境目)の座標を先に求めておき、輪の向きを求めるときに同じ座標を何度も計算しないようにする
		std::vector<Vector3> ringCenters(segmentCount + 1);
		for(uint32_t i = 0; i <= segmentCount; ++i){
			ringCenters[i] = ComputePositionOnRail(rail.controlPoints,static_cast<float>(i) / static_cast<float>(segmentCount));
		}

		Model::ModelData modelData;
		modelData.material.textureFilePath = kTubeTexturePath;
		modelData.vertices.reserve(static_cast<size_t>(segmentCount + 1) * ringVertexCount);
		modelData.indices.reserve(static_cast<size_t>(segmentCount) * kTubeSideCount * kTubeIndicesPerQuad);

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
				const Vector3 position = ringCenters[i] + normal * kTubeRadius;

				Model::VertexData vertex = {};
				vertex.position = {position.x, position.y, position.z, 1.0f};
				vertex.texcoord = {sideRate, texcoordV};
				vertex.normal = normal;
				modelData.vertices.push_back(vertex);
			}
		}

		// 隣り合う輪の頂点を、区間の順に四角形(三角形2つ)でつなぐ
		for(uint32_t i = 0; i < segmentCount; ++i){
			const uint32_t ringStart = i * ringVertexCount;
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

	for(size_t railIndex = 0; railIndex < rails_.size(); ++railIndex){
		Rail& rail = rails_[railIndex];

		// 管を作ったレールだけ、レールの始点と終点の制御点の目印を描画する(色はEdit中と同じく今乗っているレールかどうかで分ける)
		// 出ていないレールは点線だけにして、まだ乗れないことが分かるようにする
		if(rail.controlPoints.size() >= 4 && !rail.isLocked){
			Vector4 markerColor = (static_cast<int>(railIndex) == activeRailIndex_)?kActiveRailColor:kInactiveRailColor;
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
		uint32_t grownEnd = 0;
		if(!rail.isLocked){
			const float grownRate = Easing::EaseOutCubic(std::clamp(rail.unlockTimer / kRailUnlockDuration,0.0f,1.0f));
			grownEnd = (std::min)(segmentCount,static_cast<uint32_t>(std::ceil(grownRate * static_cast<float>(segmentCount))));
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

// 全レールの塗った区間を消す
void RailEditor::ClearPaint(){
	for(Rail& rail : rails_){
		rail.isPainted = false;
		rail.paintStartT = 0.0f;
		rail.paintEndT = 0.0f;
	}
}

// 鍵の的が残っていて出ていないレールを反映する
// 出ていなかったレールが出たら、管を伸ばす時間を0から数え始める
void RailEditor::UpdateRailLocks(const std::vector<bool>& lockedRails,float deltaTime){
	const size_t count = (std::min)(rails_.size(),lockedRails.size());
	for(size_t i = 0; i < count; ++i){
		Rail& rail = rails_[i];
		if(lockedRails[i]){
			rail.isLocked = true;
			rail.unlockTimer = 0.0f;
		} else if(rail.isLocked){
			rail.isLocked = false;
			rail.unlockTimer = 0.0f;
		} else{
			rail.unlockTimer = (std::min)(rail.unlockTimer + deltaTime,kRailUnlockDuration);
		}
	}
}

// 管を伸ばす演出を飛ばして、出ていないレールをすぐに反映する
void RailEditor::ResetRailLocks(const std::vector<bool>& lockedRails){
	const size_t count = (std::min)(rails_.size(),lockedRails.size());
	for(size_t i = 0; i < count; ++i){
		rails_[i].isLocked = lockedRails[i];
		rails_[i].unlockTimer = kRailUnlockDuration;
	}
}

// 進行度tと制御点配列から、レール上の座標を計算する(GetPositionOnRail・全レール描画で共用)
Vector3 RailEditor::ComputePositionOnRail(const std::vector<ControlPoint>& controlPoints,float t) const{
	// Catmull-Rom補間には最低4点必要
	if(controlPoints.size() < 4){
		return {0.0f, 0.0f, 0.0f};
	}

	// tを区間数でスケールし、現在の区間(segment)を算出
	size_t numSegments = controlPoints.size() - 3;
	float scaledT = t * static_cast<float>(numSegments);
	size_t segment = static_cast<size_t>(scaledT);

	if(segment >= numSegments){
		segment = numSegments - 1;
	}

	// 区間内でのローカルな進行度(0〜1)
	float localT = scaledT - static_cast<float>(segment);

	const Vector3& p0 = controlPoints[segment].position;
	const Vector3& p1 = controlPoints[segment + 1].position;
	const Vector3& p2 = controlPoints[segment + 2].position;
	const Vector3& p3 = controlPoints[segment + 3].position;

	return CatmullRom(p0,p1,p2,p3,localT);
}

// 進行度t(0〜1)からレール上の座標を取得(対象はアクティブなレール)
Vector3 RailEditor::GetPositionOnRail(float t) const{
	return ComputePositionOnRail(rails_[activeRailIndex_].controlPoints,t);
}

// 指定ワールド座標に最も近いレール上の進行度tを計算する
// 手順: まずkNearestSearchSampleCount分割で粗くサンプリングして最も近い点を求め、
// その前後の区間だけをkNearestRefineIterationCount回の三分探索で絞り込み、精度を上げる
float RailEditor::ComputeNearestTOnRail(const std::vector<ControlPoint>& controlPoints,const Vector3& worldPos) const{
	// Catmull-Rom補間には最低4点必要
	if(controlPoints.size() < 4){
		return 0.0f;
	}

	// 粗いサンプリングで最も近い点を探す
	float bestT = 0.0f;
	float bestDistSq = -1.0f;
	for(int i = 0; i <= kNearestSearchSampleCount; ++i){
		float t = static_cast<float>(i) / static_cast<float>(kNearestSearchSampleCount);
		Vector3 pos = ComputePositionOnRail(controlPoints,t);
		Vector3 diff = pos - worldPos;
		float distSq = Dot(diff,diff);
		if(bestDistSq < 0.0f || distSq < bestDistSq){
			bestDistSq = distSq;
			bestT = t;
		}
	}

	// 粗探索で見つけた点の前後の区間を三分探索でさらに絞り込む
	float rangeWidth = 1.0f / static_cast<float>(kNearestSearchSampleCount);
	float lo = bestT - rangeWidth;
	float hi = bestT + rangeWidth;
	if(lo < 0.0f) lo = 0.0f;
	if(hi > 1.0f) hi = 1.0f;

	for(int i = 0; i < kNearestRefineIterationCount; ++i){
		float m1 = lo + (hi - lo) / 3.0f;
		float m2 = hi - (hi - lo) / 3.0f;

		Vector3 diff1 = ComputePositionOnRail(controlPoints,m1) - worldPos;
		Vector3 diff2 = ComputePositionOnRail(controlPoints,m2) - worldPos;

		if(Dot(diff1,diff1) < Dot(diff2,diff2)){
			hi = m2;
		} else{
			lo = m1;
		}
	}

	return (lo + hi) * 0.5f;
}

// 全レールの中から、指定したワールド座標に最も近いレール・進行度・距離を求める(着地先レールの探索用)
RailEditor::NearestRailResult RailEditor::FindNearestRail(const Vector3& worldPos,int excludeRailIndex) const{
	NearestRailResult result;
	float bestDistance = -1.0f;

	for(size_t i = 0; i < rails_.size(); ++i){
		// 除外指定されたレールと、鍵の的が残っていて出ていないレールは探索しない
		if(static_cast<int>(i) == excludeRailIndex || rails_[i].isLocked){
			continue;
		}

		float t = ComputeNearestTOnRail(rails_[i].controlPoints,worldPos);
		Vector3 nearestPos = ComputePositionOnRail(rails_[i].controlPoints,t);
		float distance = Distance(nearestPos,worldPos);

		if(bestDistance < 0.0f || distance < bestDistance){
			bestDistance = distance;
			result.railIndex = static_cast<int>(i);
			result.t = t;
			result.distance = distance;
			result.position = nearestPos;
		}
	}

	return result;
}

// 進行度t(0〜1)からレール上の回転(オイラー角)を取得(対象はアクティブなレール)
Vector3 RailEditor::GetRotationOnRail(float t) const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	Vector3 forward = GetForwardOnRail(t);

	// Y成分をasinの定義域[-1, 1]にクランプ
	float clampedY = forward.y;
	if(clampedY > 1.0f) clampedY = 1.0f;
	if(clampedY < -1.0f) clampedY = -1.0f;

	// 進行方向からピッチ(X軸回転)とヨー(Y軸回転)を逆算
	float pitch = -std::asin(clampedY);
	float yaw = std::atan2(forward.x,forward.z);

	// ロール(Z軸回転)は制御点間の値を補間して使用(バンク演出用)
	// 位置と同じ区間分けで、区間の始点・終点にあたる制御点(GetPositionOnRailで言うp1・p2)の値を補間し、制御点の位置でその値になるようにする
	float roll = 0.0f;
	if(controlPoints.size() >= 4){
		size_t segment = 0;
		float localT = 0.0f;
		ComputeCurveSegment(controlPoints.size(),t,segment,localT);
		roll = Lerp(controlPoints[segment + kRailStartPointIndex].rotation.z,controlPoints[segment + kRailStartPointIndex + 1].rotation.z,localT);
	}

	return {pitch, yaw, roll};
}

// 進行度t(0〜1)における進行方向(接線ベクトル)を取得(対象はアクティブなレール)
Vector3 RailEditor::GetForwardOnRail(float t) const{
	const float kEpsilon = 0.001f;
	float t0 = t - kEpsilon;
	float t1 = t + kEpsilon;

	if(t0 < 0.0f) t0 = 0.0f;
	if(t1 > 1.0f) t1 = 1.0f;

	Vector3 p0 = GetPositionOnRail(t0);
	Vector3 p1 = GetPositionOnRail(t1);
	Vector3 diff = p1 - p0;

	float length = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

	// 差分がほぼ0の場合は正面向きを返す
	if(length < 1e-5f){
		return {0.0f, 0.0f, 1.0f};
	}

	return {diff.x / length, diff.y / length, diff.z / length};
}

// 進行度t(0〜1)からレール上の速度(各制御点のSpeed値を補間したもの)を取得(対象はアクティブなレール)
float RailEditor::GetSpeedOnRail(float t) const{
	return ComputeSpeedOnRail(rails_[activeRailIndex_].controlPoints,t);
}

// 進行度tと制御点配列から、レール上の速度(各制御点のSpeed値を補間したもの)を計算する
float RailEditor::ComputeSpeedOnRail(const std::vector<ControlPoint>& controlPoints,float t) const{
	// 制御点が無い場合は既定速度を返す
	if(controlPoints.empty()){
		return kDefaultControlPoint.speed;
	}
	// 曲線を作れない(制御点4つ未満)ときは補間できないため、先頭の制御点のSpeed値をそのまま返す
	if(controlPoints.size() < 4){
		return controlPoints[0].speed;
	}

	// GetRotationOnRailのロール補間と同じく、位置と同じ区間分けで区間の始点・終点にあたる制御点のSpeed値を補間する
	size_t segment = 0;
	float localT = 0.0f;
	ComputeCurveSegment(controlPoints.size(),t,segment,localT);
	return Lerp(controlPoints[segment + kRailStartPointIndex].speed,controlPoints[segment + kRailStartPointIndex + 1].speed,localT);
}

// 進行度tから、Catmull-Romの区間番号と区間内の進行度を求める(ComputePositionOnRailと同じ区間分け。制御点4つ以上が前提)
void RailEditor::ComputeCurveSegment(size_t pointCount,float t,size_t& outSegment,float& outLocalT) const{
	size_t numSegments = pointCount - kCatmullRomExtraPointCount;
	float scaledT = t * static_cast<float>(numSegments);
	size_t segment = static_cast<size_t>(scaledT);

	if(segment >= numSegments){
		segment = numSegments - 1;
	}

	outSegment = segment;
	outLocalT = scaledT - static_cast<float>(segment);
}

// 進行度tの位置からレールに沿ってdistanceだけ進んだ先の進行度を取得(対象はアクティブなレール)
// 進行度tは区間ごとに等分されているため、制御点間が長い区間ほど進行度1あたりの移動距離が大きくなる
// そこで現在位置での「進行度1あたりの移動距離」で割り、進む距離を進行度の増加量に変換する
float RailEditor::AdvanceTByDistance(float t,float distance) const{
	// 終端付近では後ろ側の差分を使い、進行度が範囲外にならないようにする
	float t0 = t;
	float t1 = t + kArcLengthSampleDelta;
	if(t1 > 1.0f){
		t0 = 1.0f - kArcLengthSampleDelta;
		t1 = 1.0f;
	}

	// 進行度1あたりの移動距離(1フレームの移動量は小さいため、現在位置の値で直線近似する)
	float distancePerT = Distance(GetPositionOnRail(t1),GetPositionOnRail(t0)) / kArcLengthSampleDelta;
	if(distancePerT < kMinDistancePerT){
		return t;
	}

	return t + distance / distancePerT;
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

// アクティブなレールの制御点数を取得
int RailEditor::GetControlPointCount() const{
	return static_cast<int>(rails_[activeRailIndex_].controlPoints.size());
}

// 進行度tから、直近に通過した制御点のインデックスを取得(対象はアクティブなレール)
// GetPositionOnRailと同じ区間分割を用い、区間の始点(制御点)のインデックスを返す
int RailEditor::GetControlPointIndexFromT(float t) const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	// Catmull-Rom補間には最低4点必要
	if(controlPoints.size() < 4){
		return 0;
	}

	size_t numSegments = controlPoints.size() - 3;
	float scaledT = t * static_cast<float>(numSegments);
	size_t segment = static_cast<size_t>(scaledT);

	if(segment >= numSegments){
		segment = numSegments - 1;
	}

	// 区間の始点(GetPositionOnRailで言うp1)のインデックスが「直近に通過した制御点」
	return static_cast<int>(segment) + 1;
}

// アクティブなレールの、指定した制御点を通る位置の進行度を取得
// 制御点iは、ComputePositionOnRailの区間(i - 1)の始点にあたる
// 0番目は曲線より手前、最後の制御点は曲線より先にあり曲線が通らないため、曲線の始点・終点の進行度に丸める
float RailEditor::GetTOfControlPoint(int index) const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	// Catmull-Rom補間には最低4点必要
	if(controlPoints.size() < 4){
		return 0.0f;
	}

	const float numSegments = static_cast<float>(controlPoints.size() - kCatmullRomExtraPointCount);
	const float t = (static_cast<float>(index) - static_cast<float>(kRailStartPointIndex)) / numSegments;
	return std::clamp(t,0.0f,1.0f);
}

// アクティブなレールで、進行度tまでに通ったチェックポイントのうち、最も先にある制御点のインデックスを取得
// レールの途中に着地した場合も、着地した位置より手前のチェックポイントは通ったものとして扱う
int RailEditor::FindPassedCheckpoint(float t) const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	// 先にある制御点から順に見て、最初に見つかった通過済みのチェックポイントを返す
	for(int i = static_cast<int>(controlPoints.size()) - 1; i >= 0; --i){
		if(controlPoints[i].isCheckpoint && GetTOfControlPoint(i) <= t){
			return i;
		}
	}
	return -1;
}

// アクティブなレールの全長と走り切るまでの秒数、始点から選択中の制御点までの長さと秒数を求める
// 曲線の点列と同じ細かさでレールを分け、分けた区間ごとに「長さ÷その区間の速さ」を足していく
// 速さはPlayerと同じく、制御点のSpeed値に全体の速さ(railSpeed)を掛けたもの
RailEditor::RailStats RailEditor::ComputeActiveRailStats() const{
	RailStats stats;
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	// Catmull-Rom補間には最低4点必要
	if(controlPoints.size() < 4){
		return stats;
	}

	const size_t curveSegmentCount = controlPoints.size() - kCatmullRomExtraPointCount;
	const size_t sampleCount = curveSegmentCount * kCurveSamplesPerSegment;

	// 選択中の制御点までに含まれる、分けた区間の数(制御点が曲線を通る位置は区間の境目にあたるため、整数で数えられる)
	size_t samplesToSelected = 0;
	if(selectedPointIndex_ >= 0 && selectedPointIndex_ < static_cast<int>(controlPoints.size())){
		const int curveSegmentIndex = std::clamp(selectedPointIndex_ - static_cast<int>(kRailStartPointIndex),0,static_cast<int>(curveSegmentCount));
		samplesToSelected = static_cast<size_t>(curveSegmentIndex) * kCurveSamplesPerSegment;
	}

	// 区間の終点は次の区間の始点になるため、前の区間の終点を使い回して座標の計算を1回で済ませる
	Vector3 start = ComputePositionOnRail(controlPoints,0.0f);
	for(size_t i = 0; i < sampleCount; ++i){
		const float startT = static_cast<float>(i) / static_cast<float>(sampleCount);
		const float endT = static_cast<float>(i + 1) / static_cast<float>(sampleCount);
		const Vector3 end = ComputePositionOnRail(controlPoints,endT);

		const float length = Distance(start,end);
		const float speed = ComputeSpeedOnRail(controlPoints,Lerp(startT,endT,kStatsSpeedSampleRate)) * baseRailSpeed_;
		const float time = (speed >= kMinStatsSpeed)?length / speed:0.0f;

		stats.length += length;
		stats.time += time;
		if(i < samplesToSelected){
			stats.lengthToSelected += length;
			stats.timeToSelected += time;
		}

		start = end;
	}

	return stats;
}

// アクティブなレールの全制御点を囲む範囲の中心座標を取得
Vector3 RailEditor::GetControlPointsCenter() const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	if(controlPoints.empty()){
		return {0.0f, 0.0f, 0.0f};
	}

	Vector3 minPos = controlPoints[0].position;
	Vector3 maxPos = controlPoints[0].position;

	for(const auto& point : controlPoints){
		minPos.x = (point.position.x < minPos.x)?point.position.x:minPos.x;
		minPos.y = (point.position.y < minPos.y)?point.position.y:minPos.y;
		minPos.z = (point.position.z < minPos.z)?point.position.z:minPos.z;

		maxPos.x = (point.position.x > maxPos.x)?point.position.x:maxPos.x;
		maxPos.y = (point.position.y > maxPos.y)?point.position.y:maxPos.y;
		maxPos.z = (point.position.z > maxPos.z)?point.position.z:maxPos.z;
	}

	return {
		(minPos.x + maxPos.x) * 0.5f,
		(minPos.y + maxPos.y) * 0.5f,
		(minPos.z + maxPos.z) * 0.5f
	};
}

// アクティブなレールの全制御点を囲む範囲のおおよその半径を取得
float RailEditor::GetControlPointsRadius() const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	if(controlPoints.empty()){
		return 0.0f;
	}

	Vector3 center = GetControlPointsCenter();
	float maxDistSq = 0.0f;

	for(const auto& point : controlPoints){
		float dx = point.position.x - center.x;
		float dz = point.position.z - center.z;
		float distSq = dx * dx + dz * dz; // XZ平面での距離の2乗

		if(distSq > maxDistSq){
			maxDistSq = distSq;
		}
	}

	return std::sqrt(maxDistSq);
}

// ImGui非表示時でもキー操作で表示切り替えを行うための関数(対象はアクティブなレール)
void RailEditor::ToggleShowControlPointModels(){
	rails_[activeRailIndex_].showControlPointModels = !rails_[activeRailIndex_].showControlPointModels;
}
void RailEditor::ToggleShowCurve(){
	rails_[activeRailIndex_].showCurve = !rails_[activeRailIndex_].showCurve;
}

// 指定インデックスのレール1本だけをJSONファイルに保存する(書き込めなかったときはfalse)
bool RailEditor::SaveRailToFile(int index){
	Rail& rail = rails_[index];

	// ルート要素。nameは読み込み時の簡易フォーマットチェック・レール名の保存用
	nlohmann::json root;
	root["name"] = rail.name;

	// 各制御点を配列に詰める
	nlohmann::json pointsJson = nlohmann::json::array();
	for(const auto& cp : rail.controlPoints){
		nlohmann::json pj;
		pj["position"] = {cp.position.x, cp.position.y, cp.position.z};
		pj["rotation"] = {cp.rotation.x, cp.rotation.y, cp.rotation.z};
		pj["speed"] = cp.speed;
		// レール間分岐移動用のデータ
		pj["branchTargetRailIndex"] = cp.branchTargetRailIndex;
		pj["branchTargetPointIndex"] = cp.branchTargetPointIndex;
		pj["isCheckpoint"] = cp.isCheckpoint;
		pointsJson.push_back(pj);
	}
	root["control_points"] = pointsJson;

	std::string savePathStr = GetRailFilePath(index);

	// 保存先フォルダが無ければ作成しておく
	std::filesystem::path savePath(savePathStr);
	if(savePath.has_parent_path()){
		std::filesystem::create_directories(savePath.parent_path());
	}

	// ファイルへ書き出し(setwで人が読める整形出力にする)
	std::ofstream file(savePathStr);
	if(!file.is_open()){
		return false;
	}
	file << std::setw(4) << root << std::endl;
	return true;
}

// 指定インデックスのレール1本だけをJSONファイルから読み込む(ファイルが無ければ何もしない)
void RailEditor::LoadRailFromFile(int index){
	Rail& rail = rails_[index];

	std::ifstream file(GetRailFilePath(index));
	if(!file.is_open()){
		// ファイルがまだ無い(初回起動・新規追加直後など)ときは現状維持
		return;
	}

	nlohmann::json root;
	file >> root;

	// 最低限のフォーマットチェック。想定外なら読み込みを中止して現状維持
	if(!root.is_object() || !root.contains("control_points") || !root["control_points"].is_array()){
		return;
	}

	// レール名が保存されていれば復元する
	if(root.contains("name") && root["name"].is_string()){
		rail.name = root["name"].get<std::string>();
	}

	// 読み込んだ内容で制御点を置き換える
	rail.controlPoints.clear();
	for(const auto& pj : root["control_points"]){
		ControlPoint cp{};
		cp.position.x = pj["position"][0].get<float>();
		cp.position.y = pj["position"][1].get<float>();
		cp.position.z = pj["position"][2].get<float>();
		cp.rotation.x = pj["rotation"][0].get<float>();
		cp.rotation.y = pj["rotation"][1].get<float>();
		cp.rotation.z = pj["rotation"][2].get<float>();
		cp.speed = pj["speed"].get<float>();
		// レール間分岐移動用のデータ(旧フォーマットのJSONには無いため、value()で既定値-1を補う)
		cp.branchTargetRailIndex = pj.value("branchTargetRailIndex",-1);
		cp.branchTargetPointIndex = pj.value("branchTargetPointIndex",-1);
		// チェックポイントの設定(旧フォーマットのJSONには無いため、value()で既定値falseを補う)
		cp.isCheckpoint = pj.value("isCheckpoint",false);
		rail.controlPoints.push_back(cp);
	}

	// 制御点が0個だと他の処理(補間など)が破綻するので最低1点は保証する
	if(rail.controlPoints.empty()){
		rail.controlPoints.push_back(kDefaultControlPoint);
	}

	// 描画用オブジェクトの数を制御点数に合わせる
	SyncPointObjectsToControlPoints(rail);
}

// 全レールをそれぞれ対応するJSONファイルへ保存する
void RailEditor::SaveToJson(){
	// まだ履歴に積んでいない編集があれば先に確定し、保存した状態を保存済みとして記録できるようにする
	CommitEditIfChanged();

	bool isAllSaved = true;
	for(size_t i = 0; i < rails_.size(); ++i){
		if(!SaveRailToFile(static_cast<int>(i))){
			isAllSaved = false;
		}
	}

	// 1本でも書き込めなかったときは、未保存の表示を残す
	if(isAllSaved){
		history_.MarkSaved();
	}
}

// 全レールをJSONファイルから読み込む(Loadボタン用)
// 読み込みも1回の編集として履歴に積み、Ctrl+Zで読み込む前に戻せるようにする
void RailEditor::LoadFromJson(){
	LoadRailsFromFiles();
	CommitEditIfChanged();
	history_.MarkSaved();
}

// 全レールをJSONファイルから読み込む
// メモリ上に既にあるレールは対応ファイルで上書きし、ディスクにのみ存在するレール(rail1.json, rail2.json...)は
// New Rail相当の追加をしながら連番で読み込んでいく(ファイルが途切れたところで打ち切り)
void RailEditor::LoadRailsFromFiles(){
	// 既存のレールを、それぞれ対応するファイルがあれば読み込み直す
	for(size_t i = 0; i < rails_.size(); ++i){
		LoadRailFromFile(static_cast<int>(i));
	}

	// メモリ上にまだ無いレールも、ファイルが存在する限り新規追加しながら連番で読み込む
	int index = static_cast<int>(rails_.size());
	while(std::filesystem::exists(GetRailFilePath(index))){
		AddRail();
		LoadRailFromFile(index);
		++index;
	}

	// 読み込み後は先頭のレールをアクティブな編集対象にする
	activeRailIndex_ = 0;
}
// 今のレールの状態を編集履歴に残す形で取得する
RailEditor::RailsSnapshot RailEditor::MakeSnapshot() const{
	RailsSnapshot snapshot;
	snapshot.reserve(rails_.size());
	for(const Rail& rail : rails_){
		snapshot.push_back({rail.name, rail.controlPoints});
	}
	return snapshot;
}

// 編集履歴の状態をレールに反映する
void RailEditor::ApplySnapshot(const RailsSnapshot& snapshot){
	const int prevActiveRailIndex = activeRailIndex_;

	// レールの本数を履歴の状態にそろえる(New Railを戻したときは減らし、やり直したときは増やす)
	while(rails_.size() < snapshot.size()){
		AddRail();
	}
	while(rails_.size() > snapshot.size()){
		rails_.pop_back();
	}

	for(size_t i = 0; i < rails_.size(); ++i){
		rails_[i].name = snapshot[i].name;
		rails_[i].controlPoints = snapshot[i].controlPoints;
		SyncPointObjectsToControlPoints(rails_[i]);
	}

	// アクティブなレールは戻す前のものを残し、消えたレールを指していれば最後のレールにする
	// (AddRail()は追加したレールをアクティブにするため、ここで戻す)
	activeRailIndex_ = (std::min)(prevActiveRailIndex,static_cast<int>(rails_.size()) - 1);
	if(activeRailIndex_ != prevActiveRailIndex){
		selectedPointIndex_ = -1;
	}
	isDragging_ = false;
}

// 今のレールの状態が、最後に確定した状態から変わっていれば確定して履歴に積む
// 毎フレームの比較で状態を丸ごとコピーしないよう、変わっていたときだけ状態を作る
void RailEditor::CommitEditIfChanged(){
	const RailsSnapshot& committed = history_.GetCommitted();
	bool isChanged = (committed.size() != rails_.size());
	for(size_t i = 0; !isChanged && i < rails_.size(); ++i){
		isChanged = (committed[i].name != rails_[i].name || committed[i].controlPoints != rails_[i].controlPoints);
	}
	if(!isChanged){
		return;
	}

	history_.Commit(MakeSnapshot());
	EditorDocumentManager::GetInstance()->RecordEdit(this);
}

// 1つ前の状態に戻す(Ctrl+Z)
bool RailEditor::UndoEdit(){
	RailsSnapshot snapshot;
	if(!history_.Undo(snapshot)){
		return false;
	}
	ApplySnapshot(snapshot);
	return true;
}

// Undoで戻した状態をやり直す(Ctrl+Y)
bool RailEditor::RedoEdit(){
	RailsSnapshot snapshot;
	if(!history_.Redo(snapshot)){
		return false;
	}
	ApplySnapshot(snapshot);
	return true;
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
