#pragma once
#include "MyMath.h"
#include "Model.h"
#include "EnemyBulletPool.h"
#include <memory>
#include <vector>

// クラスの前方宣言
class Obj3D;
class Obj3dCommon;
class EnemyAttackState;

/// <summary>
/// 雑魚敵
///
/// 指定された基準座標を中心に、決められた方向へ一定距離を往復する固定パターンで移動する。
/// プレイヤーが検知範囲内に入るとそちらを向き、弾が当たると撃破される。
/// 検知中は一定間隔でプレイヤーへ向けて弾を発射する。
/// 弾の撃ち方は今の攻撃の状態(EnemyAttackState を継承したクラス)に任せ、撃つたびに状態が決めた次の攻撃へ切り替える。
/// </summary>
class Enemy{
public:
	Enemy();
	~Enemy();

	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	/// <param name="basePosition">往復移動の中心となるワールド座標</param>
	/// <param name="patrolDirection">往復移動を行う方向(内部で正規化する)</param>
	/// <param name="maxHp">体力の最大値(この値からのHPで開始する)</param>
	void Initialize(Obj3dCommon* objCommon,const Vector3& basePosition,const Vector3& patrolDirection,int maxHp);

	/// <summary>
	/// 更新処理
	/// </summary>
	/// <param name="playerPosition">プレイヤーのワールド座標(検知・向きの計算に使用)</param>
	/// <param name="deltaTime">経過時間(秒)。0を渡すと移動せず表示更新のみ行う</param>
	void Update(const Vector3& playerPosition,float deltaTime);

	// 描画処理(撃破済みのときは本体を描画しない。発射済みの弾は残っていれば描画する)
	void Draw();

	// 初期状態(生存・移動位置の中心)へ戻す(Play開始時のリセット用)
	void Reset();

	/// <summary>
	/// 体力を減らす(撃破済みのときは何もしない)
	/// </summary>
	/// <param name="damage">減らす体力の量</param>
	/// <returns>このダメージで撃破されたときtrue</returns>
	bool TakeDamage(int damage);

	// 現在の体力を取得
	int GetHp() const{ return hp_; }
	// 体力の最大値を取得
	int GetMaxHp() const{ return maxHp_; }

	// 配置エディターからの編集内容を反映するための設定関数
	// 往復移動の中心座標を設定する(現在の往復位置を保ったまま移動させる)
	void SetBasePosition(const Vector3& basePosition);
	// 往復移動の方向を設定する(内部で正規化する)
	void SetPatrolDirection(const Vector3& patrolDirection);
	// 体力の最大値を設定する(現在の体力が最大値を超える場合は最大値に合わせる)
	void SetMaxHp(int maxHp);

	/// <summary>
	/// 発射済みの弾とプレイヤーの当たり判定を行い、命中した弾を消滅させる
	/// </summary>
	/// <param name="playerSphere">プレイヤーのモデルから求めたワールド座標系の境界球</param>
	/// <returns>このフレームでプレイヤーに命中した弾の数</returns>
	int CheckHitToPlayer(const Model::BoundingSphere& playerSphere);

	// 発射済みで生存している弾の数を取得(デバッグ表示用)
	int GetActiveBulletCount() const;

	// 現在のワールド座標を取得(当たり判定・撃破演出の発生位置に使用)
	const Vector3& GetPosition() const{ return position_; }
	// 生存しているかどうかを取得
	bool IsAlive() const{ return isAlive_; }
	// プレイヤーを検知しているかどうかを取得(デバッグ表示用)
	bool IsDetectingPlayer() const{ return isDetectingPlayer_; }
	// 当たり判定用に、モデルから求めたワールド座標系の境界球を取得
	Model::BoundingSphere GetHitSphere() const;

private:
	// 現在の攻撃の状態に発射を任せる(発射位置とプレイヤーへの向きはすべての攻撃で共通のため、ここで求めて渡す)
	void FireCurrentAttack(const Vector3& playerPosition);
	// 弾の発射位置(敵の胸の高さ)を取得する
	Vector3 GetBulletSpawnPosition() const;

	std::unique_ptr<Obj3D> obj_; // 描画用の3Dオブジェクト

	// 敵弾の管理用
	EnemyBulletPool bulletPool_;   // 使い回す弾(移動・追尾・寿命・当たり判定・描画を任せる。ボスと共用のクラス)
	float shotTimer_ = 0.0f;       // 次の発射までの残り時間(秒)

	// 現在の攻撃の状態(撃ち方と発射間隔を持つ。撃つたびに状態が決めた次の攻撃へ切り替える)
	std::unique_ptr<EnemyAttackState> attackState_;

	Vector3 basePosition_ = {0.0f, 0.0f, 0.0f};    // 往復移動の中心座標
	Vector3 patrolDirection_ = {1.0f, 0.0f, 0.0f}; // 往復移動の方向(正規化済み)
	Vector3 position_ = {0.0f, 0.0f, 0.0f};        // 現在のワールド座標

	float patrolOffset_ = 0.0f; // 中心座標からのずれ(往復のたびに符号が反転する)
	float patrolSign_ = 1.0f;   // 現在の進行向き(+1で正方向、-1で逆方向)
	float rotationY_ = 0.0f;    // 現在のY軸回転(向き)

	bool isAlive_ = true;             // 生存フラグ(falseで撃破済み)
	bool isDetectingPlayer_ = false;  // プレイヤーを検知しているかどうか

	// 敵の体力(0になると撃破される)
	int maxHp_ = kDefaultMaxHp; // 体力の最大値(配置エディターで敵ごとに設定できる)
	int hp_ = kDefaultMaxHp;    // 現在の体力

	// 往復移動の速度(1秒あたりの移動量)
	static constexpr float kMoveSpeed = 3.0f;
	// 中心座標から片側へ動ける距離(この距離に達すると折り返す)
	static constexpr float kPatrolRange = 5.0f;
	// プレイヤーを検知する距離(この距離以内ならプレイヤーの方を向く)
	static constexpr float kDetectionRange = 25.0f;
	// 表示スケール
	static constexpr float kScale = 0.4f;

	// 体力の最大値の初期値(配置エディターで指定が無いときに使う)
	static constexpr int kDefaultMaxHp = 3;
	// 体力の最大値として設定できる下限(0以下にすると生成直後に撃破されてしまうため)
	static constexpr int kMinMaxHp = 1;

	// 同時に存在できる弾の最大数(この数だけ初期化時に生成して使い回す)
	static constexpr size_t kMaxBulletCount = 16;
	// 弾が消滅するまでの生存時間(秒)
	static constexpr float kBulletLifeTime = 4.0f;
	// 弾の表示スケール
	static constexpr float kBulletScale = 0.2f;
	// 弾の発射位置を敵の足元より上へずらす量(胸の高さから撃っているように見せる)
	static constexpr float kBulletSpawnUpOffset = 0.5f;

	// 追尾弾が1秒あたりにプレイヤー方向へ向きを補正する割合(大きいほど急に曲がる。弾の使い回しに渡す)
	static constexpr float kHomingTurnRate = 1.5f;
};