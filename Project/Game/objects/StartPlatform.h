#pragma once

#include "MyMath.h"
#include <memory>
#include <vector>

// 前方宣言
class Obj3D;
class Obj3dCommon;
class PaintCanvas;

/// <summary>
/// スタート台
/// 最初のレールの始点の手前に置く箱。プレイヤーは台の上から始まり、自由に動いてからレールに乗る。
/// 台の前側をレールの始点と少し重ねて置き、歩いて前へ進むだけでレールに乗れるようにする。
/// 台の上には出発ゲートを立て、閉じている間はゲートより先へ進めないようにする。
/// ゲートの前の目印で出発の操作をすると、目印から床の上をインクが走って左右の柱へ向かい、柱を下から塗り上がる。
/// 塗り上がりは 3・2・1 のカウントダウンに合わせ、合図のたびに一段ずつ一気に塗り上がってランプを下から1段ずつ点ける。
/// 1 の後に梁まで塗り終わり、0 の合図で遮断棒がインクを散らしながら跳ね上がって、レールへ歩いて乗れるようになる。
/// 乗り込んだ瞬間には、足元から輪を広げて出発の合図にする。
/// 出発の演出(スーパージャンプなど)を使うときは、ゲートの先に出発台の目印を出し、その手前の線を越えたら演出を始める。
/// 台の上面にはインクを塗れ(PaintCanvas)、出発のときに飛び散ったインクが落ちたところに跡が残る。
/// </summary>
class StartPlatform{
public:
	StartPlatform();
	~StartPlatform();

	// 初期化処理(台・ゲート・目印・輪のモデルの生成)
	void Initialize(Obj3dCommon* objCommon);

	/// <summary>
	/// レールの始点に合わせて台を置く(Edit中にレールを動かしても反映されるよう、Playに入るたびに呼ぶ)
	/// </summary>
	/// <param name="railStartPosition">レールの始点の座標(台の上面の高さもこの高さにそろえる)</param>
	/// <param name="railStartForward">レールの始点での進行方向(台はこの向きの手前側に置く)</param>
	void Place(const Vector3& railStartPosition,const Vector3& railStartForward);

	// ゲートを閉じた状態(ランプ消灯・遮断棒が下りている・目印を表示)に戻す
	void ResetGate();

	// ゲートを開け始める(目印から柱へインクを流し込み、柱と梁を塗り終わったら遮断棒を上げる)
	void BeginOpenGate();

	// ゲートを、開ける演出を出さずにすぐ開ききった状態にする(タイトルを飛ばしてレールの上から始めるとき用)
	void OpenGateImmediately();

	// ゲートが開ききったか(レールへ歩いて乗れる状態か)
	bool IsGateOpened() const;

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

	// 遮断棒が跳ね上がるときにインクを飛び散らせる位置(閉じた遮断棒の中央)
	Vector3 GetGateBurstPosition() const;

	/// <summary>
	/// ゲートの中央の根元を原点にした、台の向きでの座標をワールド座標に変換する(ゲートを映すカメラの位置決めに使う)
	/// </summary>
	/// <param name="local">x: 右、y: 上、z: 前(レールの方向)</param>
	Vector3 GateLocalToWorld(const Vector3& local) const;

	// 出発台の目印を出すかどうか(出発の演出を使い、ゲートが開いてから演出を始めるまでの間だけ出す)
	void SetDeparturePadVisible(bool isVisible){ isDeparturePadVisible_ = isVisible; }

	// 出発台の中央の座標(台の上面の高さ)
	Vector3 GetDeparturePadPosition() const;

	/// <summary>
	/// 台の上で、出発台の手前の線を越えたか(出発の演出を始める判定に使う)
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
	/// ゲートが閉じていて、ゲートの前の目印の上に立っているか(出発の操作を受け付ける範囲か)
	/// </summary>
	/// <param name="position">プレイヤーの座標</param>
	bool IsInGateArea(const Vector3& position) const;

	/// <summary>
	/// ゲートが閉じている間は、ゲートより先へ進めないように押し戻す(開け始めた後はそのまま返す)
	/// 台の幅の外やジャンプ中でも、ゲートの位置の面より先へは出られない
	/// </summary>
	/// <param name="position">プレイヤーの座標</param>
	/// <returns>押し戻した後の座標</returns>
	Vector3 ClampBehindClosedGate(const Vector3& position) const;

	/// <summary>
	/// 台の上を歩いている間に端から落ちないよう、台の上面の範囲(端から少し内側)に押し戻す
	/// </summary>
	/// <param name="position">プレイヤーの座標</param>
	/// <returns>押し戻した後の座標</returns>
	Vector3 ClampInsideTop(const Vector3& position) const;

	/// <summary>
	/// 台の本体(直方体)を各面から radius だけ外側へ広げた範囲に、レイが最初に当たる距離を求める
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
	// プレイヤーを立たせる開始位置(台の上面の、ゲートから離れた後ろ寄りの位置)を取得
	const Vector3& GetSpawnPosition() const{ return spawnPosition_; }

private:
	/// <summary>
	/// 台の上面の中央を原点にした、台の向きでの座標をワールド座標に変換する
	/// </summary>
	/// <param name="local">x: 右、y: 上、z: 前(レールの方向)</param>
	Vector3 ToWorld(const Vector3& local) const;

	/// <summary>
	/// ゲートの部品の位置・回転・大きさを設定して行列を更新する
	/// </summary>
	/// <param name="obj">更新する3Dオブジェクト</param>
	/// <param name="local">台の向きでの座標(部品の中心)</param>
	/// <param name="size">部品の大きさ(各辺の長さ)</param>
	void UpdatePart(Obj3D& obj,const Vector3& local,const Vector3& size);

	/// <summary>
	/// ゲートを開けている間に、前のフレームから今のフレームまでにインクが進んだ分だけ、床・柱・梁にインクを塗る
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

	// 描画用の3Dオブジェクト
	std::unique_ptr<Obj3D> obj_;
	// ゲートの左右の柱と、上の梁
	std::vector<std::unique_ptr<Obj3D>> gateFrames_;
	// 柱と梁の塗りテクスチャ(gateFrames_ と同じ並び。持ち主は PaintManager)
	std::vector<PaintCanvas*> gateFrameCanvases_;
	// ゲートのランプ(左右の柱に下から kLampCountPerPillar 個ずつ、左・右の順に並べる)
	std::vector<std::unique_ptr<Obj3D>> lamps_;
	// ゲートの遮断棒
	std::unique_ptr<Obj3D> bar_;
	// ゲートの前の目印(ここに立つと出発の操作ができる)
	std::unique_ptr<Obj3D> marker_;
	// 出発の合図に広げる輪
	std::unique_ptr<Obj3D> launchRing_;
	// 出発台の目印(出発の演出を始める場所)
	std::unique_ptr<Obj3D> departurePad_;
	// 台に塗ったインクの塗りテクスチャ(持ち主は PaintManager)
	PaintCanvas* inkCanvas_ = nullptr;

	// 台の上面の中央
	Vector3 topCenter_ = {0.0f, 0.0f, 0.0f};
	// プレイヤーを立たせる開始位置
	Vector3 spawnPosition_ = {0.0f, 0.0f, 0.0f};
	// 台の向き(XZ平面の正規化ベクトル。前方向と右方向)
	Vector3 forwardXZ_ = {0.0f, 0.0f, 1.0f};
	Vector3 rightXZ_ = {1.0f, 0.0f, 0.0f};
	// 台の向き(Y軸回転。モデルの+Z方向を台の前方向へ向ける角度)
	float yaw_ = 0.0f;

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

	// 台の左右の幅の半分
	static constexpr float kHalfWidth = 3.0f;
	// 台の前後の長さの半分
	static constexpr float kHalfLength = 5.5f;
	// 台の厚みの半分
	static constexpr float kHalfHeight = 0.5f;
	// 台の前の端を、レールの始点からどれだけ先まで重ねるか(重ねた範囲に入ればレールに乗れる)
	static constexpr float kRailOverlapLength = 1.5f;
	// プレイヤーを立たせる開始位置の、台の上面の中央から後ろへの距離(ゲートまで少し歩かせる)
	static constexpr float kSpawnBackOffset = 3.0f;
	// 台の上を歩けるのを、端からどれだけ内側までにするか(プレイヤーの体が台からはみ出さないようにする)
	static constexpr float kEdgeMargin = 0.3f;

	// ゲートを立てる位置の、台の上面の中央から前への距離
	static constexpr float kGateDistance = 1.4f;
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

	// ゲートの柱・梁の色(暗いグレー)
	static constexpr Vector4 kFrameColor = {0.28f, 0.31f, 0.34f, 1.0f};
	// 遮断棒の色(自分のインクと同じ黄色)
	static constexpr Vector4 kBarColor = {1.0f, 0.85f, 0.15f, 1.0f};
	// ランプが消えているとき(暗い青)と点いたとき(明るい緑)の色
	static constexpr Vector4 kLampOffColor = {0.15f, 0.35f, 0.6f, 1.0f};
	static constexpr Vector4 kLampOnColor = {0.35f, 1.0f, 0.45f, 1.0f};
	// 目印・出発の輪の色(自分のインクと同じ黄色)
	static constexpr Vector4 kInkColor = {1.0f, 0.85f, 0.15f, 1.0f};
	// 台の塗りテクスチャの一辺のピクセル数(上面の 1m あたりおよそ 70 ピクセル)
	static constexpr uint32_t kInkCanvasSize = 1024;
	// 柱・梁の塗りテクスチャの一辺のピクセル数(どちらも 1m あたりおよそ 60〜75 ピクセル)
	static constexpr uint32_t kGatePillarCanvasSize = 256;
	static constexpr uint32_t kGateBeamCanvasSize = 1024;
};
