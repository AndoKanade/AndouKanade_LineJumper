#pragma once

#include "MyMath.h"
#include "Model.h"
#include <memory>
#include <string>

// 前方宣言
class Obj3D;
class Obj3dCommon;
class RailEditor;
class PlayerBulletManager;

/// <summary>
/// ゴールの的
/// プレイヤーがゴールのレールに乗ると現れ、プレイヤーより決まった距離だけ先をレールに沿って並走する。
/// プレイヤーとの距離がいつも同じなので置いていかれず、壊し損ねることがない。
/// 普通の的より固く、何発も当てて体力を0にするとステージクリアになる。
/// </summary>
class GoalTarget{
public:
	GoalTarget();
	~GoalTarget();

	/// <summary>
	/// 初期化処理(モデルの生成と、調整項目のGlobalVariablesへの登録)
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	/// <param name="paramGroup">調整項目を登録するGlobalVariablesのグループ名</param>
	void Initialize(Obj3dCommon* objCommon,const std::string& paramGroup);

	// 現れる前の状態(体力満タン・非表示)に戻す
	void Reset();

	/// <summary>
	/// 並走する位置の更新と、弾との当たり判定
	/// </summary>
	/// <param name="railEditor">プレイヤーが進んでいるレール</param>
	/// <param name="playerRailT">プレイヤーのレールの進行度</param>
	/// <param name="isOnGoalRail">プレイヤーがゴールのレールに乗っているか(乗ったら現れ、乗っている間だけ並走する)</param>
	/// <param name="cameraPosition">カメラの位置(的の模様の面をカメラへ向けるのに使う)</param>
	/// <param name="bullets">当たり判定を行うプレイヤーの弾</param>
	/// <param name="deltaTime">経過時間(秒)。0を渡すと揺れを止める</param>
	void Update(const RailEditor* railEditor,float playerRailT,bool isOnGoalRail,const Vector3& cameraPosition,PlayerBulletManager& bullets,float deltaTime);

	/// <summary>
	/// レティクルの方向が当たり判定の球を通っているかを調べる
	/// 弾をこの的へ向けて撃つのと、レティクルの中心を赤くするのに使う
	/// </summary>
	/// <param name="cameraPosition">カメラの位置</param>
	/// <param name="cameraForward">カメラの前方ベクトル(正規化済み。画面中央のレティクルの方向)</param>
	/// <param name="minDistance">カメラの前方ベクトルに沿った距離の下限(これより手前にあるときは狙えていないものとする)</param>
	/// <param name="outDistance">カメラからこの的までの、前方ベクトルに沿った距離</param>
	/// <returns>狙えているときtrue</returns>
	bool FindAimed(const Vector3& cameraPosition,const Vector3& cameraForward,float minDistance,float& outDistance) const;

	// 現れていて、まだ壊れていないときに描画する
	void Draw();

	// 体力が0になって壊れたかどうか
	bool IsDestroyed() const{ return isDestroyed_; }
	// 現在の座標を取得(壊れたときの破片の演出の発生位置に使う)
	const Vector3& GetPosition() const{ return position_; }

private:
	/// <summary>
	/// プレイヤーの進行度から、レールに沿って並走する距離だけ先の位置と、そこでの進行方向を求める
	/// レールの終点を越える分は、終点の進行方向へまっすぐ伸ばす(プレイヤーが終点で止まっても、的は先に離れたままにする)
	/// </summary>
	/// <param name="railEditor">プレイヤーが進んでいるレール</param>
	/// <param name="playerRailT">プレイヤーのレールの進行度</param>
	/// <param name="outForward">求めた位置での進行方向</param>
	/// <returns>レールの上の、並走する距離だけ先の位置(高さ・揺れを足す前)</returns>
	Vector3 ComputeAheadPosition(const RailEditor* railEditor,float playerRailT,Vector3& outForward) const;

	// 当たり判定に使う、モデルから求めたワールド座標系の境界球を取得
	Model::BoundingSphere GetHitSphere() const;

	// 描画用の3Dオブジェクト
	std::unique_ptr<Obj3D> obj_;
	// 的に使う円盤モデル(当たり判定で境界球を求めるのに使う)
	Model* model_ = nullptr;

	// 調整項目を登録したGlobalVariablesのグループ名
	std::string paramGroup_;

	// 現在の座標
	Vector3 position_ = {0.0f, 0.0f, 0.0f};
	// 体力の最大値(Reset()で調整項目から読み直す)と、現在の体力
	int maxHp_ = kDefaultMaxHp;
	int hp_ = kDefaultMaxHp;
	// プレイヤーがゴールのレールに乗って、現れたかどうか
	bool isAppeared_ = false;
	// 体力が0になって壊れたかどうか
	bool isDestroyed_ = false;
	// 現れてからの経過時間(秒。左右・上下の揺れに使う)
	float swayTimer_ = 0.0f;

	// 体力の最大値の初期値(保存済みJSONがあればそちらが優先される)
	static constexpr int kDefaultMaxHp = 20;
	// 体力の最大値として設定できる下限(0以下にすると現れた瞬間に壊れてしまうため)
	static constexpr int kMinMaxHp = 1;
	// プレイヤーより先を並走する距離の初期値(レールに沿った距離。保存済みJSONがあればそちらが優先される)
	static constexpr float kDefaultAheadDistance = 10.0f;
	// レールからの高さの初期値(保存済みJSONがあればそちらが優先される)
	static constexpr float kDefaultHeight = 1.5f;
	// 並走する位置をレールに沿って求めるときに、1回で進める距離(長い距離を一度に進めると曲がったところでずれるため、細かく分けて進める)
	static constexpr float kAheadStepDistance = 1.0f;
	// 1回で進めた距離がこれより短いときは、それ以上進めないものとして打ち切る(進めずに同じ場所を回り続けないようにする)
	static constexpr float kMinAheadStepLength = 1e-4f;
	// レールの終端の進行度
	static constexpr float kRailEndT = 1.0f;
	// 左右の揺れの幅と速さ(1秒あたりに進む角度、ラジアン)
	static constexpr float kSwaySideAmplitude = 2.0f;
	static constexpr float kSwaySideSpeed = 1.3f;
	// 上下の揺れの幅と速さ(左右と違う速さにして、同じ動きの繰り返しに見えにくくする)
	static constexpr float kSwayUpAmplitude = 0.8f;
	static constexpr float kSwayUpSpeed = 2.1f;
	// 表示スケール(普通の的(0.4)より大きくして、最後の的だと分かるようにする)
	static constexpr float kScale = 1.0f;
	// 体力が満タンのときと、残りわずかのときの色(当てるほど赤くなり、効いていることが分かるようにする)
	static constexpr Vector4 kFullHpColor = {1.0f, 0.85f, 0.2f, 1.0f};
	static constexpr Vector4 kLowHpColor = {1.0f, 0.2f, 0.25f, 1.0f};
	// カメラと的が重なっているとみなす距離(向きが定まらないため模様の面を向けない)
	static constexpr float kMinFaceDistance = 0.001f;
};
