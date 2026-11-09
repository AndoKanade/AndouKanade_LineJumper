#pragma once
#include "MyMath.h"
#include "EditHistory.h"
#include "EditorDocumentManager.h"
#include "objects/StageInfo.h"
#include <array>
#include <optional>
#include <vector>

/// <summary>
/// 敵の配置エディター
///
/// 配置モード(Placement Mode)をONにすると、Sceneビュー上でマウスドラッグにより敵を移動できる。
/// 敵ごとに往復移動の方向と体力の最大値も設定でき、配置内容はJSONファイルへの保存・再読込に対応している。
/// シーン側は本クラスのデータを参照して敵を生成する。
/// </summary>
class EnemyEditor : public IEditorDocument{
public:
	// 配置された敵1体分のデータ
	struct EnemyPoint{
		Vector3 position;        // 敵のワールド座標(往復移動の中心)
		Vector3 patrolDirection; // 往復移動を行う方向
		int maxHp;               // 体力の最大値

		// 編集履歴で、変更があったかを比べるのに使う
		bool operator==(const EnemyPoint&) const = default;
	};

	// 体力の最大値の初期値(Add Enemyで追加したときに設定される値)
	static constexpr int kDefaultMaxHp = 3;
	// 体力の最大値として設定できる下限(0以下にすると生成直後に撃破されてしまうため)
	static constexpr int kMinMaxHp = 1;

	// 下段の選択パネルで選ぶときの、このエディターのパネル名
	static constexpr const char* kPanelName = "Enemy Editor";

	EnemyEditor();
	~EnemyEditor() override;

	// 初期化処理(保存済みJSONがあれば読み込む)
	void Initialize();
	// 更新処理(ImGuiでの編集UIとマウスドラッグによる移動)
	void Update();

	// 配置されている敵の体数を取得
	int GetEnemyCount() const;
	// 指定インデックスの敵の配置データを取得(範囲外のときは既定値を返す)
	EnemyPoint GetEnemyPoint(int index) const;

	// 指定の内容で敵を1体追加する(初回起動時の初期配置生成用)
	void AddEnemyAt(const Vector3& position,const Vector3& patrolDirection,int maxHp);

	// 選択中の敵のインデックスを取得(-1は未選択)
	int GetSelectedIndex() const;
	// 選択中の敵のインデックスを設定する(-1で選択解除)
	void SetSelectedIndex(int index);

	// 編集・プレイするステージを切り替える(ステージごとの配置はすべて読み込んであり、切り替えても保存していない変更は消えない)
	void SetStageIndex(int stageIndex);
	// 今編集・プレイしているステージの番号を取得
	int GetStageIndex() const{ return stageIndex_; }

	// 全ステージの配置内容をJSONファイルに保存する
	void SaveToJson() override;
	// 全ステージの配置内容をJSONファイルから読み込む
	void LoadFromJson();

	// 未保存の一覧に出す名前
	const char* GetDocumentName() const override{ return "Enemy"; }
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

	// 現在のカメラの前方に敵を1体追加する(Add Enemyボタン用)
	void AddEnemyInFrontOfCamera();
	// 選択中の敵のパラメータ(座標・往復方向・体力)を編集するUIを表示する
	void DrawSelectedEnemyUI();
	// Editモード中にゲーム画面が実際に描かれている矩形(レターボックス適用後)を求める
	bool ComputeGameViewportRect(float& outX,float& outY,float& outW,float& outH) const;
	// 現在のマウス位置から伸びるレイを求める(画面外・カメラ未取得のときはfalse)
	bool ComputeMouseRay(PickRay& outRay) const;
	// レイに最も近い敵のインデックスを求める(掴める距離に無ければ-1)
	int PickEnemyIndex(const PickRay& ray) const;
	// マウスドラッグによる敵の移動処理
	void UpdateMouseDrag();
	// 選んでいないときに、Sceneビューで敵をクリックしたかを調べ、掴めたらEnemy Editorへの切り替えの候補として報告する
	void ReportScenePickWhileInactive();
	// 編集履歴に残す、全ステージ分の配置(並びはステージの番号と同じ)
	using StagesSnapshot = std::array<std::vector<EnemyPoint>,StageInfo::kStageCount>;

	// 指定ステージの配置の配列を取得(編集中のステージはenemies_、それ以外は預けてある配列)
	std::vector<EnemyPoint>& GetStageEnemies(int stageIndex);
	const std::vector<EnemyPoint>& GetStageEnemies(int stageIndex) const;
	// 指定ステージの配置の保存先ファイルパスを求める(resource/stage/stage1/enemy.json など)
	std::string GetSaveFilePath(int stageIndex) const;
	// 今の全ステージの配置を編集履歴に残す形で取得する
	StagesSnapshot MakeSnapshot() const;
	// 編集履歴の状態を全ステージの配置に反映する(Undo/Redo用)
	void ApplySnapshot(const StagesSnapshot& snapshot);
	// 指定ステージの配置をJSONファイルから読み込む(編集履歴には触れない。起動時とLoadFromJsonで使う。ファイルが無ければfalse)
	bool LoadEnemiesFromFile(int stageIndex);
	// 配置内容が、最後に確定した状態から変わっていれば確定して履歴に積む
	void CommitEditIfChanged();

	// 編集・プレイしているステージに配置されている敵の配列
	// ほかの処理はすべてこの配列を見るため、ステージを切り替えるときは中身をstoredStageEnemies_と入れ替える
	std::vector<EnemyPoint> enemies_;
	// 編集・プレイしていないステージの配置を預けておく場所(並びはステージの番号と同じ。編集中のステージの分は空)
	StagesSnapshot storedStageEnemies_;
	int stageIndex_ = 0; // 編集・プレイしているステージの番号

	// 編集履歴(Undo/Redo)と、保存していない変更があるかの管理
	EditHistory<StagesSnapshot> history_;
	// シーンが切り替わってエディターが破棄されたときに、保存していなかった変更を取っておく場所(次に作ったエディターで戻す)
	// クリア・ゲームオーバーなどでシーンが切り替わると、エディターは作り直されてファイルから読み込み直すため、取っておかないと消えてしまう
	// ファイルには書き込まず、戻した後も「未保存」のままにして、保存するか(Save)捨てるか(Load・Ctrl+Z)を選べるようにする
	static inline std::optional<StagesSnapshot> unsavedStash_;

	int selectedIndex_ = -1;      // 選択中の敵のインデックス(-1: 未選択)
	int pendingPickIndex_ = -1;   // 選んでいないときにSceneビューでクリックした敵(切り替わった次のフレームで選ぶ。-1: 無し)
	bool isPlacementMode_ = true; // 配置モード(ONの間だけマウスドラッグでの移動を受け付ける)

	// マウスドラッグ中の状態
	bool isDragging_ = false;                        // ドラッグ中かどうか
	Vector3 dragPlaneNormal_ = {0.0f, 0.0f, 1.0f};   // ドラッグ面の法線(掴んだ瞬間のレイ方向)
	Vector3 dragPlanePoint_ = {0.0f, 0.0f, 0.0f};    // ドラッグ面が通る点(掴んだ瞬間の敵の座標)
	Vector3 dragOffset_ = {0.0f, 0.0f, 0.0f};        // 掴んだ位置と敵の座標とのずれ(掴み直しの跳びを防ぐ)

	// 新規追加時にカメラ前方へ置く距離
	static constexpr float kSpawnDistance = 15.0f;
	// マウスで敵を掴めるとみなす、レイから敵までの距離
	static constexpr float kPickRadius = 1.0f;
	// 往復移動方向の既定値(Add Enemyで追加したときに設定される方向)
	static constexpr Vector3 kDefaultPatrolDirection = {1.0f, 0.0f, 0.0f};
	// 往復移動方向として成立する最小の長さ(これ未満のときは既定値に戻す)
	static constexpr float kMinPatrolDirectionLength = 1e-4f;
};
