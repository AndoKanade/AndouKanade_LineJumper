#pragma once

#include "MyMath.h"
#include "Model.h"
#include <memory>
#include <string>

// 前方宣言
class Obj3D;
class Obj3dCommon;
class PlayerBulletManager;

/// <summary>
/// 最後のステージのボス
/// ループするレールの輪の中心に浮かび、プレイヤーはその周りを回りながら撃つ。
/// 弾を当てて体力を0にすると倒せる。体力などの数値は GlobalVariables のグループ「Boss」の調整項目で変えられる。
/// </summary>
class Boss{
public:
	Boss();
	~Boss();

	/// <summary>
	/// 初期化処理(モデルの生成と、調整項目のGlobalVariablesへの登録)
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	void Initialize(Obj3dCommon* objCommon);

	// 戦う前の状態(体力満タン)に戻す
	void Reset();

	/// <summary>
	/// 位置・向きの更新と、プレイヤーの弾との当たり判定
	/// </summary>
	/// <param name="isBossStage">ボスのいるステージ(最後のステージ)か。違うステージでは何もせず、描画もしない</param>
	/// <param name="playerPosition">プレイヤーの座標(ボスの正面を向けるのに使う)</param>
	/// <param name="bullets">当たり判定を行うプレイヤーの弾</param>
	/// <param name="canBeHit">弾が当たるか(プレイ可能な間だけ true)</param>
	/// <param name="deltaTime">経過時間(秒)。0を渡すと上下の揺れを止める</param>
	void Update(bool isBossStage,const Vector3& playerPosition,PlayerBulletManager& bullets,bool canBeHit,float deltaTime);

	/// <summary>
	/// レティクルの方向が当たり判定の球を通っているかを調べる
	/// 弾をボスへ向けて撃つのと、レティクルの中心を赤くするのに使う
	/// </summary>
	/// <param name="cameraPosition">カメラの位置</param>
	/// <param name="cameraForward">カメラの前方ベクトル(正規化済み。画面中央のレティクルの方向)</param>
	/// <param name="minDistance">カメラの前方ベクトルに沿った距離の下限(これより手前にあるときは狙えていないものとする)</param>
	/// <param name="outDistance">カメラからボスの手前の面までの、前方ベクトルに沿った距離</param>
	/// <returns>狙えているときtrue</returns>
	bool FindAimed(const Vector3& cameraPosition,const Vector3& cameraForward,float minDistance,float& outDistance) const;

	// ボスのいるステージで、まだ倒されていないときに描画する
	void Draw();

	// ボスのいるステージで、まだ倒されていないか(当たり判定・狙いの対象になるか)
	bool IsActive() const{ return isBossStage_ && !isDefeated_; }
	// 体力が0になって倒されたかどうか
	bool IsDefeated() const{ return isDefeated_; }
	// 現在の座標(体の中心)を取得
	const Vector3& GetPosition() const{ return position_; }
	// 現在の体力と、体力の最大値を取得(体力の表示に使う)
	int GetHp() const{ return hp_; }
	int GetMaxHp() const{ return maxHp_; }

private:
	// 当たり判定に使う、モデルから求めたワールド座標系の境界球を取得
	Model::BoundingSphere GetHitSphere() const;

	// 描画用の3Dオブジェクト
	std::unique_ptr<Obj3D> obj_;
	// ボスのモデル(当たり判定で境界球を求めるのに使う)
	Model* model_ = nullptr;

	// 現在の座標(体の中心)
	Vector3 position_ = {0.0f, 0.0f, 0.0f};
	// 表示スケール(Update()で調整項目から読む。当たり判定の球にも使う)
	float scale_ = kDefaultScale;
	// 体力の最大値(Reset()で調整項目から読み直す)と、現在の体力
	int maxHp_ = kDefaultPhase1Hp;
	int hp_ = kDefaultPhase1Hp;
	// ボスのいるステージか(Update()で受け取る)
	bool isBossStage_ = false;
	// 体力が0になって倒されたかどうか
	bool isDefeated_ = false;
	// 上下の揺れに使う経過時間(秒)
	float bobTimer_ = 0.0f;

	// 調整項目の初期値(保存済みJSONがあればそちらが優先される)
	// 1形態目の体力
	static constexpr int kDefaultPhase1Hp = 120;
	// 体の中心を置く位置(ステージ4の輪の中心の少し上)
	static constexpr Vector3 kDefaultPosition = {-60.0f, 6.5f, 0.0f};
	// 表示スケール(モデルの体の半径は1のため、そのまま体の半径になる)
	static constexpr float kDefaultScale = 3.0f;
	// 上下の揺れの幅と速さ(1秒あたりに進む角度、ラジアン)
	static constexpr float kDefaultBobAmplitude = 0.4f;
	static constexpr float kDefaultBobSpeed = 1.2f;

	// 体力の最大値として設定できる下限(0以下にすると戦う前に倒れてしまうため)
	static constexpr int kMinMaxHp = 1;
	// 表示スケールとして設定できる下限(0以下にすると見えず、当たらなくなるため)
	static constexpr float kMinScale = 0.1f;
	// 体力が満タンのときと、残りわずかのときの色(当てるほど赤くなり、効いていることが分かるようにする。ゴールの的と同じ色)
	static constexpr Vector4 kFullHpColor = {1.0f, 0.85f, 0.2f, 1.0f};
	static constexpr Vector4 kLowHpColor = {1.0f, 0.2f, 0.25f, 1.0f};
	// プレイヤーとボスが水平方向で重なっているとみなす距離(向きが定まらないため正面を向け直さない)
	static constexpr float kMinFaceDistance = 0.001f;
};
