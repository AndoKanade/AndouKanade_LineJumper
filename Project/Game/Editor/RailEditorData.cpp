#include "RailEditor.h"
#include "RailEditorInternal.h"
#include "Obj3D.h"
#include "externals/json.hpp"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <string>

// RailEditorのうち、レールの保存・読み込み(rail*.json)と編集履歴(Undo/Redo)をまとめたファイル

using namespace RailEditorInternal;

namespace{
	// 制御点データの保存先(ステージのフォルダの中のフォルダ名と、ファイル名の頭・拡張子)
	const std::string kRailSaveSubDir = "rail/";
	const std::string kRailFileNamePrefix = "rail";
	const std::string kRailFileExtension = ".json";
}

// ステージとレールのインデックスから保存先ファイルパスを求める
std::string RailEditor::GetRailFilePath(int stageIndex,int railIndex) const{
	return StageInfo::GetStageDirectory(stageIndex) + kRailSaveSubDir + kRailFileNamePrefix + std::to_string(railIndex) + kRailFileExtension;
}

// レール1本だけをfilePathのJSONファイルに保存する(書き込めなかったときはfalse)
bool RailEditor::SaveRailToFile(const Rail& rail,const std::string& filePath) const{

	// ルート要素。nameは読み込み時の簡易フォーマットチェック・レール名の保存用
	nlohmann::json root;
	root["name"] = rail.name;
	root["isGoalRail"] = rail.isGoalRail;

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

	// 保存先フォルダが無ければ作成しておく
	std::filesystem::path savePath(filePath);
	if(savePath.has_parent_path()){
		std::filesystem::create_directories(savePath.parent_path());
	}

	// ファイルへ書き出し(setwで人が読める整形出力にする)
	std::ofstream file(filePath);
	if(!file.is_open()){
		return false;
	}
	file << std::setw(4) << root << std::endl;
	return true;
}

// レール1本だけをfilePathのJSONファイルから読み込む(ファイルが無ければ何もしない)
void RailEditor::LoadRailFromFile(Rail& rail,const std::string& filePath){
	std::ifstream file(filePath);
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
	// ゴールのレールか(旧フォーマットのJSONには無いため、value()で既定値falseを補う)
	rail.isGoalRail = root.value("isGoalRail",false);

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

// 全ステージの全レールをそれぞれ対応するJSONファイルへ保存する
// 保存していない変更があるかは全ステージまとめて見ているため、保存も全ステージまとめて行う
void RailEditor::SaveToJson(){
	// まだ履歴に積んでいない編集があれば先に確定し、保存した状態を保存済みとして記録できるようにする
	CommitEditIfChanged();

	bool isAllSaved = true;
	for(int stage = 0; stage < StageInfo::kStageCount; ++stage){
		const std::vector<Rail>& rails = GetStageRails(stage);
		for(size_t i = 0; i < rails.size(); ++i){
			if(!SaveRailToFile(rails[i],GetRailFilePath(stage,static_cast<int>(i)))){
				isAllSaved = false;
			}
		}
	}

	// 1本でも書き込めなかったときは、未保存の表示を残す
	if(isAllSaved){
		history_.MarkSaved();
	}
}

// 全ステージのレールをJSONファイルから読み込む(Loadボタン用)
// 読み込みも1回の編集として履歴に積み、Ctrl+Zで読み込む前に戻せるようにする
void RailEditor::LoadFromJson(){
	LoadRailsFromFiles();
	CommitEditIfChanged();
	history_.MarkSaved();
}

// 全ステージのレールをJSONファイルから読み込む
// メモリ上に既にあるレールは対応ファイルで上書きし、ディスクにのみ存在するレール(rail1.json, rail2.json...)は
// New Rail相当の追加をしながら連番で読み込んでいく(ファイルが途切れたところで打ち切り)
void RailEditor::LoadRailsFromFiles(){
	for(int stage = 0; stage < StageInfo::kStageCount; ++stage){
		std::vector<Rail>& rails = GetStageRails(stage);

		// レールが1本も無いと他の処理(補間など)が破綻するため、ファイルが無いステージにも最低1本は用意する
		if(rails.empty()){
			AppendRail(rails);
		}

		// 既存のレールを、それぞれ対応するファイルがあれば読み込み直す
		for(size_t i = 0; i < rails.size(); ++i){
			LoadRailFromFile(rails[i],GetRailFilePath(stage,static_cast<int>(i)));
		}

		// メモリ上にまだ無いレールも、ファイルが存在する限り新規追加しながら連番で読み込む
		int index = static_cast<int>(rails.size());
		while(std::filesystem::exists(GetRailFilePath(stage,index))){
			AppendRail(rails);
			LoadRailFromFile(rails.back(),GetRailFilePath(stage,index));
			++index;
		}
	}

	// 読み込み後は先頭のレールをアクティブな編集対象にする
	activeRailIndex_ = 0;
	selectedPointIndex_ = -1;
}

// 今の全ステージのレールの状態を編集履歴に残す形で取得する
RailEditor::StagesSnapshot RailEditor::MakeSnapshot() const{
	StagesSnapshot snapshot;
	for(int stage = 0; stage < StageInfo::kStageCount; ++stage){
		const std::vector<Rail>& rails = GetStageRails(stage);
		snapshot[stage].reserve(rails.size());
		for(const Rail& rail : rails){
			snapshot[stage].push_back({rail.name, rail.controlPoints, rail.isGoalRail});
		}
	}
	return snapshot;
}

// 編集履歴の状態を全ステージのレールに反映する
void RailEditor::ApplySnapshot(const StagesSnapshot& snapshot){
	const int prevActiveRailIndex = activeRailIndex_;

	for(int stage = 0; stage < StageInfo::kStageCount; ++stage){
		std::vector<Rail>& rails = GetStageRails(stage);
		const RailsSnapshot& stageSnapshot = snapshot[stage];

		// レールの本数を履歴の状態にそろえる(New Railを戻したときは減らし、やり直したときは増やす)
		while(rails.size() < stageSnapshot.size()){
			AppendRail(rails);
		}
		while(rails.size() > stageSnapshot.size()){
			rails.pop_back();
		}

		for(size_t i = 0; i < rails.size(); ++i){
			rails[i].name = stageSnapshot[i].name;
			rails[i].controlPoints = stageSnapshot[i].controlPoints;
			rails[i].isGoalRail = stageSnapshot[i].isGoalRail;
			SyncPointObjectsToControlPoints(rails[i]);
		}
	}

	// アクティブなレールは戻す前のものを残し、消えたレールを指していれば最後のレールにする
	activeRailIndex_ = (std::min)(prevActiveRailIndex,static_cast<int>(rails_.size()) - 1);
	if(activeRailIndex_ != prevActiveRailIndex){
		selectedPointIndex_ = -1;
	}
	isDragging_ = false;
}

// 今のレールの状態が、最後に確定した状態から変わっていれば確定して履歴に積む
// 毎フレームの比較で状態を丸ごとコピーしないよう、ステージ・レールごとに比べて、変わっていたときだけ状態を作る
void RailEditor::CommitEditIfChanged(){
	const StagesSnapshot& committed = history_.GetCommitted();
	bool isChanged = false;
	for(int stage = 0; !isChanged && stage < StageInfo::kStageCount; ++stage){
		const std::vector<Rail>& rails = GetStageRails(stage);
		const RailsSnapshot& committedRails = committed[stage];
		isChanged = (committedRails.size() != rails.size());
		for(size_t i = 0; !isChanged && i < rails.size(); ++i){
			isChanged = (committedRails[i].name != rails[i].name || committedRails[i].controlPoints != rails[i].controlPoints || committedRails[i].isGoalRail != rails[i].isGoalRail);
		}
	}
	if(!isChanged){
		return;
	}

	history_.Commit(MakeSnapshot());
	EditorDocumentManager::GetInstance()->RecordEdit(this);
}

// 1つ前の状態に戻す(Ctrl+Z)
bool RailEditor::UndoEdit(){
	StagesSnapshot snapshot;
	if(!history_.Undo(snapshot)){
		return false;
	}
	ApplySnapshot(snapshot);
	return true;
}

// Undoで戻した状態をやり直す(Ctrl+Y)
bool RailEditor::RedoEdit(){
	StagesSnapshot snapshot;
	if(!history_.Redo(snapshot)){
		return false;
	}
	ApplySnapshot(snapshot);
	return true;
}
