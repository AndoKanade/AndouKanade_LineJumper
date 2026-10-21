#pragma once
#include <cstdint>
#include <deque>
#include <vector>

/// <summary>
/// エディターの編集履歴(Undo/Redo)と、保存済みかどうかを管理するクラス(ヘッダーオンリー)
///
/// 編集が確定するたびに、確定前の状態を履歴に積む。状態ごとに番号を振っておき、
/// 保存したときの番号と今の番号を比べて、保存していない変更があるかを判定する
/// (Undoで保存したときの状態まで戻れば、未保存の表示も消える)。
/// </summary>
template<class T>
class EditHistory{
public:
	// 履歴を消し、stateを編集の基準(保存済み)の状態にする(起動時の読み込み直後に呼ぶ)
	void Reset(const T& state){
		undoStack_.clear();
		redoStack_.clear();
		committed_ = {state, nextVersion_++};
		savedVersion_ = committed_.version;
	}

	// 最後に確定した状態を取得(今の状態と比べて、変更があったかを判定するのに使う)
	const T& GetCommitted() const{ return committed_.state; }

	// 新しい状態を確定し、確定前の状態を履歴に積む(新しく編集したので、やり直しの履歴は消す)
	void Commit(const T& state){
		undoStack_.push_back(committed_);
		// 古い履歴から捨て、履歴が増え続けないようにする
		if(undoStack_.size() > kMaxHistoryCount){
			undoStack_.pop_front();
		}
		redoStack_.clear();
		committed_ = {state, nextVersion_++};
	}

	// 1つ前の状態に戻し、戻した状態をoutStateに入れる(戻せる履歴が無ければfalse)
	bool Undo(T& outState){
		if(undoStack_.empty()){
			return false;
		}
		redoStack_.push_back(committed_);
		committed_ = undoStack_.back();
		undoStack_.pop_back();
		outState = committed_.state;
		return true;
	}

	// Undoで戻した状態をやり直し、やり直した状態をoutStateに入れる(やり直せる履歴が無ければfalse)
	bool Redo(T& outState){
		if(redoStack_.empty()){
			return false;
		}
		undoStack_.push_back(committed_);
		committed_ = redoStack_.back();
		redoStack_.pop_back();
		outState = committed_.state;
		return true;
	}

	// 最後に確定した状態を保存済みとして記録する(保存・読み込みの直後に呼ぶ)
	void MarkSaved(){ savedVersion_ = committed_.version; }
	// 保存していない変更があるか
	bool IsModified() const{ return committed_.version != savedVersion_; }

private:
	// 履歴1つ分(状態と、状態ごとに振った番号)
	struct Entry{
		T state{};
		uint64_t version = 0;
	};

	std::deque<Entry> undoStack_;  // Undoで戻る先の状態(末尾ほど新しい)
	std::vector<Entry> redoStack_; // Redoでやり直す先の状態(末尾ほど新しい)
	Entry committed_{};            // 最後に確定した状態

	uint64_t nextVersion_ = 0;     // 次に確定する状態に振る番号
	uint64_t savedVersion_ = 0;    // 保存したときの状態の番号

	// 1つのエディターで覚えておく履歴の最大数
	static constexpr size_t kMaxHistoryCount = 100;
};
