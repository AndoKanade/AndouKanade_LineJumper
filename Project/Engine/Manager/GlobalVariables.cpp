#include "GlobalVariables.h"
#include "ImGuiManager.h"
#include "imgui.h"
#include "EditorWidgets.h"
#include "EditorContext.h"
#include "externals/json.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>

#ifdef USE_IMGUI
namespace{
	// 文字列に、検索する文字が含まれているか(大文字・小文字は区別しない。検索する文字が空なら含まれているとみなす)
	bool ContainsIgnoreCase(const std::string& text,const char* pattern){
		const std::string patternText = pattern;
		auto toLower = [](char c){ return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
		auto it = std::search(text.begin(),text.end(),patternText.begin(),patternText.end(),
			[&toLower](char a,char b){ return toLower(a) == toLower(b); });
		return it != text.end() || patternText.empty();
	}
}
#endif

// シングルトンのインスタンス取得
GlobalVariables* GlobalVariables::GetInstance(){
	static GlobalVariables instance;
	return &instance;
}

// グループを作成する(既にあれば何もしない)
void GlobalVariables::CreateGroup(const std::string& groupName){
	// operator[] は存在しなければ空のGroupを作る。存在すればそのまま
	datas_[groupName];
}

// ---------------------------------------------------------------------------
// 項目の登録:まだ無いキーのときだけデフォルト値として追加する。
// (起動時にLoadFilesで読み込んだ値を、後から呼ばれるAddItemのデフォルトで
//  上書きしてしまわないための「追加のみ」動作)
// ---------------------------------------------------------------------------
void GlobalVariables::AddItem(const std::string& groupName,const std::string& key,int32_t value){
	Group& group = datas_[groupName];
	if(group.items.find(key) == group.items.end()){
		group.items[key] = value;
	}
}
void GlobalVariables::AddItem(const std::string& groupName,const std::string& key,float value){
	Group& group = datas_[groupName];
	if(group.items.find(key) == group.items.end()){
		group.items[key] = value;
	}
}
void GlobalVariables::AddItem(const std::string& groupName,const std::string& key,bool value){
	Group& group = datas_[groupName];
	if(group.items.find(key) == group.items.end()){
		group.items[key] = value;
	}
}
void GlobalVariables::AddItem(const std::string& groupName,const std::string& key,const Vector3& value){
	Group& group = datas_[groupName];
	if(group.items.find(key) == group.items.end()){
		group.items[key] = value;
	}
}

// --- 値の設定(既存キーを上書き、無ければ追加) ---
void GlobalVariables::SetValue(const std::string& groupName,const std::string& key,int32_t value){
	datas_[groupName].items[key] = value;
}
void GlobalVariables::SetValue(const std::string& groupName,const std::string& key,float value){
	datas_[groupName].items[key] = value;
}
void GlobalVariables::SetValue(const std::string& groupName,const std::string& key,bool value){
	datas_[groupName].items[key] = value;
}
void GlobalVariables::SetValue(const std::string& groupName,const std::string& key,const Vector3& value){
	datas_[groupName].items[key] = value;
}

// ---------------------------------------------------------------------------
// 値の取得。int/floatは相互に変換して返すので、JSONの数値が
// どちらの型で格納されていても数値として安全に取り出せる。
// ---------------------------------------------------------------------------
int32_t GlobalVariables::GetIntValue(const std::string& groupName,const std::string& key) const{
	auto groupIt = datas_.find(groupName);
	if(groupIt == datas_.end()) return 0;
	auto itemIt = groupIt->second.items.find(key);
	if(itemIt == groupIt->second.items.end()) return 0;

	const Item& item = itemIt->second;
	if(std::holds_alternative<int32_t>(item)) return std::get<int32_t>(item);
	if(std::holds_alternative<float>(item)) return static_cast<int32_t>(std::get<float>(item));
	return 0;
}
float GlobalVariables::GetFloatValue(const std::string& groupName,const std::string& key) const{
	auto groupIt = datas_.find(groupName);
	if(groupIt == datas_.end()) return 0.0f;
	auto itemIt = groupIt->second.items.find(key);
	if(itemIt == groupIt->second.items.end()) return 0.0f;

	const Item& item = itemIt->second;
	if(std::holds_alternative<float>(item)) return std::get<float>(item);
	if(std::holds_alternative<int32_t>(item)) return static_cast<float>(std::get<int32_t>(item));
	return 0.0f;
}
bool GlobalVariables::GetBoolValue(const std::string& groupName,const std::string& key) const{
	auto groupIt = datas_.find(groupName);
	if(groupIt == datas_.end()) return false;
	auto itemIt = groupIt->second.items.find(key);
	if(itemIt == groupIt->second.items.end()) return false;

	const Item& item = itemIt->second;
	if(std::holds_alternative<bool>(item)) return std::get<bool>(item);
	return false;
}
Vector3 GlobalVariables::GetVector3Value(const std::string& groupName,const std::string& key) const{
	auto groupIt = datas_.find(groupName);
	if(groupIt == datas_.end()) return {0.0f, 0.0f, 0.0f};
	auto itemIt = groupIt->second.items.find(key);
	if(itemIt == groupIt->second.items.end()) return {0.0f, 0.0f, 0.0f};

	const Item& item = itemIt->second;
	if(std::holds_alternative<Vector3>(item)) return std::get<Vector3>(item);
	return {0.0f, 0.0f, 0.0f};
}

// 毎フレーム更新:ファイル監視(ホットリロード)+ ImGui描画
void GlobalVariables::Update(){
	// 外部でJSONが書き換えられていたら読み直す(ホットリロードはPlayモードでも有効)
	CheckFileUpdate();

#ifdef USE_IMGUI
	// Playモード中は調整パネルを隠す(値の反映自体は上のCheckFileUpdate/GetValueで有効なまま)
	if(EditorContext::GetInstance()->IsPlayMode()){ return; }

	// 固定タイルレイアウトの下段・右半分に配置
	EditorWidgets::BeginFixedPanel("Global Variables",EditorWidgets::ComputeLayout().globalVars);

	// 全グループを一括保存するボタン
	if(ImGui::Button("Save All")){
		for(const auto& [groupName, group] : datas_){
			SaveFile(groupName);
		}
	}
	ImGui::SameLine();

	// 表示するグループの選択(All: 全グループ)。下段は高さが低いため、1つに絞って見やすくできるようにする
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * kGroupComboWidthInFonts);
	if(ImGui::BeginCombo("Group",shownGroupName_.empty()?kAllGroupsLabel:shownGroupName_.c_str())){
		if(ImGui::Selectable(kAllGroupsLabel,shownGroupName_.empty())){
			shownGroupName_.clear();
		}
		for(const auto& [groupName, group] : datas_){
			if(ImGui::Selectable(groupName.c_str(),shownGroupName_ == groupName)){
				shownGroupName_ = groupName;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();

	// 項目名の検索欄(大文字・小文字は区別しない。グループ名に一致したときは、そのグループの項目をすべて出す)
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * kSearchFieldWidthInFonts);
	ImGui::InputTextWithHint("##Search","Search",searchText_,sizeof(searchText_));
	const bool isSearching = (searchText_[0] != '\0');
	ImGui::Separator();

	// グループごとに折りたたみ表示
	for(auto& [groupName, group] : datas_){
		// グループを1つに絞っているときは、ほかのグループを出さない
		if(!shownGroupName_.empty() && groupName != shownGroupName_){
			continue;
		}

		// 検索中は、グループ名か項目名のどれかに一致するグループだけを出す
		const bool isGroupNameMatched = ContainsIgnoreCase(groupName,searchText_);
		if(isSearching && !isGroupNameMatched){
			bool hasMatchedItem = false;
			for(const auto& [key, item] : group.items){
				if(ContainsIgnoreCase(key,searchText_)){
					hasMatchedItem = true;
					break;
				}
			}
			if(!hasMatchedItem){
				continue;
			}
		}

		// グループをまたいだ同名キーのID衝突を防ぐ
		ImGui::PushID(groupName.c_str());

		// 1つに絞っているときと検索中は、開く手間を省くため開いた状態で出す
		if(!shownGroupName_.empty() || isSearching){
			ImGui::SetNextItemOpen(true);
		}
		if(ImGui::CollapsingHeader(groupName.c_str())){
			// 各項目を型に応じたウィジェットで編集([-]/[+]ボタン方式)
			for(auto& [key, item] : group.items){
				// 検索中は、一致した項目だけを出す(グループ名に一致したときはすべて出す)
				if(isSearching && !isGroupNameMatched && !ContainsIgnoreCase(key,searchText_)){
					continue;
				}
				if(std::holds_alternative<int32_t>(item)){
					// int32_t は int と同一なので参照をそのまま渡せる
					EditorWidgets::ButtonInt(key.c_str(),std::get<int32_t>(item));
				} else if(std::holds_alternative<float>(item)){
					EditorWidgets::ButtonFloat(key.c_str(),std::get<float>(item),0.01f,0.1f);
				} else if(std::holds_alternative<bool>(item)){
					ImGui::Checkbox(key.c_str(),&std::get<bool>(item));
				} else if(std::holds_alternative<Vector3>(item)){
					EditorWidgets::ButtonVector3(key.c_str(),std::get<Vector3>(item),0.1f,1.0f);
				}
			}

			// このグループだけを保存
			if(ImGui::Button("Save")){
				SaveFile(groupName);
			}
		}

		ImGui::PopID();
	}

	ImGui::End();
#endif
}

// 保存先ディレクトリ内の.jsonを全て読み込む(起動時に一度)
void GlobalVariables::LoadFiles(){
	std::filesystem::path dir(kDirectoryPath);
	std::error_code ec;
	if(!std::filesystem::exists(dir,ec)){
		// まだ1つも保存していなければディレクトリが無い。何もしない
		return;
	}

	// ディレクトリ内の.jsonファイルを走査し、ファイル名(拡張子除く)をグループ名として読み込む
	for(const auto& entry : std::filesystem::directory_iterator(dir,ec)){
		const std::filesystem::path& filePath = entry.path();
		if(filePath.extension() != ".json"){
			continue;
		}
		LoadFile(filePath.stem().string());
	}
}

// 指定グループを1つJSONから読み込む
void GlobalVariables::LoadFile(const std::string& groupName){
	std::string filePath = kDirectoryPath + groupName + ".json";
	std::ifstream file(filePath);
	if(!file.is_open()){
		return;
	}

	nlohmann::json root;
	try{
		file >> root;
	} catch(...){
		// 壊れたJSON(編集途中など)は無視して現状維持
		return;
	}
	file.close();

	// ルート直下にグループ名のオブジェクトがある想定
	auto itGroup = root.find(groupName);
	if(itGroup == root.end() || !itGroup->is_object()){
		return;
	}

	Group& group = datas_[groupName];

	// 各項目をJSONの型を見て復元する
	for(auto it = itGroup->begin(); it != itGroup->end(); ++it){
		const std::string& key = it.key();
		const nlohmann::json& jv = it.value();

		if(jv.is_array() && jv.size() == 3){
			// [x, y, z] → Vector3
			group.items[key] = Vector3{jv[0].get<float>(), jv[1].get<float>(), jv[2].get<float>()};
		} else if(jv.is_boolean()){
			group.items[key] = jv.get<bool>();
		} else if(jv.is_number_integer()){
			// 既存が float なら float として保持(手編集で "3" と書かれても型を維持)
			auto existing = group.items.find(key);
			if(existing != group.items.end() && std::holds_alternative<float>(existing->second)){
				group.items[key] = jv.get<float>();
			} else{
				group.items[key] = jv.get<int32_t>();
			}
		} else if(jv.is_number_float()){
			group.items[key] = jv.get<float>();
		}
	}

	// 監視用に、このファイルの更新時刻を記録
	std::error_code ec;
	auto writeTime = std::filesystem::last_write_time(filePath,ec);
	if(!ec){
		group.lastWriteTime = writeTime;
		group.hasWriteTime = true;
	}
}

// 指定グループをJSONへ保存する
void GlobalVariables::SaveFile(const std::string& groupName){
	auto itGroup = datas_.find(groupName);
	if(itGroup == datas_.end()){
		return;
	}

	// ルート直下に groupName のオブジェクトを作り、その中に全項目を書く
	nlohmann::json root;
	root[groupName] = nlohmann::json::object();

	for(const auto& [key, item] : itGroup->second.items){
		if(std::holds_alternative<int32_t>(item)){
			root[groupName][key] = std::get<int32_t>(item);
		} else if(std::holds_alternative<float>(item)){
			root[groupName][key] = std::get<float>(item);
		} else if(std::holds_alternative<bool>(item)){
			root[groupName][key] = std::get<bool>(item);
		} else if(std::holds_alternative<Vector3>(item)){
			const Vector3& v = std::get<Vector3>(item);
			root[groupName][key] = {v.x, v.y, v.z};
		}
	}

	// 保存先フォルダが無ければ作成
	std::error_code ec;
	std::filesystem::create_directories(kDirectoryPath,ec);

	std::string filePath = kDirectoryPath + groupName + ".json";
	std::ofstream file(filePath);
	if(!file.is_open()){
		return;
	}
	file << std::setw(4) << root << std::endl;
	file.close();

	// 自分の書き込みで監視が反応しないよう、更新時刻を記録しておく
	auto writeTime = std::filesystem::last_write_time(filePath,ec);
	if(!ec){
		itGroup->second.lastWriteTime = writeTime;
		itGroup->second.hasWriteTime = true;
	}
}

// デバッグ時のみ:各グループのJSONの更新時刻を監視し、変わっていたら読み直す
void GlobalVariables::CheckFileUpdate(){
	for(auto& [groupName, group] : datas_){
		std::string filePath = kDirectoryPath + groupName + ".json";

		std::error_code ec;
		if(!std::filesystem::exists(filePath,ec)){
			continue;
		}
		auto writeTime = std::filesystem::last_write_time(filePath,ec);
		if(ec){
			continue;
		}

		// 更新時刻が変わっていたら外部で書き換えられたとみなして読み直す
		if(!group.hasWriteTime || writeTime != group.lastWriteTime){
			LoadFile(groupName); // 内部で lastWriteTime も更新される
		}
	}
}
