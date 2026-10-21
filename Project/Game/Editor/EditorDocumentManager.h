#pragma once
#include <algorithm>
#include <vector>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

/// <summary>
/// Undo/Redoと保存の対象になるエディターの共通の窓口
/// レール・的・敵のエディターがそれぞれ実装し、EditorDocumentManagerに登録する。
/// </summary>
class IEditorDocument{
public:
	virtual ~IEditorDocument() = default;

	// 未保存の一覧に出す名前
	virtual const char* GetDocumentName() const = 0;
	// 1つ前の状態に戻す(戻せる履歴が無ければfalse)
	virtual bool UndoEdit() = 0;
	// Undoで戻した状態をやり直す(やり直せる履歴が無ければfalse)
	virtual bool RedoEdit() = 0;
	// JSONファイルに保存する
	virtual void SaveToJson() = 0;
	// 保存していない変更があるか
	virtual bool IsModified() const = 0;
};

/// <summary>
/// 全エディターの編集の順番と保存をまとめて管理するシングルトン(ヘッダーオンリー)
///
/// 各エディターは自分の編集履歴(EditHistory)を持ち、編集を確定するたびにRecordEdit()で「どのエディターを編集したか」を記録する。
/// Ctrl+Zでは記録の新しい順にエディターを選んで戻すため、レール・的・敵をまたいで編集した順に戻せる。
/// </summary>
class EditorDocumentManager{
public:
	static EditorDocumentManager* GetInstance(){
		static EditorDocumentManager instance;
		return &instance;
	}

	// エディターを登録する(エディターの初期化時に呼ぶ)
	void Register(IEditorDocument* document){
		if(std::find(documents_.begin(),documents_.end(),document) == documents_.end()){
			documents_.push_back(document);
		}
	}

	// エディターの登録を外す(エディターの破棄時に呼ぶ。シーン切り替え後に消えたエディターを指したままにしないため)
	void Unregister(IEditorDocument* document){
		std::erase(documents_,document);
		std::erase(undoOrder_,document);
		std::erase(redoOrder_,document);
	}

	// documentの編集を確定したことを記録する(新しく編集したので、やり直しの記録は消す)
	void RecordEdit(IEditorDocument* document){
		undoOrder_.push_back(document);
		// 各エディターの履歴の上限を合わせた数より多くは戻せないため、古い記録から捨てる
		if(undoOrder_.size() > kMaxOrderCount){
			undoOrder_.erase(undoOrder_.begin());
		}
		redoOrder_.clear();
	}

	// 最後に編集したエディターを1つ前の状態に戻す
	void Undo(){
		// エディター側で履歴の上限を超えて捨てた分は戻せないため、戻せるエディターが見つかるまで記録をさかのぼる
		while(!undoOrder_.empty()){
			IEditorDocument* document = undoOrder_.back();
			undoOrder_.pop_back();
			if(document->UndoEdit()){
				redoOrder_.push_back(document);
				return;
			}
		}
	}

	// 最後にUndoしたエディターをやり直す
	void Redo(){
		while(!redoOrder_.empty()){
			IEditorDocument* document = redoOrder_.back();
			redoOrder_.pop_back();
			if(document->RedoEdit()){
				undoOrder_.push_back(document);
				return;
			}
		}
	}

	bool CanUndo() const{ return !undoOrder_.empty(); }
	bool CanRedo() const{ return !redoOrder_.empty(); }

	// 保存していない変更があるエディターをすべて保存する
	void SaveAll(){
		for(IEditorDocument* document : documents_){
			if(document->IsModified()){
				document->SaveToJson();
			}
		}
	}

	// 登録されている全エディターを取得(未保存の一覧の表示用)
	const std::vector<IEditorDocument*>& GetDocuments() const{ return documents_; }

#ifdef USE_IMGUI
	// Ctrl+Z(Undo)・Ctrl+Y / Ctrl+Shift+Z(Redo)・Ctrl+S(すべて保存)のショートカットを処理する
	// 文字入力中や、ImGuiの操作中・マウスでドラッグ中は、編集の途中で状態が変わらないよう受け付けない
	void HandleShortcuts(){
		const ImGuiIO& io = ImGui::GetIO();
		if(io.WantTextInput || ImGui::IsAnyItemActive() || ImGui::IsMouseDown(ImGuiMouseButton_Left)){
			return;
		}

		if(ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)){
			Undo();
		} else if(ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y) || ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)){
			Redo();
		} else if(ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)){
			SaveAll();
		}
	}
#endif

private:
	EditorDocumentManager() = default;
	~EditorDocumentManager() = default;
	EditorDocumentManager(const EditorDocumentManager&) = delete;
	EditorDocumentManager& operator=(const EditorDocumentManager&) = delete;

	std::vector<IEditorDocument*> documents_; // 登録されているエディター
	std::vector<IEditorDocument*> undoOrder_; // 編集を確定したエディターの順番(末尾ほど新しい)
	std::vector<IEditorDocument*> redoOrder_; // Undoしたエディターの順番(末尾ほど新しい)

	// 編集の順番を覚えておく最大数(レール・的・敵の3つのエディターの履歴の上限を合わせた数)
	static constexpr size_t kMaxOrderCount = 300;
};
