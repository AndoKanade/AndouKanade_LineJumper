#pragma once

#include "MyMath.h"
#include "Model.h"
#include <functional>
#include <memory>
#include <random>
#include <vector>

// 前方宣言
class Obj3D;
class Obj3dCommon;

/// <summary>
/// プレイヤーの弾の管理
/// 発射・移動・寿命切れの処理と、的や敵との当たり判定に使う境界球の判定を行う。
/// 弾は射程の距離まではまっすぐ飛び、射程を過ぎると勢いが落ちて重力で落ちていく。
/// 撃つたびに向きを少しランダムにずらして、弾をばらけさせる。
/// 発射のたびに生成すると無駄が多いため、初期化時に最大数だけ作っておき使い回す。
/// 着地を調べる処理(SetLandingHandler)を渡すと、弾が面に着いたところにインクを塗って消える。
/// </summary>
class PlayerBulletManager{
public:
	/// <summary>
	/// 弾の着地を調べる処理の型
	/// 移動前の座標・移動後の座標・塗るインクの半径を受け取り、面に着いてインクを塗ったら true を返す
	/// </summary>
	using LandingHandler = std::function<bool(const Vector3& from,const Vector3& to,float inkRadius)>;

	PlayerBulletManager();
	~PlayerBulletManager();

	// 初期化処理(弾のモデルを読み込み、使い回す弾を最大数ぶん生成する)
	void Initialize(Obj3dCommon* objCommon);

	/// <summary>
	/// 弾を1発発射する(未使用の弾が無いときは何もしない)
	/// </summary>
	/// <param name="position">発射位置</param>
	/// <param name="direction">狙う方向(正規化済み。ここから少しランダムにずらした向きへ撃つ)</param>
	/// <param name="canHit">的・敵に当たるか(スタート台での試し撃ちの弾は false にして、塗るだけにする)</param>
	void Fire(const Vector3& position,const Vector3& direction,bool canHit);

	// 弾の着地を調べる処理を設定する(設定しなければ、弾は何にも着かずに寿命まで飛ぶ)
	void SetLandingHandler(LandingHandler handler){ landingHandler_ = std::move(handler); }

	// 弾の移動と生存時間の更新(的に当たらなくても一定時間で消滅させる。面に着いた弾はインクを塗って消滅させる)
	void Move(float deltaTime);

	/// <summary>
	/// 指定した境界球に重なる生存中の弾を1発探し、見つかればその弾を消滅させる
	/// 的・敵に当たらない弾(試し撃ちの弾)は判定しない
	/// </summary>
	/// <param name="sphere">判定相手の境界球</param>
	/// <returns>弾が当たったときtrue</returns>
	bool CheckHit(const Model::BoundingSphere& sphere);

	// 弾のトランスフォームを更新する(描画用)
	void UpdateTransforms();

	// 発射中の弾を描画する
	void Draw();

	// すべての弾を未使用に戻す(ゲームの開始時に残っている弾のリセット用)
	void Clear();

	// 弾1発が敵に与えるダメージ量を取得
	int GetDamage() const{ return kDamage; }

private:
	// 弾(プロジェクタイル)
	struct Bullet{
		// 描画用の3Dオブジェクト(初期化時に一度だけ生成する)
		std::unique_ptr<Obj3D> obj;
		Vector3 position;
		Vector3 velocity;
		float lifeTime = 0.0f;
		// 発射してから飛んだ距離(射程を過ぎたかの判定に使う)
		float traveledDistance = 0.0f;
		// 使用中かどうか(false なら未使用で、次の発射に使い回せる)
		bool isAlive = false;
		// 的・敵に当たるか
		bool canHit = true;
	};

	Obj3dCommon* objCommon_ = nullptr;

	// 弾に使う球モデル(当たり判定で境界球を求めるのに使う)
	Model* model_ = nullptr;

	// 使い回す弾の実体(サイズは kMaxBulletCount で固定)
	std::vector<Bullet> bullets_;

	// 弾の着地を調べる処理
	LandingHandler landingHandler_;

	// 弾のばらつきに使う乱数生成器
	std::mt19937 randomEngine_;

	// 狙う方向を、ばらつきの角度の範囲でランダムにずらした向きを返す
	Vector3 ApplySpread(const Vector3& direction);

	// 弾の移動速度(1秒あたりの移動量)
	static constexpr float kSpeed = 40.0f;
	// 弾の表示スケール(当たり判定の大きさにも使う)
	static constexpr float kScale = 0.15f;
	// 見た目を筋にするための、表示スケールに掛ける太さと長さの割合(当たり判定には使わない)
	static constexpr float kStreakWidthRate = 0.35f;
	static constexpr float kStreakLengthRate = 3.0f;
	// 弾が的に当たらなかった場合に消滅するまでの生存時間(秒)
	static constexpr float kLifeTime = 2.0f;
	// 射程(この距離まではまっすぐ飛び、重力も受けない)
	static constexpr float kRange = 20.0f;
	// 射程を過ぎた弾に適用する重力加速度(1秒あたりの下方向への速度変化量。すぐ落ちて射程が分かるよう強めにする)
	static constexpr float kFallGravity = 40.0f;
	// 射程を過ぎた弾の勢いの落ち方(1秒あたりに速度が減る割合。大きいほど射程の先へ伸びずに落ちる)
	static constexpr float kFallDrag = 4.0f;
	// 撃つたびに向きをずらす最大の角度(ラジアン)。横には大きく、縦には小さくばらけさせる
	// (射程 20 の先で横に最大 0.3、縦に最大 0.1 ずれる。的の当たり判定の半径より小さくして、狙った的から外れないようにする)
	static constexpr float kSpreadAngleHorizontal = 0.015f;
	static constexpr float kSpreadAngleVertical = 0.005f;
	// 使い回す弾の最大数(連射の間隔 0.1 秒・生存時間 2 秒で同時に飛ぶ最大 20 発に余裕を持たせた数)
	static constexpr size_t kMaxBulletCount = 24;
	// 弾1発が敵に与えるダメージ量
	static constexpr int kDamage = 1;
	// 弾が面に着いたときに塗るインクの半径
	static constexpr float kLandingInkRadius = 0.5f;
};
