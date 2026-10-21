#pragma once
#include "MyMath.h"
#include "EditHistory.h"
#include "EditorDocumentManager.h"
#include <vector>
#include <memory>
#include <string>

// クラスの前方宣言
class Obj3D;
class Obj3dCommon;

class RailEditor : public IEditorDocument{
public:
	// レールの制御点を表す構造体
	struct ControlPoint{
		Vector3 position; // 制御点の3D座標
		Vector3 rotation; // 通過時のカメラの角度
		float speed;      // 次の点への移動速度

		// レール間分岐移動用のデータ
		int branchTargetRailIndex = -1;  // 分岐先レールのインデックス(-1: 分岐なし)
		int branchTargetPointIndex = -1; // 分岐先レール側の対応する制御点インデックス

		// 編集履歴で、変更があったかを比べるのに使う
		bool operator==(const ControlPoint&) const = default;
	};

	// 着地判定(オフレール時、全レール中から最も近いレールを探す)用の結果
	struct NearestRailResult{
		int railIndex = -1;    // 最も近いレールのインデックス(レールが1本も無い場合は-1)
		float t = 0.0f;        // そのレール上での進行度(0〜1)
		float distance = 0.0f; // そのレールまでの距離

		// 最近傍点のワールド座標
		// 着地判定で水平距離と高さを別々に見るために使う(呼び出し側での再計算を避けるため、探索時の値をそのまま返す)
		Vector3 position = {0.0f, 0.0f, 0.0f};
	};

	RailEditor();
	~RailEditor() override;

	// 初期化処理
	void Initialize(Obj3dCommon* objCommon);
	// 更新処理（ImGuiでの編集など）
	void Update();
	// デバッグ用の描画処理
	void Draw();

	// レール上の座標を取得 (t: 0〜1)。対象はアクティブなレール
	Vector3 GetPositionOnRail(float t) const;
	// レール上の回転を取得 (t: 0〜1)。対象はアクティブなレール
	Vector3 GetRotationOnRail(float t) const;
	// レール上の正規化された進行方向を取得 (t: 0〜1)。対象はアクティブなレール
	Vector3 GetForwardOnRail(float t) const;
	// レール上の速度(各制御点のSpeed値を補間したもの)を取得 (t: 0〜1)。対象はアクティブなレール
	float GetSpeedOnRail(float t) const;
	// 進行度tの位置からレールに沿ってdistance(ワールド座標の距離)だけ進んだ先の進行度を取得。対象はアクティブなレール
	// 制御点間の距離に関係なく一定の速さで進めるために使う
	float AdvanceTByDistance(float t,float distance) const;

	// 全レールの中から、指定したワールド座標に最も近いレール・進行度・距離を求める(着地先レールの探索用)
	// excludeRailIndexに指定したレールは探索対象から外す(-1: 除外なし。並走レールへの乗り換えで今のレールを外すのに使う)
	NearestRailResult FindNearestRail(const Vector3& worldPos,int excludeRailIndex = -1) const;

	// 指定インデックスのレールをアクティブなレールとして切り替える(Playモードでのレール乗り換えにも使用)
	void SwitchActiveRail(int index);
	// 現在アクティブなレールのインデックスを取得(デバッグ表示用)
	int GetActiveRailIndex() const;
	// 読み込まれている全レールの本数を取得(デバッグ表示用)
	int GetRailCount() const;

	// アクティブなレールの制御点数を取得
	int GetControlPointCount() const;
	// 進行度tから、直近に通過した制御点のインデックスを取得(対象はアクティブなレール)
	int GetControlPointIndexFromT(float t) const;
	// アクティブなレールの、指定した制御点を通る位置の進行度を取得(曲線が通らない両端の制御点は、曲線の始点・終点の進行度にする)
	float GetTOfControlPoint(int index) const;

	// 一覧で選択中の制御点のインデックスを取得(-1: 未選択。対象はアクティブなレール)
	int GetSelectedPointIndex() const{ return selectedPointIndex_; }
	// 指定レールをアクティブにして、指定した制御点を選択する(Playから戻ったときに、Play前の編集対象に戻すのに使う)
	void SelectPoint(int railIndex,int pointIndex);

	// 「Focus」で選択中の制御点にカメラを寄せる操作があれば、その位置をoutPositionに入れてtrueを返す(1回読むと消える)
	// カメラはGameSceneが持っているため、寄せる処理はGameScene側で行う
	bool ConsumeFocusRequest(Vector3& outPosition);

	// レールの全長から走り切るまでの秒数を求めるときの、全体の速さ(Playerの調整項目railSpeed)を設定する
	// 調整項目はGameScene側で読むため、同じ値を二重に持たないよう外から渡してもらう
	void SetBaseRailSpeed(float speed){ baseRailSpeed_ = speed; }

	// ImGui非表示時でもキー操作で表示切り替えを行うための関数(対象はアクティブなレール)
	void ToggleShowControlPointModels();
	void ToggleShowCurve();

	// アクティブなレールの、進行度tの位置までをインクで塗った区間に含める(Play中にレールの見た目を塗るのに使う)
	void PaintActiveRail(float t);
	// 全レールの塗った区間を消す(リスタート時に呼ぶ)
	void ClearPaint();

	// 着地できる範囲の描画に使う水平方向の許容距離を設定する
	// 判定値そのものはGameScene側が持っているため、同じ値を二重に持たないよう外から渡してもらう
	void SetLandingRangeRadius(float radius){ landingRangeRadius_ = radius; }

	// アクティブなレールの全制御点の中心座標を取得
	Vector3 GetControlPointsCenter() const;
	// アクティブなレールの全制御点を囲む範囲の半径を取得
	float GetControlPointsRadius() const;

	// 全レールの制御点をそれぞれ対応するJSONファイルに保存する
	void SaveToJson() override;
	// 全レールの制御点をJSONファイルから読み込む(ディスクに存在する分だけ連番でレールを復元する)
	void LoadFromJson();

	// 未保存の一覧に出す名前
	const char* GetDocumentName() const override{ return "Rail"; }
	// 1つ前の状態に戻す(Ctrl+Z)
	bool UndoEdit() override;
	// Undoで戻した状態をやり直す(Ctrl+Y)
	bool RedoEdit() override;
	// 保存していない変更があるか
	bool IsModified() const override{ return history_.IsModified(); }

private:
	// 編集履歴に残す、1本分のレールのデータ(描画用オブジェクトなどを除いた、保存する内容だけ)
	struct RailSnapshot{
		std::string name;
		std::vector<ControlPoint> controlPoints;
	};
	using RailsSnapshot = std::vector<RailSnapshot>;

	// レールの全長と走り切るまでの秒数(目安)
	struct RailStats{
		float length = 0.0f;           // レールの全長
		float time = 0.0f;             // 走り切るまでの秒数
		float lengthToSelected = 0.0f; // レールの始点から選択中の制御点までの長さ
		float timeToSelected = 0.0f;   // レールの始点から選択中の制御点までの秒数
	};

	// 1本分のレールが持つデータ一式(制御点+描画用オブジェクト+表示設定)
	struct Rail{
		std::string name;                                   // レール名(UI表示・保存ファイル名の目印用)
		std::vector<ControlPoint> controlPoints;            // 制御点を保存する配列
		std::vector<std::unique_ptr<Obj3D>> pointObjects;   // 制御点描画用の3Dオブジェクト群

		// Play中にレールの本体として描く管(円柱をつないだもの。区間数に合わせて足りない分だけ増やす)
		std::vector<std::unique_ptr<Obj3D>> tubeObjects;
		// 管の区間ごとに描画するかどうか(短すぎて向きが定まらない区間はfalse。BuildTubes()で求める)
		// 要素数がそのレールで使う管の数になる
		std::vector<bool> tubeSegmentVisible;

		bool showControlPointModels = true;                 // 制御点の球体モデルの描画フラグ
		bool showCurve = true;                              // 曲線表示フラグ

		// インクで塗った区間(進行度の範囲)。乗った位置から進んだ位置までを塗る
		bool isPainted = false;
		float paintStartT = 0.0f;
		float paintEndT = 0.0f;
	};

	// 制御点・曲線の点列・着地範囲の描画用の3Dオブジェクト生成(modelPathで使うモデルを指定する)
	std::unique_ptr<Obj3D> CreatePointObject(const std::string& modelPath);
	// Play中のレール本体の管の位置・向き・大きさを求めて行列を更新する
	// Play中はレールの形が変わらないため、Playに入ってから最初の描画で1回だけ呼ぶ
	void BuildTubes();
	// Play中のレール本体を、塗った区間とそれ以外で色を分けて管として描画する
	void DrawTubes();
	// rail.pointObjectsの個数をrail.controlPointsに同期
	void SyncPointObjectsToControlPoints(Rail& rail);
	// 描画用オブジェクトがcount個に満たなければ、足りない分だけmodelPathのモデルで生成する(多い分は削らず、使わないだけにする)
	void EnsureObjectCount(std::vector<std::unique_ptr<Obj3D>>& objects,size_t count,const std::string& modelPath);
	// 制御点数pointCountのレールで、Edit中の曲線の点列に使う点の数を求める
	size_t GetCurveSampleCount(size_t pointCount) const;
	// Edit中の曲線の点列・着地できる範囲の点を描くオブジェクトを、全レール共通の置き場から1つ取り出す
	// 足りないときは1フレームに作る数を制限しながら増やし、このフレームに作れる数を使い切ったらnullptrを返す(その点は次のフレーム以降に描く)
	Obj3D* AcquireSampleObject();
	// レールの末尾を延長する位置に置く制御点を求める(末尾2点の向きにそのまま伸ばす)
	ControlPoint ComputeExtendedPoint(const std::vector<ControlPoint>& controlPoints) const;
	// 指定インデックスの位置に制御点を挿入し、その制御点を選択状態にする
	void InsertControlPoint(Rail& rail,size_t insertIndex,const ControlPoint& point);
	// 指定した制御点の直後に、その制御点から始まる螺旋(らせん階段のような形)の制御点をまとめて挿入する(形はspiral_の設定で決める)
	void GenerateSpiral(Rail& rail,size_t startIndex);
	// 新しいレールを1本追加し、アクティブなレールとして切り替える
	void AddRail();
	// レールのインデックスから保存先ファイルパスを求める(0番目のみ既存の互換パスを使う)
	std::string GetRailFilePath(int index) const;
	// 進行度tと制御点配列から、レール上の座標を計算する(GetPositionOnRail・全レール描画で共用)
	Vector3 ComputePositionOnRail(const std::vector<ControlPoint>& controlPoints,float t) const;
	// 指定ワールド座標に最も近いレール上の進行度tを計算する(粗いサンプリング+三分探索で絞り込み)
	float ComputeNearestTOnRail(const std::vector<ControlPoint>& controlPoints,const Vector3& worldPos) const;
	// 進行度tから、Catmull-Romの区間番号と区間内の進行度を求める(制御点4つ以上が前提)
	// 位置・ロール・速度で同じ区間分けを使い、制御点に設定した値がその制御点の位置で効くようにする
	void ComputeCurveSegment(size_t pointCount,float t,size_t& outSegment,float& outLocalT) const;
	// 進行度tと制御点配列から、レール上の速度(各制御点のSpeed値を補間したもの)を計算する(GetSpeedOnRail・全長の計算で共用)
	float ComputeSpeedOnRail(const std::vector<ControlPoint>& controlPoints,float t) const;
	// アクティブなレールの全長と走り切るまでの秒数、始点から選択中の制御点までの長さと秒数を求める
	RailStats ComputeActiveRailStats() const;

	// 今のレールの状態を編集履歴に残す形で取得する
	RailsSnapshot MakeSnapshot() const;
	// 編集履歴の状態をレールに反映する(Undo/Redo用。レールの本数が変わっていれば増減させる)
	void ApplySnapshot(const RailsSnapshot& snapshot);
	// 今のレールの状態が、最後に確定した状態から変わっていれば確定して履歴に積む
	void CommitEditIfChanged();

	// Sceneビュー上で制御点を左ドラッグして動かす処理
	void UpdateMouseDrag();

	// 指定インデックスのレール1本だけをJSONファイルに保存する(SaveToJsonの内部実装。書き込めなかったときはfalse)
	bool SaveRailToFile(int index);
	// 指定インデックスのレール1本だけをJSONファイルから読み込む(LoadFromJsonの内部実装)
	void LoadRailFromFile(int index);
	// 全レールをJSONファイルから読み込む(編集履歴には触れない。起動時とLoadFromJsonで使う)
	void LoadRailsFromFiles();

	std::vector<Rail> rails_;       // 全レールの配列
	int activeRailIndex_ = 0;       // 現在編集・使用中のレールのインデックス(このレールだけ黄色で描画する)

	// Hierarchy的な一覧(制御点リスト)で選択中の制御点インデックス(-1: 未選択)
	int selectedPointIndex_ = -1;

	// 着地できる範囲の表示ON/OFF(全レールまとめて切り替える。ImGuiのチェックボックスから操作する)
	bool showLandingRange_ = false;
	// 着地できる範囲の水平方向の許容距離(GameSceneの判定値をSetLandingRangeRadius()で受け取る)
	float landingRangeRadius_ = kDefaultLandingRangeRadius;

	// Play中のレール本体の管の行列を求め終えたか(Edit中はレールが編集されうるためfalseに戻す)
	bool areTubesBuilt_ = false;

	// Edit中の曲線の点列・着地できる範囲の点を描くオブジェクトの置き場(全レール共通)
	// 1つ作るごとにGPUのバッファを2つ確保して重いため、レールごとにまとめて作らず、全レールで使い回す
	std::vector<std::unique_ptr<Obj3D>> sampleObjects_;
	// このフレームのDraw()で取り出したオブジェクトの数(毎フレーム0に戻し、置き場の先頭から順に使う)
	size_t usedSampleObjectCount_ = 0;
	// このフレームに新しく作ったオブジェクトの数(1フレームに作る数を制限して、重さを複数フレームに分ける)
	size_t createdSampleObjectCount_ = 0;

	// 編集履歴(Undo/Redo)と、保存していない変更があるかの管理
	EditHistory<RailsSnapshot> history_;

	// 「Focus」で選択中の制御点にカメラを寄せる操作があったか
	bool hasFocusRequest_ = false;

	// レールの全長から走り切るまでの秒数を求めるときの全体の速さ(GameSceneからSetBaseRailSpeed()で受け取る)
	float baseRailSpeed_ = 0.0f;

	// Sceneビュー上での制御点のドラッグ
	bool isPointDragMode_ = false;                   // ONの間だけSceneビュー上で制御点を掴める(的・敵の配置と同時に掴まないよう初期はOFF)
	bool keepsHeightWhileDragging_ = true;           // ONなら高さを変えずに水平方向だけ動かす(高さはPositionのYで調整する)

	// 螺旋を作るときの設定(ImGuiで変える。作った後の制御点は普通の制御点と同じように保存・編集できる)
	struct SpiralSettings{
		float radius = 8.0f;        // 螺旋の半径(真上から見た円の半径)
		float risePerTurn = 6.0f;   // 1周で上がる高さ(負の値なら下がる)
		float turns = 2.0f;         // 何周するか
		int pointsPerTurn = 8;      // 1周に置く制御点の数(多いほど円に近くなる)
		bool turnsRight = true;     // 右回り(真上から見て時計回り)か
	};
	SpiralSettings spiral_;
	bool isDragging_ = false;                        // ドラッグ中かどうか
	Vector3 dragPlaneNormal_ = {0.0f, 1.0f, 0.0f};   // ドラッグ面の法線
	Vector3 dragPlanePoint_ = {0.0f, 0.0f, 0.0f};    // ドラッグ面が通る点(掴んだ瞬間の制御点の座標)
	Vector3 dragOffset_ = {0.0f, 0.0f, 0.0f};        // 掴んだ位置と制御点の座標とのずれ(掴んだ瞬間に跳ばないようにする)

	Obj3dCommon* objCommon_ = nullptr; // 3Dオブジェクト共通設定へのポインタ

	// 螺旋の設定で使える範囲(半径が小さすぎると急カーブになり、1周の点が少なすぎると円に見えないため)
	static constexpr float kMinSpiralRadius = 1.0f;
	static constexpr float kMinSpiralTurns = 0.25f;
	static constexpr int kMinSpiralPointsPerTurn = 4;
	static constexpr int kMaxSpiralPointsPerTurn = 32;

	// マウスで制御点を掴めるとみなす、レイから制御点までの距離
	static constexpr float kPickRadius = 0.5f;
	// 走り切るまでの秒数を求めるとき、速さがこれ未満の区間は止まっているとみなして数えない(0除算防止)
	static constexpr float kMinStatsSpeed = 1e-4f;

	// 曲線の点列・着地できる範囲の点のオブジェクトを、1フレームに新しく作る最大数
	static constexpr size_t kMaxSampleObjectCreatePerFrame = 8;
	// 起動時に、今のレールに必要な数より多めに作っておくオブジェクトの数(制御点を足して延長しても、しばらくは新しく作らずに済むようにする)
	static constexpr size_t kSampleObjectHeadroom = 200;

	// レール曲線可視化の、Catmull-Romの1区間あたりのサンプリング分割数
	// レールを延長して区間が増えても点の間隔が粗くならないよう、区間数に比例させる
	static constexpr int kCurveSamplesPerSegment = 20;

	// Play中のレール本体の管の、Catmull-Romの1区間あたりの分割数(多いほど曲がりがなめらかになる)
	// 曲線の点列と同じく、レールを延長しても管が角ばらないよう区間数に比例させる
	static constexpr int kTubeSegmentsPerRailSegment = 24;
	// Play中のレール本体の管の半径
	static constexpr float kTubeRadius = 0.05f;
	// 管の1区間の長さがこれ未満なら、向きが定まらないため描画しない
	static constexpr float kMinTubeSegmentLength = 1e-4f;

	// 着地できる範囲の可視化のサンプリング分割数(左右2列ぶん描くため曲線より粗くする)
	static constexpr int kLandingRangeSampleCount = 50;
	// 着地できる範囲の水平方向の許容距離の初期値(SetLandingRangeRadius()で上書きされる)
	static constexpr float kDefaultLandingRangeRadius = 1.0f;

	// 最近傍点探索用の定数
	// 粗いサンプリングの分割数(精度と負荷のバランスをとった値)
	static constexpr int kNearestSearchSampleCount = 200;
	// 粗探索後、三分探索で絞り込む反復回数
	static constexpr int kNearestRefineIterationCount = 20;

	// 距離から進行度を求めるときに使う定数
	// 進行度1あたりの移動距離を前後の座標の差分で求めるときの、進行度tの刻み幅
	static constexpr float kArcLengthSampleDelta = 0.001f;
	// 進行度1あたりの移動距離がこれ未満なら、制御点が重なっているとみなして進行度を進めない(0除算防止)
	static constexpr float kMinDistancePerT = 1e-5f;
};