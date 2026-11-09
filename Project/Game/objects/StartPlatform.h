#pragma once

#include "MyMath.h"
#include "objects/StageInfo.h"
#include <array>
#include <memory>
#include <vector>

// 前方宣言
class Obj3D;
class Obj3dCommon;
class PaintCanvas;

/// <summary>
/// スタート台(ステージ選択の場所)
/// ワールドに固定して置く台。真上から見て「凸」の形で、正方形の部分の正面・右・左の辺と、後ろの辺の途中から伸ばした通路の先にゲートを1つずつ立てる。
/// ゲートの番号はステージの番号と同じ(0: 正面 = ステージ1、1: 右 = ステージ2、2: 左 = ステージ3、3: 後ろの通路の先 = 最後のステージ)。
/// 各ステージのレールの始点は、そのゲートの先の辺と少し重なる位置(GetRailStartPosition())に置き、歩いて前へ進むだけでレールに乗れるようにする。
/// プレイヤーは台の真ん中から始まり、自由に動いてから、選んだゲートの前の目印で出発の操作をする。
/// ゲートが閉じている間はゲートより先へ進めない。最後のステージのゲートは、ほかのステージをすべてクリアするまで開けられない。
/// クリアしたステージのゲートはランプを点けたままにし、まだ開けられないゲートはランプを赤くする。
/// ゲートの前の目印で出発の操作をすると、目印から床の上をインクが走って左右の柱へ向かい、柱を下から塗り上がる。
/// 塗り上がりは 3・2・1 のカウントダウンに合わせ、合図のたびに一段ずつ一気に塗り上がってランプを下から1段ずつ点ける。
/// 1 の後に梁まで塗り終わり、0 の合図で遮断棒がインクを散らしながら跳ね上がって、レールへ歩いて乗れるようになる。
/// 乗り込んだ瞬間には、足元から輪を広げて出発の合図にする。
/// 出発の演出(スーパージャンプなど)を使うときは、ゲートの先に出発台の目印を出し、その手前の線を越えたら演出を始める。
/// 台の上面にはインクを塗れ(PaintCanvas)、出発のときに飛び散ったインクが落ちたところに跡が残る。
/// クリアしたステージのゲートの前の床は、台のインクを消すたびにインクで塗り直し、どのステージをクリアしたか分かるようにする。
/// 最後のステージへ続く通路には、ほかのステージに1枚ずつ壁を立ててふさぎ、そのステージをクリアすると壁が壊れる。
/// </summary>
class StartPlatform{
public:
	StartPlatform();
	~StartPlatform();

	// 初期化処理(台・ゲート・目印・輪のモデルの生成と、決まった位置への配置)
	void Initialize(Obj3dCommon* objCommon);

	// 台の上面の中央(ワールドに固定した位置。地面の格子・太陽の向きの基準にも使う)
	static constexpr Vector3 kTopCenter = {-4.8f, 0.0f, 0.0f};
	// 台の正面の向き(正面の辺のゲートからステージ1へ出る向き。XZ平面の正規化ベクトル)
	// 上面の中央と正面の向きは、ステージ1のレールの始点 (1.7, 0, 0) が正面の辺の出発位置に来るように決めている
	static constexpr Vector3 kFrontDirection = {1.0f, 0.0f, 0.0f};
	// ゲートの数(ステージの数と同じ。ゲートの番号はステージの番号と同じ)
	static constexpr int kGateCount = StageInfo::kStageCount;
	// 最後のステージへ続く通路をふさぐ壁の数(最後のステージ以外のステージに1枚ずつ。壁の番号はステージの番号と同じ)
	static constexpr int kBarrierCount = StageInfo::kFinalStageIndex;

	// ゲートを閉じた状態(ランプ消灯・遮断棒が下りている・目印を表示)に戻す
	// 選んでいるゲートは、今のステージ(StageInfo::currentStageIndex)のゲートに戻す
	void ResetGate();

	// 指定したゲートを選んで開け始める(目印から柱へインクを流し込み、柱と梁を塗り終わったら遮断棒を上げる)
	void BeginOpenGate(int gateIndex);

	// 選んでいるゲートを、開ける演出を出さずにすぐ開ききった状態にする(タイトルを飛ばしてレールの上から始めるとき用)
	void OpenGateImmediately();

	// 選んでいるゲートが開ききったか(レールへ歩いて乗れる状態か)
	bool IsGateOpened() const;

	// 選んでいる(開けている・最後に開けた)ゲートの番号
	int GetSelectedGateIndex() const{ return selectedGateIndex_; }

	/// <summary>
	/// 指定したゲートの先にあるステージの、レールの始点を置く位置(ゲートの先の辺と少し重なる位置。台の上面の高さ)
	/// </summary>
	/// <param name="gateIndex">ゲートの番号(ステージの番号と同じ)</param>
	Vector3 GetRailStartPosition(int gateIndex) const;
	// 指定したゲートから出ていく向き(XZ平面の正規化ベクトル)
	Vector3 GetGateDirection(int gateIndex) const;

	// このフレームで遮断棒が跳ね上がり始めたか(GameScene がゲートからインクを飛び散らせるのに使う)
	bool IsBarBurstTriggered() const{ return isBarBurstTriggered_; }

	// カウントダウンで数える数(最初に出す数。ランプの段の数と同じ)
	static constexpr int GetCountdownCount(){ return kCountdownCount; }

	// 遮断棒が跳ね上がり始めたか(カウントダウンが 0 になったか。開ききった後も true のまま)
	bool HasBarStarted() const{ return isGateOpening_ && gateOpenTimer_ >= kBarStartTime; }

	/// <summary>
	/// ゲートのカウントダウンで今出している数(3・2・1。合図のたびにランプが1段点き、数が1つ減る)
	/// </summary>
	/// <returns>数を出していないとき(最初の合図の前・0 の合図の後・ゲートを開けていないとき)は 0</returns>
	int GetCountdownNumber() const;

	// 遮断棒が跳ね上がるときにインクを飛び散らせる位置(選んでいるゲートの、閉じた遮断棒の中央)
	Vector3 GetGateBurstPosition() const;

	/// <summary>
	/// 選んでいるゲートの中央の根元を原点にした、ゲートの向きでの座標をワールド座標に変換する(ゲートを映すカメラの位置決めに使う)
	/// </summary>
	/// <param name="local">x: 右、y: 上、z: 前(ゲートから出ていく方向)</param>
	Vector3 GateLocalToWorld(const Vector3& local) const;

	// 出発台の目印を出すかどうか(出発の演出を使い、ゲートが開いてから演出を始めるまでの間だけ出す)
	void SetDeparturePadVisible(bool isVisible){ isDeparturePadVisible_ = isVisible; }

	// 選んでいるゲートの出発台の中央の座標(台の上面の高さ)
	Vector3 GetDeparturePadPosition() const;

	/// <summary>
	/// 指定したステージの壁を、そのステージをクリアしていても壊さずに立たせておく(壁を壊す演出で壊れるまで残す)
	/// </summary>
	/// <param name="stageIndex">壁のステージの番号</param>
	void HoldBarrier(int stageIndex);

	// 立たせておいた壁を、クリアしたステージの壁と同じく壊れた状態にする(演出で壊れた瞬間と、やり直したときに呼ぶ)
	void ReleaseBarrierHold();

	// 指定したステージの壁が立っているか(そのステージをまだクリアしていないか、演出で壊れるのを待っているか)
	bool IsBarrierStanding(int stageIndex) const;

	// 指定したステージの壁の中心の座標(壁を映すカメラの注目する位置と、壊れたときに破片を飛ばす位置に使う)
	Vector3 GetBarrierCenter(int stageIndex) const;

	/// <summary>
	/// 台の上面の中央を原点にした、台の向きでの座標をワールド座標に変換する(台を映すカメラの位置決めに使う)
	/// </summary>
	/// <param name="local">x: 右、y: 上、z: 前(正面の向き)</param>
	Vector3 PlatformLocalToWorld(const Vector3& local) const{ return ToWorld(local); }

	/// <summary>
	/// 台の上で、選んでいるゲートの出発台の手前の線を越えたか(出発の演出を始める判定に使う)
	/// 線は台の幅いっぱいに引くため、出発台の横を回り込んでもレールへ乗る前に必ず越える
	/// </summary>
	/// <param name="position">プレイヤーの座標</param>
	bool IsPastDepartureLine(const Vector3& position) const;

	/// <summary>
	/// 出発の合図として、足元から輪を広げる
	/// </summary>
	/// <param name="position">輪を広げる中心(プレイヤーの足元)</param>
	void EmitLaunchRing(const Vector3& position);

	/// <summary>
	/// 台の上面にインクを1滴塗る(実際に塗られるのは FlushInk() のとき)
	/// </summary>
	/// <param name="position">インクの中心(上面の上の座標)</param>
	/// <param name="radius">インクの半径</param>
	void PaintInk(const Vector3& position,float radius);

	/// <summary>
	/// 落ちてくるインクの粒が、このフレームの移動で台の上面に着いたら、着いた位置にインクを塗る
	/// </summary>
	/// <param name="from">移動前の座標</param>
	/// <param name="to">移動後の座標</param>
	/// <param name="radius">塗るインクの半径</param>
	/// <param name="landedPosition">上面に着いた位置を受け取る変数(不要なら nullptr)</param>
	/// <returns>上面に着いて塗ったら true(粒はそこで消す)</returns>
	bool TryLandInk(const Vector3& from,const Vector3& to,float radius,Vector3* landedPosition = nullptr);

	// 台に塗ったインクをすべて消す
	void ClearInk();

	// 予約したインクを台の塗りテクスチャに描き込む(インクを塗る処理がすべて終わった後、描画より前に1フレーム1回呼ぶ)
	void FlushInk();

	// ランプ・遮断棒・目印・輪の動きを進め、行列を更新する
	void Update(float deltaTime);

	// 台とゲートの描画処理(不透明なもの)
	void Draw();

	// 足元の目印と出発の輪の描画処理(半透明なので、他の3Dオブジェクトの後に描く)
	void DrawEffects();

	// 水平方向(XZ平面)で見て、指定した座標が台の上面の範囲に入っているか
	bool IsInsideTop(const Vector3& position) const;

	/// <summary>
	/// ゲートがどれも開いていないときに、どのゲートの前の目印の上に立っているか(出発の操作を受け付ける範囲か)
	/// まだ開けられない最後のステージのゲートの前では受け付けない
	/// </summary>
	/// <param name="position">プレイヤーの座標</param>
	/// <returns>立っている目印のゲートの番号(どの目印の上にもいないときは -1)</returns>
	int FindGateArea(const Vector3& position) const;

	/// <summary>
	/// プレイ会用の設定で閉じているゲートの、すぐ手前に立っているか(「いまは遊べないよ!!」を出す範囲か)
	/// ゲートの幅の中で、ゲートから手前へ kLockedNoticeDepth 以内にいれば立っているとする
	/// </summary>
	/// <param name="position">プレイヤーの座標</param>
	bool IsInFrontOfPlaySessionClosedGate(const Vector3& position) const;

	/// <summary>
	/// 閉じているゲートより先へ進めないように押し戻す(開け始めたゲートの先へはそのまま進める)
	/// 台の範囲の外やジャンプ中でも、閉じているゲートの位置の面より先へは出られない
	/// </summary>
	/// <param name="position">プレイヤーの座標</param>
	/// <returns>押し戻した後の座標</returns>
	Vector3 ClampBehindClosedGate(const Vector3& position) const;

	/// <summary>
	/// 台の上を歩いている間に端から落ちないよう、台の上面の範囲(正方形の部分か通路の、端から少し内側)に押し戻す
	/// </summary>
	/// <param name="position">プレイヤーの座標</param>
	/// <returns>押し戻した後の座標</returns>
	Vector3 ClampInsideTop(const Vector3& position) const;

	/// <summary>
	/// 台の本体(正方形の部分と通路の2つの直方体)を各面から radius だけ外側へ広げた範囲に、レイが最初に当たる距離を求める
	/// 始点が範囲の中にあるとき・maxDistance までに当たらないときは maxDistance を返す
	/// </summary>
	/// <param name="origin">レイの始点</param>
	/// <param name="direction">レイの向き(正規化済み)</param>
	/// <param name="maxDistance">調べる最大の距離</param>
	/// <param name="radius">台を各面から外側へ広げる距離</param>
	/// <returns>当たった距離</returns>
	float Raycast(const Vector3& origin,const Vector3& direction,float maxDistance,float radius) const;

	// 台の上面の高さを取得
	float GetTopHeight() const{ return topCenter_.y; }
	// プレイヤーを立たせる開始位置(台の上面の真ん中)を取得
	const Vector3& GetSpawnPosition() const{ return topCenter_; }

private:
	// ゲート1つ分の、置き方と部品
	struct Gate{
		// ゲートから出ていく向き(XZ平面の正規化ベクトル)と、その右の向き
		Vector3 forward = {0.0f, 0.0f, 1.0f};
		Vector3 right = {1.0f, 0.0f, 0.0f};
		// 部品の向き(Y軸回転。モデルの+Z方向をゲートから出ていく向きへ向ける角度)
		float yaw = 0.0f;
		// ゲートの中央の根元(台の上面の高さ)
		Vector3 origin = {0.0f, 0.0f, 0.0f};
		// 台の上面の中央から、ゲートの先の辺までの距離
		float edgeDistance = 0.0f;
		// 左右の柱と、上の梁
		std::vector<std::unique_ptr<Obj3D>> frames;
		// 柱と梁の塗りテクスチャ(frames と同じ並び。持ち主は PaintManager)
		std::vector<PaintCanvas*> frameCanvases;
		// ランプ(左右の柱に下から kLampCountPerPillar 個ずつ、左・右の順に並べる)
		std::vector<std::unique_ptr<Obj3D>> lamps;
		// 遮断棒
		std::unique_ptr<Obj3D> bar;
		// 遮断棒の今の高さ(Update() で決め、カメラのめり込み防止で使う)
		float barHeight = kBarClosedHeight;
		// ゲートの前の目印(ここに立つと出発の操作ができる)
		std::unique_ptr<Obj3D> marker;
		// 出発台の目印(出発の演出を始める場所)
		std::unique_ptr<Obj3D> departurePad;
	};

	// 台の上面の中央・正面の向きから、台の本体とゲートの部品を置く(動かないため、初期化のときに1回だけ行う)
	void PlaceParts();

	/// <summary>
	/// 台の上面の中央を原点にした、台の向きでの座標をワールド座標に変換する
	/// </summary>
	/// <param name="local">x: 右、y: 上、z: 前(正面の向き)</param>
	Vector3 ToWorld(const Vector3& local) const;

	/// <summary>
	/// ゲートの中央の根元を原点にした、ゲートの向きでの座標をワールド座標に変換する
	/// </summary>
	/// <param name="gate">基準にするゲート</param>
	/// <param name="local">x: 右、y: 上、z: 前(ゲートから出ていく方向)</param>
	Vector3 GateToWorld(const Gate& gate,const Vector3& local) const;

	/// <summary>
	/// ゲートの部品の位置・回転・大きさを設定して行列を更新する
	/// </summary>
	/// <param name="obj">更新する3Dオブジェクト</param>
	/// <param name="gate">部品を立てるゲート</param>
	/// <param name="local">ゲートの向きでの座標(部品の中心)</param>
	/// <param name="size">部品の大きさ(各辺の長さ)</param>
	void UpdatePart(Obj3D& obj,const Gate& gate,const Vector3& local,const Vector3& size);

	// 水平方向で見て、指定した座標が正方形の部分・通路の上面の範囲に入っているか
	bool IsInsideSquareTop(const Vector3& position) const;
	bool IsInsideCorridorTop(const Vector3& position) const;

	// 指定したゲートを開けられるか(最後のステージのゲートは、ほかのステージをすべてクリアし、通路の壁がすべて壊れるまで開けられない)
	bool IsGateSelectable(int gateIndex) const;

	// クリアしたステージのゲートの前の床をインクで塗る(台のインクを消した後に呼び、クリアしたことが分かるようにする)
	void PaintClearedGateFloors();

	/// <summary>
	/// 台の向きでの中心 localCenter・各辺の長さの半分 halfSize の直方体を各面から radius だけ外側へ広げた範囲に、レイが最初に当たる距離を求める
	/// </summary>
	/// <returns>当たった距離(始点が範囲の中にあるとき・maxDistance までに当たらないときは maxDistance)</returns>
	float RaycastBox(const Vector3& localCenter,const Vector3& halfSize,const Vector3& origin,const Vector3& direction,float maxDistance,float radius) const;

	/// <summary>
	/// 通路に立っている壁の、台の中央側の面にレイが最初に当たるまでの距離を求める(カメラが壁の奥へ入らないようにする)
	/// 面から radius だけ手前で止める。始点が面にそれより近いときは 0 を返す
	/// </summary>
	/// <returns>止める距離(当たらないときは maxDistance)</returns>
	float RaycastBarriers(const Vector3& origin,const Vector3& direction,float maxDistance,float radius) const;

	/// <summary>
	/// 中心・向きを決めた直方体を各面から radius だけ外側へ広げた範囲に、レイが最初に当たる距離を求める(RaycastBox・ゲートの部品で共用)
	/// </summary>
	/// <param name="worldCenter">直方体の中心(ワールド座標)</param>
	/// <param name="right">直方体の右の向き(XZ平面の正規化ベクトル)</param>
	/// <param name="forward">直方体の前の向き(XZ平面の正規化ベクトル)</param>
	/// <param name="halfSizeLocal">直方体の各辺の長さの半分(x: 右、y: 上、z: 前)</param>
	/// <returns>当たった距離(始点が範囲の中にあるとき・maxDistance までに当たらないときは maxDistance)</returns>
	float RaycastOrientedBox(const Vector3& worldCenter,const Vector3& right,const Vector3& forward,const Vector3& halfSizeLocal,const Vector3& origin,const Vector3& direction,float maxDistance,float radius) const;

	/// <summary>
	/// ゲートの柱・梁・遮断棒に、レイが最初に当たる距離を求める(カメラがゲートの部品の中へ入らないようにする)
	/// </summary>
	/// <returns>当たった距離(当たらないときは maxDistance)</returns>
	float RaycastGates(const Vector3& origin,const Vector3& direction,float maxDistance,float radius) const;

	/// <summary>
	/// 選んでいるゲートを開けている間に、前のフレームから今のフレームまでにインクが進んだ分だけ、床・柱・梁にインクを塗る
	/// </summary>
	/// <param name="previousTimer">前のフレームでのゲートを開け始めてからの経過時間(秒)</param>
	void PaintGateInk(float previousTimer);

	/// <summary>
	/// 柱を塗り上がったインクの高さ(台の上面から)
	/// </summary>
	/// <param name="timer">ゲートを開け始めてからの経過時間(秒)</param>
	static float GetInkClimbHeight(float timer);

	// カウントダウンの index 回目(0 始まり)の合図の時刻(ゲートを開け始めてからの秒数。この合図で下から index 段目のランプが点く)
	static constexpr float GetCountTime(int index){ return kFirstCountTime + kCountdownInterval * static_cast<float>(index); }

	// 台の本体(正方形の部分と、後ろの辺から伸ばした通路)
	std::unique_ptr<Obj3D> obj_;
	std::unique_ptr<Obj3D> corridorObj_;
	// ゲート(並びはステージの番号と同じ)
	std::array<Gate,kGateCount> gates_;
	// 最後のステージへ続く通路をふさぐ壁(並びはステージの番号と同じ)
	std::array<std::unique_ptr<Obj3D>,kBarrierCount> barriers_;
	// クリアしていても壊さずに立たせておく壁のステージの番号(-1: 立たせていない)
	int heldBarrierIndex_ = -1;
	// 出発の合図に広げる輪
	std::unique_ptr<Obj3D> launchRing_;
	// 台に塗ったインクの塗りテクスチャ(正方形の部分と通路。持ち主は PaintManager)
	PaintCanvas* inkCanvas_ = nullptr;
	PaintCanvas* corridorInkCanvas_ = nullptr;

	// 台の上面の中央
	Vector3 topCenter_ = kTopCenter;
	// 台の向き(XZ平面の正規化ベクトル。正面の向きと右の向き)
	Vector3 forwardXZ_ = kFrontDirection;
	Vector3 rightXZ_ = {1.0f, 0.0f, 0.0f};
	// 台の向き(Y軸回転。モデルの+Z方向を台の正面へ向ける角度)
	float yaw_ = 0.0f;

	// 選んでいる(開けている・最後に開けた)ゲートの番号
	int selectedGateIndex_ = 0;
	// ゲートを開け始めてからの経過時間(秒)
	float gateOpenTimer_ = 0.0f;
	// ゲートを開け始めたか(開ききった後も true のまま)
	bool isGateOpening_ = false;
	// このフレームで遮断棒が跳ね上がり始めたか
	bool isBarBurstTriggered_ = false;
	// 出発台の目印を出すか
	bool isDeparturePadVisible_ = false;
	// 目印を点滅させるための経過時間(秒)
	float markerTimer_ = 0.0f;
	// 出発の輪を広げ始めてからの経過時間(秒)
	float launchRingTimer_ = 0.0f;
	// 出発の輪を表示しているか
	bool isLaunchRingActive_ = false;
	// 出発の輪の中心
	Vector3 launchRingPosition_ = {0.0f, 0.0f, 0.0f};

	// 台の正方形の部分の一辺の長さの半分
	static constexpr float kSquareHalfSize = 8.0f;
	// 後ろの辺の真ん中から後ろへ伸ばす通路の幅の半分と長さ
	static constexpr float kCorridorHalfWidth = 3.0f;
	static constexpr float kCorridorLength = 8.0f;
	// 台の厚みの半分
	static constexpr float kHalfHeight = 0.5f;
	// ゲートの先の辺を、レールの始点からどれだけ先まで重ねるか(重ねた範囲に入ればレールに乗れる)
	static constexpr float kRailOverlapLength = 1.5f;
	// 台の上を歩けるのを、端からどれだけ内側までにするか(プレイヤーの体が台からはみ出さないようにする)
	static constexpr float kEdgeMargin = 0.3f;

	// ゲートを立てる位置の、ゲートの先の辺から手前への距離(ゲートと辺の間に、出発台とレールに乗る範囲を取る)
	static constexpr float kGateEdgeInset = 4.1f;
	// ゲートの幅の半分(左右の柱の外側の端まで)
	static constexpr float kGateHalfWidth = 3.0f;
	// カメラがゲートの部品(柱・梁・遮断棒)へ近づける距離(カメラの近平面 0.1 より少し大きくし、部品の中が見えないようにする)
	static constexpr float kGateCameraMargin = 0.12f;
	// 台の向きで見た、各ゲートから出ていく向き(x: 右、z: 前。並びはステージの番号と同じ: 正面・右・左・後ろ)
	static constexpr Vector3 kGateLocalDirections[kGateCount] = {
		{0.0f, 0.0f, 1.0f},
		{1.0f, 0.0f, 0.0f},
		{-1.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, -1.0f},
	};
	// 正方形の部分のゲートどうしが角でぶつからない
	static_assert(kGateHalfWidth < kSquareHalfSize - kGateEdgeInset,"正方形の部分の隣り合うゲートが角でぶつかる");
	// 後ろの通路にゲートが収まる
	static_assert(kGateHalfWidth <= kCorridorHalfWidth,"後ろの通路の幅にゲートが収まらない");
	// ゲートが閉じている間に近づける、ゲートからの距離(遮断棒にめり込まない距離)
	static constexpr float kGateWallMargin = 0.25f;
	// ゲートの柱の大きさ(台の左右の端に立て、台の幅いっぱいをふさぐ)
	static constexpr Vector3 kPillarSize = {0.22f, 1.4f, 0.22f};
	// ゲートの梁の厚み(高さ)と奥行き
	static constexpr float kBeamHeight = 0.18f;
	static constexpr float kBeamDepth = 0.26f;
	// 1本の柱に並べるランプの数
	static constexpr int kLampCountPerPillar = 3;
	// ランプの大きさ(柱の手前側の面に貼り付ける薄い板)
	static constexpr Vector3 kLampSize = {0.14f, 0.16f, 0.04f};
	// 一番下のランプの高さと、ランプどうしの間隔
	static constexpr float kLampBottomHeight = 0.45f;
	static constexpr float kLampSpacing = 0.3f;
	// 遮断棒の太さ(長さは柱の間をふさぐ長さにする)
	static constexpr float kBarThickness = 0.08f;
	// 遮断棒の高さ(閉じているとき・開いたとき)
	static constexpr float kBarClosedHeight = 0.45f;
	static constexpr float kBarOpenedHeight = 1.25f;
	// ゲートを開けるときの各段階の時間(秒)
	// 目印から柱の根元まで床を走る → 3・2・1 の合図ごとに柱を一段ずつ塗り上がる → 柱の上まで塗り上がって梁の中央まで塗る → 0 の合図で遮断棒が跳ね上がる
	static constexpr float kInkRunDuration = 0.45f;
	static constexpr float kBeamFillDuration = 0.3f;
	static constexpr float kBarOpenDuration = 0.45f;
	// カウントダウンで数える数(ランプの段の数と同じ)と、合図の間隔(秒)
	static constexpr int kCountdownCount = kLampCountPerPillar;
	static constexpr float kCountdownInterval = 0.6f;
	// 柱のインクが次の高さまで一気に塗り上がる時間(秒。合図の瞬間に届くよう、合図の少し前から塗り上がり始める)
	static constexpr float kInkSurgeDuration = 0.25f;
	// 最初の合図(3)の時刻、遮断棒が跳ね上がり始める時刻(0 の合図)と、ゲートが開ききる時刻(ゲートを開け始めてからの秒数)
	static constexpr float kFirstCountTime = kInkRunDuration + kInkSurgeDuration;
	static constexpr float kBarStartTime = kFirstCountTime + kCountdownInterval * static_cast<float>(kCountdownCount);
	static constexpr float kGateOpenedTime = kBarStartTime + kBarOpenDuration;
	static_assert(kCountdownInterval >= kInkSurgeDuration + kBeamFillDuration,"最後の合図(1)から 0 の合図までの間に、柱の上まで塗り上がって梁を塗り終えられない");
	// 出発の操作をした瞬間に、目印の上に塗るインクの半径
	static constexpr float kGateStampRadius = 0.55f;
	// 床を走るインクの粒の間隔・半径と、横に揺らす幅・細かさ(1m 進むあたりの位相の進み、ラジアン)
	static constexpr float kFloorInkSpacing = 0.18f;
	static constexpr float kFloorInkRadius = 0.2f;
	static constexpr float kFloorInkWobbleAmplitude = 0.08f;
	static constexpr float kFloorInkWobbleFrequency = 9.0f;
	// 柱を塗り上がるインクの粒の間隔・半径(柱の太さより少し大きくし、横の面まで回り込ませる)
	static constexpr float kClimbInkSpacing = 0.1f;
	static constexpr float kClimbInkRadius = 0.17f;
	// 梁を塗るインクの粒の間隔・半径
	static constexpr float kBeamInkSpacing = 0.15f;
	static constexpr float kBeamInkRadius = 0.14f;
	// ランプが点いた瞬間に光る色と、点いたときの色に落ち着くまでの時間(秒)
	static constexpr Vector4 kLampFlashColor = {0.9f, 1.0f, 0.9f, 1.0f};
	static constexpr float kLampFlashDuration = 0.2f;
	// 目印の、ゲートから手前への距離と半径
	static constexpr float kMarkerDistanceFromGate = 0.9f;
	static constexpr float kMarkerRadius = 0.5f;
	// 出発の操作を受け付ける、目印の中心からの距離
	static constexpr float kGateAreaRadius = 0.6f;
	// プレイ会用の設定で閉じているゲートの前で「いまは遊べないよ!!」を出す、ゲートから手前への距離(目印の位置より少し手前まで)
	static constexpr float kLockedNoticeDepth = 1.5f;
	// 出発台の中央の、ゲートから前への距離と、出発台の目印の半径
	// 出発台の手前の端(中央から半径だけ手前)に、出発の演出を始める線を引く
	static constexpr float kDeparturePadDistance = 1.6f;
	static constexpr float kDeparturePadRadius = 0.7f;
	// 出発の操作を受け付ける、台の上面との高さの差(ジャンプ中は受け付けない)
	static constexpr float kGateAreaHeightRange = 0.1f;
	// 目印・輪を上面から浮かせる高さ(上面と重なってちらつかないようにする)
	static constexpr float kDecalLift = 0.01f;
	// 目印の点滅の速さ(1秒あたりの位相の進み、ラジアン)と、最も薄くなったときの濃さ
	static constexpr float kMarkerBlinkSpeed = 4.0f;
	static constexpr float kMarkerMinAlpha = 0.35f;
	// 出発の輪が広がりきるまでの時間(秒)と、広がり始め・広がりきったときの半径
	static constexpr float kLaunchRingDuration = 0.6f;
	static constexpr float kLaunchRingStartRadius = 0.3f;
	static constexpr float kLaunchRingEndRadius = 2.4f;

	// 通路の壁の、台の上面の中央から後ろへの距離(壁の中心まで。並びはステージの番号と同じで、手前からステージ1・2・3の壁を並べる)
	static constexpr float kBarrierDistances[kBarrierCount] = {8.6f, 9.4f, 10.2f};
	// 壁の高さと、厚みの半分(幅は通路の幅いっぱいにする)
	static constexpr float kBarrierHeight = 1.6f;
	static constexpr float kBarrierHalfThickness = 0.1f;
	// 壁はすべて通路の上に立てる
	static_assert(kBarrierDistances[0] - kBarrierHalfThickness > kSquareHalfSize,"通路の壁が正方形の部分にはみ出している");
	// 一番奥の壁が、最後のステージのゲートの前の目印(出発の操作を受け付ける範囲)にかからない
	static_assert(kBarrierDistances[kBarrierCount - 1] + kBarrierHalfThickness < kSquareHalfSize + kCorridorLength - kGateEdgeInset - kMarkerDistanceFromGate - kGateAreaRadius,
		"通路の壁が最後のステージのゲートの前の目印にかかっている");
	// クリアしたステージのゲートの前の床を塗るインクの数と、1つずつの位置(ゲートの中央の根元を原点にした、ゲートの向きでの座標。x: 右、z: 前)と半径
	// 目印(ゲートから kMarkerDistanceFromGate 手前)と重ならないよう、目印より手前(台の真ん中側)にインク溜まりを作る
	static constexpr int kClearedFloorSplatCount = 6;
	static constexpr Vector3 kClearedFloorSplatPositions[kClearedFloorSplatCount] = {
		{0.0f, 0.0f, -2.5f},
		{-1.3f, 0.0f, -2.1f},
		{1.2f, 0.0f, -2.2f},
		{-0.7f, 0.0f, -3.4f},
		{0.8f, 0.0f, -3.3f},
		{0.1f, 0.0f, -1.8f},
	};
	static constexpr float kClearedFloorSplatRadii[kClearedFloorSplatCount] = {1.2f, 0.65f, 0.6f, 0.55f, 0.6f, 0.45f};

	// ゲートの柱・梁の色(暗いグレー)
	static constexpr Vector4 kFrameColor = {0.28f, 0.31f, 0.34f, 1.0f};
	// 遮断棒の色(自分のインクと同じ黄色)
	static constexpr Vector4 kBarColor = {1.0f, 0.85f, 0.15f, 1.0f};
	// ランプが消えているとき(暗い青)と点いたとき(明るい緑)の色
	static constexpr Vector4 kLampOffColor = {0.15f, 0.35f, 0.6f, 1.0f};
	static constexpr Vector4 kLampOnColor = {0.35f, 1.0f, 0.45f, 1.0f};
	// まだ開けられないゲートのランプの色(暗い赤)
	static constexpr Vector4 kLampLockedColor = {0.6f, 0.12f, 0.12f, 1.0f};
	// 目印・出発の輪の色(自分のインクと同じ黄色)
	static constexpr Vector4 kInkColor = {1.0f, 0.85f, 0.15f, 1.0f};
	// 通路の壁の色(どのステージの壁か分かるよう、ステージごとに変える。並びはステージの番号と同じ)
	static constexpr Vector4 kBarrierColors[kBarrierCount] = {
		{1.0f, 0.5f, 0.15f, 1.0f},
		{0.2f, 0.75f, 1.0f, 1.0f},
		{0.65f, 0.35f, 1.0f, 1.0f},
	};
	// 台の塗りテクスチャの一辺のピクセル数(正方形の部分・通路の上面の 1m あたりおよそ 60 ピクセル)
	static constexpr uint32_t kInkCanvasSize = 1024;
	static constexpr uint32_t kCorridorInkCanvasSize = 512;
	// 柱・梁の塗りテクスチャの一辺のピクセル数(どちらも 1m あたりおよそ 60〜75 ピクセル)
	static constexpr uint32_t kGatePillarCanvasSize = 256;
	static constexpr uint32_t kGateBeamCanvasSize = 1024;
};
