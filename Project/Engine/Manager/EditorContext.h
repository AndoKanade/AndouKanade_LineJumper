#pragma once
#include <string>

/// <summary>
/// エディタの状態を保持するシングルトン(ヘッダーオンリー)
///
/// Unityの「Sceneビュー(編集) ↔ Gameビュー(実行)」のような2モードを管理する。
///  ・Editモード: ゲーム画面を中央に縮小表示し、まわりに編集パネルを並べて配置/パラメータを調整する
///  ・Playモード: 縮小をやめて全画面表示にし、編集パネルやギズモを隠したクリーンな実行画面にする
///
/// 描画側(Application)・パネル側(各シーン/エディタ)から IsPlayMode() を参照して振る舞いを切り替える。
/// </summary>
// ピクセル単位の矩形(Sceneパネルの領域を描画側へ渡すのに使う)
struct EditorRect{
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;
};

class EditorContext{
public:
	static EditorContext* GetInstance(){
		static EditorContext instance;
		return &instance;
	}

	// 実行(Play)モードかどうか
	bool IsPlayMode() const{ return isPlayMode_; }
	void SetPlayMode(bool play){ isPlayMode_ = play; }
	void TogglePlayMode(){ isPlayMode_ = !isPlayMode_; }

	// Editモードでゲーム画面を縮小表示する倍率(1.0で原寸)。※Sceneパネル方式では未使用だが将来用に残す
	float GetEditViewScale() const{ return editViewScale_; }
	void SetEditViewScale(float scale){ editViewScale_ = scale; }

	// 中央「Scene」パネルの内側領域(ピクセル)。ImGui側が毎フレーム設定し、描画側(Application)が
	// ゲームのビューポートをこの矩形に合わせる。w/hが0のときは「Sceneパネル無し=全画面」とみなす。
	const EditorRect& GetSceneViewRect() const{ return sceneViewRect_; }
	void SetSceneViewRect(float x,float y,float w,float h){ sceneViewRect_ = {x, y, w, h}; }
	void ClearSceneViewRect(){ sceneViewRect_ = {}; }

	// 右の列に出して操作するエディターのパネル名(下段のエディター選択パネルで切り替える)
	// 選ばれていないエディターはパネルを出さず、Sceneビューでのドラッグ・ショートカットも受け付けない
	const std::string& GetActiveToolPanel() const{ return activeToolPanel_; }
	void SetActiveToolPanel(const std::string& panelName){ activeToolPanel_ = panelName; }
	bool IsActiveToolPanel(const char* panelName) const{ return activeToolPanel_ == panelName; }

	// Sceneビューのクリックで物を掴めたエディターを報告する(選んでいないエディターへ自動で切り替えるのに使う)
	// 選んでいるエディターが掴めたときは切り替えないよう覚えておき、ほかはカメラに一番近い物を掴んだエディターを候補にする
	void ReportScenePick(const char* panelName,float distance){
		if(IsActiveToolPanel(panelName)){
			isActivePanelPicked_ = true;
			return;
		}
		if(pickedPanel_.empty() || distance < pickedDistance_){
			pickedPanel_ = panelName;
			pickedDistance_ = distance;
		}
	}
	// このフレームの報告から切り替え先を決めて切り替える(選んでいるエディターが掴めていれば切り替えない)
	// 報告は毎フレーム消す。切り替えたときtrue
	bool ResolveScenePicks(){
		bool isSwitched = false;
		if(!isActivePanelPicked_ && !pickedPanel_.empty()){
			activeToolPanel_ = pickedPanel_;
			isSwitched = true;
		}
		isActivePanelPicked_ = false;
		pickedPanel_.clear();
		return isSwitched;
	}

private:
	EditorContext() = default;
	~EditorContext() = default;
	EditorContext(const EditorContext&) = delete;
	EditorContext& operator=(const EditorContext&) = delete;

#ifdef USE_IMGUI
	bool isPlayMode_ = false;     // 起動時は編集モード
#else
	bool isPlayMode_ = true;      // ImGuiが無い(Release)ときは編集UIを使えないため、起動時から実行モードにする
#endif
	float editViewScale_ = 0.6f;  // 編集モードの縮小率(将来用)
	EditorRect sceneViewRect_{};  // 中央Sceneパネルの領域
	std::string activeToolPanel_; // 右の列に出しているエディターのパネル名(空なら選択パネルが先頭のエディターを選ぶ)

	// このフレームのSceneビューのクリックで物を掴めたエディターの報告
	bool isActivePanelPicked_ = false; // 選んでいるエディターが掴めたか
	std::string pickedPanel_;          // 選んでいないエディターのうち、一番手前の物を掴んだエディターのパネル名(空なら無し)
	float pickedDistance_ = 0.0f;      // pickedPanel_ が掴んだ物までの、レイに沿った距離
};
