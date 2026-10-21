#pragma once
#include "MyMath.h"
#include "EditHistory.h"
#include "EditorDocumentManager.h"
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

		// 編集履歴で、変更があったかを比べるのに使う
		bool operator==(const TargetPoint&) const = default;
	};

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
	// 指定インデックスの的の座標への参照を取得(Inspectorからの編集用)
	Vector3& GetTargetPositionRef(int index);

	// 指定座標に的を1個追加する(初回起動時の初期配置生成用)
	void AddTargetAt(const Vector3& position);

	// 選択中の的のインデックスを取得(-1は未選択)
	int GetSelectedIndex() const;
	// 選択中の的のインデックスを設定する(-1で選択解除)
	void SetSelectedIndex(int index);

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
	// Editモード中にゲーム画面が実際に描かれている矩形(レターボックス適用後)を求める
	bool ComputeGameViewportRect(float& outX,float& outY,float& outW,float& outH) const;
	// 現在のマウス位置から伸びるレイを求める(画面外・カメラ未取得のときはfalse)
	bool ComputeMouseRay(PickRay& outRay) const;
	// レイに最も近い的のインデックスを求める(掴める距離に無ければ-1)
	int PickTargetIndex(const PickRay& ray) const;
	// マウスドラッグによる的の移動処理
	void UpdateMouseDrag();
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

	// マウスドラッグ中の状態
	bool isDragging_ = false;                        // ドラッグ中かどうか
	Vector3 dragPlaneNormal_ = {0.0f, 0.0f, 1.0f};   // ドラッグ面の法線(掴んだ瞬間のレイ方向)
	Vector3 dragPlanePoint_ = {0.0f, 0.0f, 0.0f};    // ドラッグ面が通る点(掴んだ瞬間の的の座標)
	Vector3 dragOffset_ = {0.0f, 0.0f, 0.0f};        // 掴んだ位置と的の座標とのずれ(掴み直しの跳びを防ぐ)

	// 新規追加時にカメラ前方へ置く距離
	static constexpr float kSpawnDistance = 15.0f;
	// マウスで的を掴めるとみなす、レイから的までの距離
	static constexpr float kPickRadius = 1.0f;
};
