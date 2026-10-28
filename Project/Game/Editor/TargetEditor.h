#pragma once
#include "MyMath.h"
#include "EditHistory.h"
#include "EditorDocumentManager.h"
#include <functional>
#include <optional>
#include <vector>

/// <summary>
/// 的の配置エディター
///
/// 配置モード(Placement Mode)をONにすると、Sceneビュー上でマウスドラッグにより的を移動できる。
/// 配置内容はJSONファイルへの保存・再読込に対応しており、シーン側は本クラスの座標を参照して的を生成する。
/// </summary>
class TargetEditor : public IEditorDocument{
public:
	// 配置された的1個分のデータ
	struct TargetPoint{
		Vector3 position; // 的のワールド座標
		// 壊すと出るレールのインデックス(-1: 鍵の的ではない。同じレールを指定した的をすべて壊すとレールが出る)
		int unlockRailIndex = -1;

		// 編集履歴で、変更があったかを比べるのに使う
		bool operator==(const TargetPoint&) const = default;
	};

	// 下段の選択パネルで選ぶときの、このエディターのパネル名
	static constexpr const char* kPanelName = "Target Editor";

	TargetEditor();
	~TargetEditor() override;

	// 初期化処理(保存済みJSONがあれば読み込む)
	void Initialize();
	// 更新処理(ImGuiでの編集UIとマウスドラッグによる移動)
	void Update();

	// 配置されている的の個数を取得
	int GetTargetCount() const;
	// 指定インデックスの的の座標を取得
	Vector3 GetTargetPosition(int index) const;
	// 指定インデックスの的を壊すと出るレールのインデックスを取得(-1: 鍵の的ではない)
	int GetUnlockRailIndex(int index) const;

	// 指定座標に的を1個追加する(初回起動時の初期配置生成用)
	void AddTargetAt(const Vector3& position);

	// 選択中の的のインデックスを取得(-1は未選択)
	int GetSelectedIndex() const;
	// 選択中の的のインデックスを設定する(-1で選択解除)
	void SetSelectedIndex(int index);

	// 「Focus」の操作があれば、選択中の的の位置を返す(1回読むと消える)
	bool ConsumeFocusRequest(Vector3& outPosition);

	// 選択中の的の編集欄の下に、配置データ以外の項目(生存フラグなど)のUIを足す(引数は選択中の的のインデックス)
	void SetSelectedTargetExtraUI(std::function<void(int)> drawer);

	// 配置内容をJSONファイルに保存する
	void SaveToJson() override;
	// 配置内容をJSONファイルから読み込む
	void LoadFromJson();

	// 未保存の一覧に出す名前
	const char* GetDocumentName() const override{ return "Target"; }
	// 1つ前の状態に戻す(Ctrl+Z)
	bool UndoEdit() override;
	// Undoで戻した状態をやり直す(Ctrl+Y)
	bool RedoEdit() override;
	// 保存していない変更があるか
	bool IsModified() const override{ return history_.IsModified(); }

private:
	// マウス位置から画面奥へ伸びるレイ
	struct PickRay{
		Vector3 origin;    // レイの始点(ニアクリップ面上の点)
		Vector3 direction; // レイの正規化された方向
	};

	// 現在のカメラの前方に的を1個追加する(Add Targetボタン用)
	void AddTargetInFrontOfCamera();
	// 選択中の的を、カメラの右方向へ少しずらして複製する(Duplicateボタン・Ctrl+D用)
	void DuplicateSelectedTarget();
	// 選択中の的を削除する(Delete Targetボタン・Deleteキー用)
	void DeleteSelectedTarget();
	// 選択中の的があるか
	bool HasSelection() const;
	// 選択中の的の座標・壊すと出るレールを編集するUIを表示する
	void DrawSelectedTargetUI();
	// 配置されている的の一覧を表示する(クリックで選択、ダブルクリックでFocus)
	void DrawTargetList();
	// Sceneビュー操作中のキーボードショートカット(Delete・Ctrl+D・F)の処理
	void UpdateShortcuts();
	// スナップがONなら、座標をグリッドに合わせる(高さを保つ移動中は高さはそのままにする)
	Vector3 ApplySnap(const Vector3& position,bool keepsHeight) const;
	// Editモード中にゲーム画面が実際に描かれている矩形(レターボックス適用後)を求める
	bool ComputeGameViewportRect(float& outX,float& outY,float& outW,float& outH) const;
	// 現在のマウス位置から伸びるレイを求める(画面外・カメラ未取得のときはfalse)
	bool ComputeMouseRay(PickRay& outRay) const;
	// レイに最も近い的のインデックスを求める(掴める距離に無ければ-1)
	int PickTargetIndex(const PickRay& ray) const;
	// マウスドラッグによる的の移動処理
	void UpdateMouseDrag();
	// 指定の的を掴んで、マウスドラッグでの移動を始める
	void StartDrag(int index,const PickRay& ray);
	// 選んでいないときに、Sceneビューで的をクリックしたかを調べ、掴めたらTarget Editorへの切り替えの候補として報告する
	void ReportScenePickWhileInactive();
	// 配置内容をJSONファイルから読み込む(編集履歴には触れない。起動時とLoadFromJsonで使う。ファイルが無ければfalse)
	bool LoadTargetsFromFile();
	// 配置内容が、最後に確定した状態から変わっていれば確定して履歴に積む
	void CommitEditIfChanged();

	std::vector<TargetPoint> targets_; // 配置されている的の配列

	// 編集履歴(Undo/Redo)と、保存していない変更があるかの管理
	EditHistory<std::vector<TargetPoint>> history_;
	// シーンが切り替わってエディターが破棄されたときに、保存していなかった変更を取っておく場所(次に作ったエディターで戻す)
	// クリア・ゲームオーバーなどでシーンが切り替わると、エディターは作り直されてファイルから読み込み直すため、取っておかないと消えてしまう
	// ファイルには書き込まず、戻した後も「未保存」のままにして、保存するか(Save)捨てるか(Load・Ctrl+Z)を選べるようにする
	static inline std::optional<std::vector<TargetPoint>> unsavedStash_;

	int selectedIndex_ = -1;      // 選択中の的のインデックス(-1: 未選択)
	bool isPlacementMode_ = true; // 配置モード(ONの間だけマウスドラッグでの移動を受け付ける)
	bool keepsHeightWhileDragging_ = false; // ONなら高さを変えずに水平方向だけ動かす(高さはShift+ドラッグかPositionのYで調整する)
	bool isSnapEnabled_ = false;            // ONならドラッグで動かした座標をグリッドに合わせる
	float snapSize_ = kDefaultSnapSize;     // スナップのグリッドの間隔
	bool hasFocusRequest_ = false;          // 「Focus」が押されたか(GameSceneが読んで俯瞰デバッグカメラを寄せる)
	int pendingPickIndex_ = -1;             // 選んでいないときにSceneビューでクリックした的(切り替わった次のフレームで選ぶ。-1: 無し)
	std::function<void(int)> selectedTargetExtraUI_; // 選択中の的の編集欄の下に足すUI(無ければ何も足さない)

	// マウスドラッグ中の状態
	bool isDragging_ = false;                        // ドラッグ中かどうか
	bool isDraggingHeightOnly_ = false;              // 高さだけを動かすドラッグか(Shiftを押しながら掴んだとき)
	bool isDraggingKeepsHeight_ = false;             // 高さを保つドラッグか(掴んだ瞬間のKeep Heightの状態)
	Vector3 dragPlaneNormal_ = {0.0f, 0.0f, 1.0f};   // ドラッグ面の法線(掴んだ瞬間のレイ方向)
	Vector3 dragPlanePoint_ = {0.0f, 0.0f, 0.0f};    // ドラッグ面が通る点(掴んだ瞬間の的の座標)
	Vector3 dragOffset_ = {0.0f, 0.0f, 0.0f};        // 掴んだ位置と的の座標とのずれ(掴み直しの跳びを防ぐ)

	// 新規追加時にカメラ前方へ置く距離
	static constexpr float kSpawnDistance = 15.0f;
	// マウスで的を掴めるとみなす、レイから的までの距離
	static constexpr float kPickRadius = 1.0f;
	// 複製した的を元の的からずらす距離(カメラの右方向へずらし、重なって見分けられなくなるのを防ぐ)
	static constexpr float kDuplicateOffsetDistance = 1.5f;
	// スナップのグリッドの間隔の初期値と下限(0に近いと割り算が不安定になるため下限を設ける)
	static constexpr float kDefaultSnapSize = 0.5f;
	static constexpr float kMinSnapSize = 0.05f;
	// スナップの間隔をドラッグで変えるときの変化量
	static constexpr float kSnapSizeDragSpeed = 0.05f;
	// スナップの間隔の入力欄の幅(文字の大きさの何倍か。チェックボックスの横に収まる幅にする)
	static constexpr float kSnapSizeFieldWidthInFonts = 4.0f;
	// パネルのPositionをドラッグで変えるときの変化量
	static constexpr float kPositionDragSpeed = 0.1f;
	// 壊すと出るレールのインデックスの下限(-1: 鍵の的ではない)
	static constexpr int kNoUnlockRailIndex = -1;
	// 的の一覧を表示する子ウィンドウの高さ(行数が増えても他のUIが押し出されないよう、高さを決めてスクロールさせる)
	static constexpr float kTargetListHeight = 100.0f;
};
