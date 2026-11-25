#pragma once

#include "MyMath.h"
#include <array>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

// 前方宣言
class Obj3D;
class Obj3dCommon;
class InkEffectManager;
class TargetManager;
class GoalTarget;

/// <summary>
/// サブウェポン(Q)の弾の管理
/// Q を押した瞬間に画面に映っている的(普通の的・鍵の的・ゴールの的)のうち、プレイヤーから近い的を最大3つ選び、背中のタンクから3発の弾を飛ばす。
/// 的が3つなら1発ずつ、2つなら近い方に2発、1つなら3発ともその的へ飛ばす。
/// ほかのサブの弾がもう向かっている的は選ばない(ゴールの的は体力があり、何発も当てる的のため、向かっている弾があっても選ぶ)。
/// 弾は後ろへ出てから左・上・右へ扇のように開き、曲がって的へ向かう。撃つ瞬間に、狙っていない的の近くを通らない進路を選ぶ。
/// 決めた時間で必ず的に着く(並走して動くゴールの的は、飛んでいる間も行き先を的の今の位置に合わせ直す)。
/// 着いたときに、普通の的・鍵の的は生きていれば壊し、ゴールの的は体力を減らす。
/// メインの弾などに先に壊されていたら、その位置でしぶきを出して消える(まわりの的は壊さない)。
/// 飛んでいる途中は何にも当たらない。
/// </summary>
class HomingShotManager{
public:
	HomingShotManager();
	~HomingShotManager();

	/// <summary>
	/// 初期化処理(弾のモデルを読み込み、使い回す弾を最大数ぶん生成し、調整項目を登録する)
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	/// <param name="paramGroup">調整項目を登録するGlobalVariablesのグループ名</param>
	void Initialize(Obj3dCommon* objCommon,const std::string& paramGroup);

	// 飛んでいる弾から垂れるインクの粒と、壊れていた的に着いたときのしぶきを出す演出を設定する(設定しなければ出さない)
	void SetInkEffect(InkEffectManager* inkEffect){ inkEffect_ = inkEffect; }

	/// <summary>
	/// 狙う的を選んで3発撃つ(狙える的が無いとき、使い回す弾が足りないときは撃たない)
	/// </summary>
	/// <param name="playerPosition">プレイヤーの表示位置(足元。的を近い順に選ぶ基準と、背中のタンクの位置を求めるのに使う)</param>
	/// <param name="cameraForward">カメラの前方ベクトル(背中の向きと、弾を開く向きを求めるのに使う)</param>
	/// <param name="viewProjection">カメラのビュー・プロジェクション行列(画面に映っている的に限るのに使う)</param>
	/// <param name="targets">狙う的(普通の的・鍵の的)</param>
	/// <param name="goalTarget">狙うゴールの的(現れていて、まだ壊れていないときだけ狙う)</param>
	/// <returns>撃ったとき true</returns>
	bool Fire(const Vector3& playerPosition,const Vector3& cameraForward,const Matrix4x4& viewProjection,const TargetManager& targets,const GoalTarget& goalTarget);

	/// <summary>
	/// 弾を進め、着いた弾の的を壊す・ゴールの的の体力を減らす(的の Update() の後、ゴールの的の Update() の前に呼ぶ)
	/// </summary>
	/// <param name="deltaTime">経過時間(秒)</param>
	/// <param name="targets">着いた弾が壊す的</param>
	/// <param name="goalTarget">着いた弾が体力を減らすゴールの的</param>
	void Update(float deltaTime,TargetManager& targets,GoalTarget& goalTarget);

	// 飛んでいる弾を描画する
	void Draw();

	// すべての弾を消す(やり直し・ステージの切り替え・大ジャンプ・Editへ戻るときに呼ぶ)
	void Clear();

private:
	// 狙う的(普通の的・鍵の的は番号で、ゴールの的は isGoal で表す)
	struct AimTarget{
		int index = -1;      // 的の番号(ゴールの的のときは使わない)
		bool isGoal = false; // ゴールの的か
	};

	// 1発の弾
	struct Shot{
		// 描画用の3Dオブジェクト(初期化時に一度だけ生成する)
		std::unique_ptr<Obj3D> obj;
		// 進路の3次ベジェ曲線の制御点(始点: 背中のタンク、終点: 狙った的の中心)
		std::array<Vector3,4> controlPoints;
		// 狙った的
		AimTarget target;
		// 撃ってからの経過時間(秒)
		float timer = 0.0f;
		// 軌跡の粒を前に出してからの経過時間(秒)
		float trailTimer = 0.0f;
		// 飛んでいるか(false なら未使用で、次に撃つときに使い回せる)
		bool isAlive = false;
	};

	// 1発の進路を決めるときに使う、撃つ瞬間の向き
	struct LaunchFrame{
		Vector3 origin;    // 背中のタンクの位置(進路の始点)
		Vector3 backward;  // 背中の向き(水平。カメラの前方の逆)
		Vector3 right;     // 右の向き(水平)
	};

	/// <summary>
	/// 狙う的を、画面に映っていて範囲内にある生きている的(ゴールの的を含む)から、プレイヤーに近い順に選ぶ
	/// ほかの弾がもう向かっている的は選ばない(ゴールの的は選ぶ)
	/// </summary>
	/// <param name="playerPosition">プレイヤーの位置</param>
	/// <param name="viewProjection">カメラのビュー・プロジェクション行列</param>
	/// <param name="targets">的</param>
	/// <param name="goalTarget">ゴールの的</param>
	/// <param name="outTargets">選んだ的(近い順。最大で1回に撃つ弾の数)</param>
	void SelectTargets(const Vector3& playerPosition,const Matrix4x4& viewProjection,const TargetManager& targets,const GoalTarget& goalTarget,std::vector<AimTarget>& outTargets) const;

	/// <summary>
	/// 1発の進路(3次ベジェ曲線の制御点)を決める
	/// 開く向き・広がりを変えた進路を順に試し、狙っていない生きている的の近くを通らない最初の進路を選ぶ
	/// どの進路でも近くを通ってしまうときは、近くを通る的の数が一番少ない進路にする(通る的はすり抜ける)
	/// </summary>
	/// <param name="frame">撃つ瞬間の向き</param>
	/// <param name="slotAngle">この弾を開く向き(背中から見て、右を0として上へ回る角度。ラジアン)</param>
	/// <param name="aim">狙う的</param>
	/// <param name="targets">的(よける的を調べるのに使う)</param>
	/// <param name="goalTarget">ゴールの的(よける的を調べるのに使う)</param>
	/// <returns>進路の制御点</returns>
	std::array<Vector3,4> PlanPath(const LaunchFrame& frame,float slotAngle,const AimTarget& aim,const TargetManager& targets,const GoalTarget& goalTarget) const;

	/// <summary>
	/// 進路が、狙っていない生きている的(ゴールの的を含む)の近くを通る数を数える
	/// </summary>
	/// <param name="controlPoints">進路の制御点</param>
	/// <param name="aim">狙う的(数えない)</param>
	/// <param name="targets">的</param>
	/// <param name="goalTarget">ゴールの的</param>
	/// <returns>近くを通る的の数</returns>
	int CountPathObstacles(const std::array<Vector3,4>& controlPoints,const AimTarget& aim,const TargetManager& targets,const GoalTarget& goalTarget) const;

	// 狙う的の今の中心の座標を取得する
	static const Vector3& GetAimPosition(const AimTarget& aim,const TargetManager& targets,const GoalTarget& goalTarget);

	// 進路の進行度 t(0〜1)の位置を求める
	static Vector3 EvaluatePath(const std::array<Vector3,4>& controlPoints,float t);
	// 進路の進行度 t(0〜1)での進む向き(正規化していない)を求める
	static Vector3 EvaluatePathTangent(const std::array<Vector3,4>& controlPoints,float t);

	// 弾の描画用の行列を更新する(球を進む向きへ少し伸ばす)
	void UpdateShotTransform(Shot& shot,const Vector3& position,const Vector3& direction);

	Obj3dCommon* objCommon_ = nullptr;

	// 調整項目を登録したGlobalVariablesのグループ名
	std::string paramGroup_;

	// 使い回す弾の実体(サイズは kMaxShotCount で固定)
	std::vector<Shot> shots_;

	// 軌跡の粒・しぶきを出す演出(GameScene が持つものを借りる)
	InkEffectManager* inkEffect_ = nullptr;

	// 1回に撃つ弾の数
	static constexpr int kShotsPerVolley = 3;
	// 同時に飛ばせる回数(インクが戻る速さでは2回分が重なることはほとんどないが、余裕を持たせる)
	static constexpr int kMaxVolleys = 3;
	// 使い回す弾の最大数
	static constexpr size_t kMaxShotCount = static_cast<size_t>(kShotsPerVolley * kMaxVolleys);
	// 半分を求めるときの割合
	static constexpr float kHalf = 0.5f;
	// 弾ごとに開く向き(背中から見て、右を0として上へ回る角度。ラジアン)。並びは 左(180度)・上(90度)・右(0度)
	static constexpr std::array<float,kShotsPerVolley> kSlotAngles = {std::numbers::pi_v<float>, std::numbers::pi_v<float> * kHalf, 0.0f};

	// 狙う的を探す、プレイヤーからの距離の初期値(保存済みJSONがあればそちらが優先される)
	static constexpr float kDefaultRange = 25.0f;
	// 撃ってから的に着くまでの時間の初期値(秒。保存済みJSONがあればそちらが優先される)
	static constexpr float kDefaultFlightTime = 0.6f;
	// ゴールの的に1発当たったときに減らす体力の初期値(メインの弾1発と同じ。保存済みJSONがあればそちらが優先される)
	static constexpr int kDefaultGoalDamage = 1;
	// 的に着くまでの時間として設定できる下限(秒。0にすると進行度を求められないため)
	static constexpr float kMinFlightTime = 0.05f;

	// 背中のタンクの、足元からの高さと、体の中心から背中側へずらす距離
	static constexpr float kTankHeight = 0.4f;
	static constexpr float kTankBackOffset = 0.12f;
	// 撃ち出した直後に向かう点(進路の2番目の制御点)を、タンクから背中側へ離す距離と、開く向きへ離す距離
	static constexpr float kLaunchBackDistance = 1.5f;
	static constexpr float kLaunchSpreadDistance = 2.5f;
	// 的の手前で向かう点(進路の3番目の制御点)を、的から開く向きへ離す距離と、プレイヤー側へ戻す距離
	static constexpr float kApproachSpreadDistance = 1.5f;
	static constexpr float kApproachBackDistance = 4.0f;
	// 的が近いときに、プレイヤー側へ戻す距離が的までの距離に対して大きくなりすぎないようにする割合(的より後ろへ回り込みすぎない)
	static constexpr float kApproachBackMaxRate = 0.5f;

	// よける進路を探すときに試す、開く向きのずらし方(ラジアン)と、広がりの倍率(試す順に並べる。最初の組み合わせが基本の進路)
	static constexpr std::array<float,5> kDetourAngleOffsets = {0.0f, 0.45f, -0.45f, 0.9f, -0.9f};
	static constexpr std::array<float,2> kDetourSpreadScales = {1.0f, 1.6f};
	// 進路がほかの的の近くを通るかを調べるために、進路の上で調べる点の数
	static constexpr int kPathSampleCount = 16;
	// ほかの的の当たり判定の半径に足す余白(弾の大きさの分と、すれすれを通らない分)
	static constexpr float kAvoidMargin = 0.3f;

	// 3次ベジェ曲線の係数(位置を求めるときの中の2つの制御点の重み・向きを求めるときの重み)
	static constexpr float kBezierInnerWeight = 3.0f;
	static constexpr float kBezierTangentCrossWeight = 6.0f;

	// 弾の表示スケール
	static constexpr float kScale = 0.2f;
	// 球を進む向きへ伸ばすときの、表示スケールに掛ける太さと長さの割合(メインの弾より丸い球に見せる)
	static constexpr float kStretchWidthRate = 0.7f;
	static constexpr float kStretchLengthRate = 1.8f;
	// 1発の弾から軌跡の粒を出す間隔(秒。曲がって飛ぶ進路が見えるよう、メインの弾より細かくする)
	static constexpr float kTrailInterval = 0.03f;
	// 壊れていた的に着いたときに出すしぶきの数
	static constexpr int kPopSplashCount = 6;
	// 向きを求められないとみなす長さ
	static constexpr float kDirectionEpsilon = 0.0001f;
};
