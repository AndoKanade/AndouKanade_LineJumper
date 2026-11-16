#pragma once
#include "MyMath.h"
#include "Model.h"
#include <memory>
#include <vector>

// クラスの前方宣言
class Obj3D;
class Obj3dCommon;

/// <summary>
/// 敵の弾の使い回し(雑魚敵 Enemy とボス Boss で共用する)
///
/// 発射のたびに生成・破棄すると無駄が多いため、初期化時に必要数だけ作っておき使い回す。
/// 弾は等速でまっすぐ飛び、追尾弾は毎フレーム少しずつプレイヤーの方へ向きを変える。決めた時間がたつと未使用に戻る。
/// </summary>
class EnemyBulletPool{
public:
	// 弾の作り(初期化時に決める)
	struct Settings{
		size_t count = 0;                             // 同時に存在できる弾の最大数(この数だけ初期化時に生成して使い回す)
		float scale = 1.0f;                           // 弾の表示スケール(当たり判定の球にも使う)
		float lifeTime = 0.0f;                        // 弾が消滅するまでの生存時間(秒)
		float homingTurnRate = 0.0f;                  // 追尾弾が1秒あたりにプレイヤー方向へ向きを補正する割合(大きいほど急に曲がる)
		Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};     // 弾の色
	};

	EnemyBulletPool();
	~EnemyBulletPool();

	/// <summary>
	/// 初期化処理(弾のモデルの読み込みと、使い回す弾の生成)
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	/// <param name="settings">弾の作り</param>
	void Initialize(Obj3dCommon* objCommon,const Settings& settings);

	// 発射済みの弾をすべて未使用に戻す
	void Clear();

	/// <summary>
	/// 弾の更新処理(移動・追尾の向き補正・寿命切れの判定・描画用トランスフォームの更新)
	/// </summary>
	/// <param name="playerPosition">プレイヤーの座標(追尾弾の向き補正に使う)</param>
	/// <param name="deltaTime">経過時間(秒)。0なら動かずに表示だけ更新する</param>
	void Update(const Vector3& playerPosition,float deltaTime);

	/// <summary>
	/// 指定した方向へ弾を1発発射する(未使用の弾が無いときは何もしない)
	/// </summary>
	/// <param name="spawnPosition">発射位置</param>
	/// <param name="direction">飛ばす方向(内部で正規化する)</param>
	/// <param name="speed">移動速度(1秒あたりの移動量)</param>
	/// <param name="isHoming">追尾するかどうか</param>
	/// <param name="damage">当たったときに減らす体力の量(弾ごとに変えないときは省略できる)</param>
	void Fire(const Vector3& spawnPosition,const Vector3& direction,float speed,bool isHoming,float damage = 0.0f);

	/// <summary>
	/// 発射済みの弾とプレイヤーの当たり判定(弾とプレイヤーの境界球が重なったら、その弾を未使用に戻す)
	/// </summary>
	/// <param name="playerSphere">プレイヤーのワールド座標系の境界球</param>
	/// <param name="outMaxDamage">命中した弾のうち、一番大きいダメージを入れる先(いらないときは nullptr)</param>
	/// <returns>命中した弾の数</returns>
	int CheckHitToPlayer(const Model::BoundingSphere& playerSphere,float* outMaxDamage = nullptr);

	// 発射中の弾を描画する
	void Draw();

	// 発射済みで生存している弾の数を取得(デバッグ表示用)
	int GetActiveCount() const;

	// 追尾弾の曲がりやすさを変える(調整項目の変更を反映するのに使う)
	void SetHomingTurnRate(float homingTurnRate){ settings_.homingTurnRate = homingTurnRate; }

private:
	// 弾1発分のデータ
	struct Bullet{
		std::unique_ptr<Obj3D> obj;            // 描画用の3Dオブジェクト(初期化時に一度だけ生成する)
		Vector3 position = {0.0f, 0.0f, 0.0f}; // 現在のワールド座標
		Vector3 velocity = {0.0f, 0.0f, 0.0f}; // 1秒あたりの移動量
		float lifeTime = 0.0f;                 // 発射してからの経過時間(秒)
		bool isAlive = false;                  // 使用中かどうか(falseなら未使用=再利用できる)
		float speed = 0.0f;                    // 移動速度(向きを変えても速さを保つために保持する)
		bool isHoming = false;                 // 追尾するかどうか(trueなら毎フレーム向きを補正する)
		float damage = 0.0f;                   // 当たったときに減らす体力の量
	};

	// 弾の作り
	Settings settings_;
	// 弾のモデル(当たり判定で弾の境界球を求めるのに使う)
	Model* bulletModel_ = nullptr;
	// 使い回す弾の実体(サイズはsettings_.countで固定)
	std::vector<Bullet> bullets_;

	// 向きが定まらないとみなすベクトルの長さ(プレイヤーと弾が重なっているときは追尾の向き補正をしない)
	static constexpr float kMinDirectionLength = 1e-5f;
	// 追尾弾が1フレームで向きを補正する割合の上限(これを超えると一気に向きが切り替わってしまう)
	static constexpr float kMaxHomingTurnRatio = 1.0f;
};
