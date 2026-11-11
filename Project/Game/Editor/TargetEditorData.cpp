#include "TargetEditor.h"
#include "externals/json.hpp"
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <string>
#include <utility>

// TargetEditorのうち、配置の保存・読み込み(target.json)、編集履歴(Undo/Redo)、ステージの切り替えをまとめたファイル

namespace{
	// 的の配置データの保存先(ステージのフォルダの中のファイル名)
	const std::string kSaveFileName = "target.json";
}

// 全ステージの配置内容をそれぞれのステージのJSONファイルに保存する
// 保存していない変更があるかは全ステージまとめて見ているため、保存も全ステージまとめて行う
void TargetEditor::SaveToJson(){
	// まだ履歴に積んでいない編集があれば先に確定し、保存した状態を保存済みとして記録できるようにする
	CommitEditIfChanged();

	bool isAllSaved = true;
	for(int stage = 0; stage < StageInfo::kStageCount; ++stage){
		// ルート要素。targetsは読み込み時の簡易フォーマットチェックにも使う
		nlohmann::json root;

		nlohmann::json targetsJson = nlohmann::json::array();
		for(const auto& target : GetStageTargets(stage)){
			nlohmann::json tj;
			tj["position"] = {target.position.x, target.position.y, target.position.z};
			tj["unlockRailIndex"] = target.unlockRailIndex;
			targetsJson.push_back(tj);
		}
		root["targets"] = targetsJson;

		const std::string savePathStr = GetSaveFilePath(stage);

		// 保存先フォルダが無ければ作成しておく
		std::filesystem::path savePath(savePathStr);
		if(savePath.has_parent_path()){
			std::filesystem::create_directories(savePath.parent_path());
		}

		// ファイルへ書き出し(setwで人が読める整形出力にする)
		std::ofstream file(savePathStr);
		if(!file.is_open()){
			isAllSaved = false;
			continue;
		}
		file << std::setw(4) << root << std::endl;
	}

	// 1つでも書き込めなかったときは、未保存の表示を残す
	if(isAllSaved){
		history_.MarkSaved();
	}
}

// 全ステージの配置内容をJSONファイルから読み込む(Loadボタン用。どのステージのファイルも無ければ何もしない)
// 読み込みも1回の編集として履歴に積み、Ctrl+Zで読み込む前に戻せるようにする
void TargetEditor::LoadFromJson(){
	bool isAnyLoaded = false;
	for(int stage = 0; stage < StageInfo::kStageCount; ++stage){
		if(LoadTargetsFromFile(stage)){
			isAnyLoaded = true;
		}
	}
	if(!isAnyLoaded){
		return;
	}
	CommitEditIfChanged();
	history_.MarkSaved();
}

// 指定ステージの配置内容をJSONファイルから読み込む(ファイルが無い・形式が違うときは現状維持でfalse)
bool TargetEditor::LoadTargetsFromFile(int stageIndex){
	std::ifstream file(GetSaveFilePath(stageIndex));
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
	std::vector<TargetPoint>& points = GetStageTargets(stageIndex);
	points.clear();
	for(const auto& tj : root["targets"]){
		TargetPoint point{};
		point.position.x = tj["position"][0].get<float>();
		point.position.y = tj["position"][1].get<float>();
		point.position.z = tj["position"][2].get<float>();
		// 壊すと出るレール(旧フォーマットのJSONには無いため、value()で既定値-1を補う)
		point.unlockRailIndex = tj.value("unlockRailIndex",-1);
		points.push_back(point);
	}

	// 読み込みで的の個数が変わるため、選択とドラッグ状態は解除する
	selectedIndex_ = -1;
	isDragging_ = false;
	return true;
}

// 配置内容が、最後に確定した状態から変わっていれば確定して履歴に積む
// 毎フレームの比較で状態を丸ごとコピーしないよう、ステージごとに比べて、変わっていたときだけ状態を作る
void TargetEditor::CommitEditIfChanged(){
	const StagesSnapshot& committed = history_.GetCommitted();
	bool isChanged = false;
	for(int stage = 0; !isChanged && stage < StageInfo::kStageCount; ++stage){
		isChanged = (GetStageTargets(stage) != committed[stage]);
	}
	if(!isChanged){
		return;
	}
	history_.Commit(MakeSnapshot());
	EditorDocumentManager::GetInstance()->RecordEdit(this);
}

// 1つ前の状態に戻す(Ctrl+Z)
bool TargetEditor::UndoEdit(){
	StagesSnapshot snapshot;
	if(!history_.Undo(snapshot)){
		return false;
	}
	ApplySnapshot(snapshot);
	// 戻した結果的が減っていれば、選択が範囲外を指さないよう解除する
	if(selectedIndex_ >= static_cast<int>(targets_.size())){
		selectedIndex_ = -1;
	}
	isDragging_ = false;
	return true;
}

// Undoで戻した状態をやり直す(Ctrl+Y)
bool TargetEditor::RedoEdit(){
	StagesSnapshot snapshot;
	if(!history_.Redo(snapshot)){
		return false;
	}
	ApplySnapshot(snapshot);
	// やり直した結果的が減っていれば、選択が範囲外を指さないよう解除する
	if(selectedIndex_ >= static_cast<int>(targets_.size())){
		selectedIndex_ = -1;
	}
	isDragging_ = false;
	return true;
}

// 指定ステージの配置の配列を取得(編集中のステージはtargets_、それ以外は預けてある配列)
std::vector<TargetEditor::TargetPoint>& TargetEditor::GetStageTargets(int stageIndex){
	return (stageIndex == stageIndex_)?targets_:storedStageTargets_[stageIndex];
}
const std::vector<TargetEditor::TargetPoint>& TargetEditor::GetStageTargets(int stageIndex) const{
	return (stageIndex == stageIndex_)?targets_:storedStageTargets_[stageIndex];
}

// 指定ステージの配置の保存先ファイルパスを求める
std::string TargetEditor::GetSaveFilePath(int stageIndex) const{
	return StageInfo::GetStageDirectory(stageIndex) + kSaveFileName;
}

// 今の全ステージの配置を編集履歴に残す形で取得する
TargetEditor::StagesSnapshot TargetEditor::MakeSnapshot() const{
	StagesSnapshot snapshot;
	for(int stage = 0; stage < StageInfo::kStageCount; ++stage){
		snapshot[stage] = GetStageTargets(stage);
	}
	return snapshot;
}

// 編集履歴の状態を全ステージの配置に反映する
void TargetEditor::ApplySnapshot(const StagesSnapshot& snapshot){
	for(int stage = 0; stage < StageInfo::kStageCount; ++stage){
		GetStageTargets(stage) = snapshot[stage];
	}
}

// 編集・プレイするステージを切り替える
void TargetEditor::SetStageIndex(int stageIndex){
	if(!StageInfo::IsValidStageIndex(stageIndex) || stageIndex == stageIndex_){
		return;
	}

	// 今のステージの配置を預け、切り替えた先のステージの配置を取り出す
	std::swap(targets_,storedStageTargets_[stageIndex_]);
	std::swap(targets_,storedStageTargets_[stageIndex]);
	stageIndex_ = stageIndex;

	// 前のステージの的を指したままにならないよう、選択とドラッグ状態を解除する
	selectedIndex_ = -1;
	pendingPickIndex_ = -1;
	isDragging_ = false;
}
