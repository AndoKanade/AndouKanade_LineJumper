#pragma once
#include "MyMath.h"
#include <memory>

// 前方宣言
class EnemyBulletPool;

/// <summary>
/// 雑魚敵の攻撃行動(状態)の基底クラス
/// 雑魚敵(Enemy)は今の攻撃の状態を1つだけ持ち、撃つ時が来たら弾の撃ち方と次に撃つまでの間隔をこの状態に任せる。
/// 撃ち終えたら、次に使う攻撃の状態を CreateNext() で状態自身が決める(単発 → 3方向拡散 → 追尾弾 → 単発…の順に回る)。
/// </summary>
class EnemyAttackState{
public:
	virtual ~EnemyAttackState() = default;

	/// <summary>
	/// この攻撃の撃ち方で弾を発射する
	/// </summary>
	/// <param name="bulletPool">弾を撃ち出す敵弾の使い回し</param>
	/// <param name="spawnPosition">発射位置(敵の胸の高さ)</param>
	/// <param name="baseDirection">発射位置からプレイヤーへ向かう方向(正規化済み)</param>
	virtual void Fire(EnemyBulletPool& bulletPool,const Vector3& spawnPosition,const Vector3& baseDirection) const = 0;

	// この攻撃を撃つまでの待ち時間(秒)
	virtual float GetShotInterval() const = 0;

	// この攻撃を撃ち終えた後に使う攻撃の状態を生成する
	virtual std::unique_ptr<EnemyAttackState> CreateNext() const = 0;

protected:
	// 単発・3方向拡散ショットの弾の移動速度(1秒あたりの移動量)
	static constexpr float kBulletSpeed = 18.0f;
};

/// <summary>
/// 単発: 発射した瞬間のプレイヤー方向へ直進する弾を1発撃つ(次は3方向拡散)
/// </summary>
class SingleShotAttackState : public EnemyAttackState{
public:
	void Fire(EnemyBulletPool& bulletPool,const Vector3& spawnPosition,const Vector3& baseDirection) const override;
	float GetShotInterval() const override{ return kShotInterval; }
	std::unique_ptr<EnemyAttackState> CreateNext() const override;

private:
	// 単発の発射間隔(秒)
	static constexpr float kShotInterval = 1.2f;
};

/// <summary>
/// 3方向拡散: プレイヤー方向を中心に、左右へ角度をつけた弾を同時に撃つ(次は追尾弾)
/// </summary>
class SpreadShotAttackState : public EnemyAttackState{
public:
	void Fire(EnemyBulletPool& bulletPool,const Vector3& spawnPosition,const Vector3& baseDirection) const override;
	float GetShotInterval() const override{ return kShotInterval; }
	std::unique_ptr<EnemyAttackState> CreateNext() const override;

private:
	// 同時に発射する弾数(左右対称にするため奇数にする)
	static constexpr int kBulletCount = 3;
	// 隣の弾との間につける角度(ラジアン、約12度)
	static constexpr float kSpreadAngle = 0.21f;
	// 発射間隔(秒)。単発より弾数が多いため長めにする
	static constexpr float kShotInterval = 2.0f;
};

/// <summary>
/// 追尾弾: 発射後もプレイヤー方向へ少しずつ向きを変え続ける弾を1発撃つ(次は単発)
/// </summary>
class HomingShotAttackState : public EnemyAttackState{
public:
	void Fire(EnemyBulletPool& bulletPool,const Vector3& spawnPosition,const Vector3& baseDirection) const override;
	float GetShotInterval() const override{ return kShotInterval; }
	std::unique_ptr<EnemyAttackState> CreateNext() const override;

private:
	// 追尾弾の移動速度(1秒あたりの移動量)。避けられる余地を残すため通常弾より遅くする
	static constexpr float kHomingBulletSpeed = 12.0f;
	// 発射間隔(秒)。避けにくい攻撃なので最も長めにする
	static constexpr float kShotInterval = 2.5f;
};
