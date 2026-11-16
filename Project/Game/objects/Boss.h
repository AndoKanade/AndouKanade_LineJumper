#pragma once

#include "MyMath.h"
#include "Model.h"
#include "EnemyBulletPool.h"
#include <memory>
#include <string>

// 前方宣言
class Obj3D;
class Obj3dCommon;
class PlayerBulletManager;

/// <summary>
/// 最後のステージのボス
/// ループするレールの輪の中心に浮かび、プレイヤーはその周りを回りながら撃つ。
/// プレイヤーが輪に乗っている間、先読みの連射 → 扇状の弾 → 追尾弾 の順に撃ってくる。
/// 1形態目の体力を0にすると2形態目に変わり、内側の輪が現れ、プレイヤーが乗っている輪を塗り返す攻撃が加わる(別の輪へ飛び移って避ける)。
/// 2形態目の体力を0にすると倒せる。体力・攻撃の間隔・弾の速さなどの数値は GlobalVariables のグループ「Boss」の調整項目で変えられる。
/// </summary>
class Boss{
public:
	// プレイヤーが乗っている輪を塗り返す攻撃の段階
	enum class RingAttackState{
		None,    // 攻撃していない
		Warning, // 予告(輪を点滅させる。まだ当たらない)
		Danger,  // 攻撃中(輪が敵のインクで塗られていて、乗っていると被弾する)
	};

	Boss();
	~Boss();

	/// <summary>
	/// 初期化処理(モデルと弾の生成、調整項目のGlobalVariablesへの登録)
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	void Initialize(Obj3dCommon* objCommon);

	// 戦う前の状態(1形態目・体力満タン・弾なし)に戻す
	void Reset();
	// 指定した形態の最初の状態(体力満タン・弾なし)にする(コンティニューで、残機が尽きたときの形態から戦い直すのに使う)
	void ResetToPhase(int phase);
	// 体力は残したまま、撃った弾を消して攻撃を最初の待ち時間からやり直す(プレイヤーが残機を減らしてやり直すときに使う)
	void ResetAttack();

	/// <summary>
	/// 位置・向き・攻撃の更新と、プレイヤーの弾との当たり判定
	/// </summary>
	/// <param name="isBossStage">ボスのいるステージ(最後のステージ)か。違うステージでは何もせず、描画もしない</param>
	/// <param name="playerPosition">プレイヤーの座標(ボスの正面を向けるのと、弾を撃つ先を決めるのに使う)</param>
	/// <param name="bullets">当たり判定を行うプレイヤーの弾</param>
	/// <param name="canBeHit">弾が当たるか(プレイ可能な間だけ true)</param>
	/// <param name="canAttack">攻撃するか(プレイヤーが輪に乗っている間だけ true)</param>
	/// <param name="playerRailIndex">プレイヤーが乗っている輪のレールの番号(輪に乗っていないときは -1。輪を塗り返す攻撃の対象にする)</param>
	/// <param name="deltaTime">経過時間(秒)。0を渡すと上下の揺れ・弾・攻撃を止める</param>
	void Update(bool isBossStage,const Vector3& playerPosition,PlayerBulletManager& bullets,bool canBeHit,bool canAttack,int playerRailIndex,float deltaTime);

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

	/// <summary>
	/// ボスの弾とプレイヤーの当たり判定(当たった弾は消える)
	/// </summary>
	/// <param name="playerSphere">プレイヤーのワールド座標系の境界球</param>
	/// <returns>命中した弾の数</returns>
	int CheckHitToPlayer(const Model::BoundingSphere& playerSphere);

	// ボスのいるステージで、まだ倒されていないときに本体を描画し、撃った弾を描画する
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
	// 飛んでいる弾の数を取得(デバッグ表示用)
	int GetActiveBulletCount() const{ return bulletPool_.GetActiveCount(); }

	// 今の形態(1 か 2)を取得
	int GetPhase() const{ return phase_; }
	// 2形態目に変わったか(変わる演出の最中も含む。内側の輪を出すかの判定に使う)
	bool IsSecondPhase() const{ return phase_ >= kSecondPhase; }
	// このフレームで2形態目に変わり始めたかを受け取る(1回読むと消える。GameSceneが演出を出すのに使う)
	bool ConsumePhaseChanged();
	// 2形態目で現れる輪のレールの番号(調整項目)を取得
	int GetPhase2RailIndex() const;
	// 今の形態でプレイヤーが輪を回る速さに掛ける倍率を取得(2形態目は調整項目の倍率、1形態目は1)
	float GetRailSpeedRate() const;

	// 輪を塗り返す攻撃の段階・対象のレールの番号を取得
	RingAttackState GetRingAttackState() const{ return ringAttackState_; }
	int GetRingAttackRailIndex() const{ return ringAttackRailIndex_; }
	// 輪を塗り返す攻撃で、対象の輪の色を寄せる先の色と割合を取得(予告は白く点滅、攻撃中は赤)
	Vector4 GetRingAttackColor() const;
	float GetRingAttackTintRate() const;

private:
	// 攻撃の種類(1回の攻撃を終えるたびに次の種類へ切り替える)
	enum class AttackType{
		Burst,   // 先読みの連射: プレイヤーが進む先を狙った弾を、短い間隔で続けて撃つ
		Fan,     // 扇状の弾: プレイヤーが進む先を中心に、左右へ角度をつけた弾を同時に撃つ
		Homing,  // 追尾弾: 発射後もプレイヤーの方へ少しずつ向きを変え続ける、遅い弾を撃つ
		RingInk, // 輪の塗り返し(2形態目だけ): プレイヤーが乗っている輪を予告のあと敵のインクで塗り、乗っていると被弾させる
	};

	// 当たり判定に使う、体の球をワールド座標系に置いた球を取得
	Model::BoundingSphere GetHitSphere() const;
	// プレイヤーの水平方向の速さを、前のフレームからの移動量で求める(先読みに使う)
	void UpdatePlayerVelocity(const Vector3& playerPosition,float deltaTime);
	// 攻撃の待ち時間を進め、時間が来たら今の種類の攻撃を撃つ
	void UpdateAttack(const Vector3& playerPosition,bool canAttack,int playerRailIndex,float deltaTime);
	// 輪を塗り返す攻撃の予告・攻撃中の時間を進める
	void UpdateRingAttack(float deltaTime);
	// 1形態目の体力が尽きたときに、2形態目へ変わり始める
	void BeginPhaseChange();
	// 今の形態の攻撃の順番の、index番目の種類を取得(末尾まで来たら先頭へ戻る)
	AttackType GetAttackType(size_t index) const;
	// 今の形態の攻撃の順番の要素数を取得
	size_t GetAttackOrderCount() const;
	// 速さbulletSpeedの弾が届くころにプレイヤーがいる位置(先読みした狙う点)を求める
	Vector3 ComputeLeadTarget(const Vector3& playerPosition,float bulletSpeed) const;
	// 体の表面から、狙う点へ向かう向きを左右にyawOffset(ラジアン)だけずらして弾を1発撃つ
	void FireToward(const Vector3& target,float yawOffset,float speed,bool isHoming);

	// 描画用の3Dオブジェクト
	std::unique_ptr<Obj3D> obj_;
	// 使い回す弾(移動・追尾・寿命・当たり判定・描画を任せる。雑魚敵と共用のクラス)
	EnemyBulletPool bulletPool_;

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

	// 今の攻撃の種類(kAttackOrderの何番目かを指す)
	size_t attackIndex_ = 0;
	// 次に撃つまでの残り時間(秒)
	float attackTimer_ = 0.0f;
	// 連射の途中で、まだ撃っていない残りの弾数(0なら連射の途中ではない)
	int burstShotsLeft_ = 0;
	// 前のフレームのプレイヤーの座標と、そこから求めた水平方向の速さ(1秒あたりの移動量)
	Vector3 previousPlayerPosition_ = {0.0f, 0.0f, 0.0f};
	Vector3 playerVelocity_ = {0.0f, 0.0f, 0.0f};
	// 前のフレームのプレイヤーの座標を覚えているか(覚える前は速さを求められない)
	bool hasPreviousPlayerPosition_ = false;

	// 今の形態(kFirstPhase か kSecondPhase)
	int phase_ = kFirstPhase;
	// 2形態目へ変わる演出の残り時間と、演出にかける時間(秒)。残り時間が0より大きい間は弾が当たらず、攻撃もしない
	float phaseChangeTimer_ = 0.0f;
	float phaseChangeDuration_ = 0.0f;
	// このフレームで2形態目に変わり始めたか(ConsumePhaseChanged()で読むと消える)
	bool hasPhaseChanged_ = false;

	// 弾を撃つ前の予備動作の進み具合(0: していない 〜 1: 撃つ直前。体をふくらませて白く光らせるのに使う)
	float telegraphRate_ = 0.0f;

	// 輪を塗り返す攻撃の段階・対象のレールの番号・今の段階に入ってからの経過時間(秒)
	RingAttackState ringAttackState_ = RingAttackState::None;
	int ringAttackRailIndex_ = kNoRingAttackRail;
	float ringAttackTimer_ = 0.0f;

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
	// 1回の攻撃を終えてから、次の攻撃を撃つまでの時間(秒)
	static constexpr float kDefaultAttackInterval = 1.6f;
	// 先読みの連射・扇状の弾の速さ(1秒あたりの移動量)
	static constexpr float kDefaultBulletSpeed = 16.0f;
	// 先読みの連射で続けて撃つ弾数と、その間隔(秒)
	static constexpr int kDefaultBurstCount = 3;
	static constexpr float kDefaultBurstInterval = 0.25f;
	// 扇状の弾で同時に撃つ弾数と、隣の弾との間につける角度(ラジアン、約7度)
	static constexpr int kDefaultFanCount = 5;
	static constexpr float kDefaultFanAngle = 0.12f;
	// 追尾弾の速さと、1秒あたりにプレイヤー方向へ向きを補正する割合(避けられる余地を残すため、ほかの弾より遅くする)
	static constexpr float kDefaultHomingSpeed = 9.0f;
	static constexpr float kDefaultHomingTurnRate = 1.2f;
	// 先読みの強さ(1でプレイヤーがそのまま進んだときに当たる位置、0で撃った瞬間の位置を狙う)
	static constexpr float kDefaultLeadRate = 1.0f;
	// 弾を撃つ前の予備動作の時間(秒。攻撃の間隔のうち、最後のこの時間だけ体をふくらませて白く光らせ、撃つタイミングを見せる)
	static constexpr float kDefaultTelegraphTime = 0.7f;
	// 2形態目の体力
	static constexpr int kDefaultPhase2Hp = 160;
	// 2形態目へ変わる演出にかける時間(秒。この間は弾が当たらず、攻撃もしない)
	static constexpr float kDefaultPhaseChangeDuration = 2.0f;
	// 2形態目で、体の中心を1形態目より上へ上げる量
	static constexpr float kDefaultPhase2Rise = 1.0f;
	// 2形態目で、1回の攻撃を終えてから次の攻撃を撃つまでの時間(秒。1形態目より短くする)
	static constexpr float kDefaultPhase2AttackInterval = 1.1f;
	// 2形態目で、プレイヤーが輪を回る速さに掛ける倍率
	static constexpr float kDefaultPhase2RailSpeedRate = 1.25f;
	// 2形態目で現れる輪のレールの番号(ステージ4の Rail 2)
	static constexpr int kDefaultPhase2RailIndex = 2;
	// 輪を塗り返す攻撃の、予告の時間と攻撃中の時間(秒)
	static constexpr float kDefaultRingWarnTime = 1.5f;
	static constexpr float kDefaultRingDangerTime = 2.5f;

	// 形態の番号
	static constexpr int kFirstPhase = 1;
	static constexpr int kSecondPhase = 2;
	// 輪を塗り返す攻撃の対象が無いことを表す値
	static constexpr int kNoRingAttackRail = -1;
	// 輪を塗り返す攻撃の予告で、輪を点滅させる速さ(1秒あたりに進む角度、ラジアン)
	static constexpr float kRingWarnBlinkSpeed = 20.0f;
	// 輪を塗り返す攻撃の予告と攻撃中に、輪の色を寄せる先の色(予告は白、攻撃中は敵のインクより目立つ赤)
	static constexpr Vector4 kRingWarnColor = {1.0f, 1.0f, 1.0f, 1.0f};
	static constexpr Vector4 kRingDangerColor = {1.0f, 0.1f, 0.1f, 1.0f};
	// 予備動作で、撃つ直前に体をふくらませる量(表示スケールに対する割合)と、体の色を寄せる先の色・寄せる割合の上限
	static constexpr float kTelegraphScaleRate = 0.18f;
	static constexpr Vector4 kTelegraphColor = {1.0f, 1.0f, 1.0f, 1.0f};
	static constexpr float kTelegraphColorRate = 0.8f;
	// 2形態目へ変わる演出の間に、体を一瞬ふくらませる量(表示スケールに対する割合)
	static constexpr float kPhaseChangeScalePulse = 0.25f;
	// 2形態目の、体力が満タンのときの色(1形態目の黄色と見分けられるよう紫にする。残りわずかの色は1形態目と同じ赤)
	static constexpr Vector4 kPhase2FullHpColor = {0.7f, 0.45f, 1.0f, 1.0f};
	// 円周率(2形態目へ変わる演出の、体のふくらみの計算に使う)
	static constexpr float kPi = 3.14159265f;

	// モデルの体の球の半径(tools/generate_boss_assets.py の BODY_RADIUS と合わせる。当たり判定に使う)
	static constexpr float kModelBodyRadius = 1.0f;
	// 体力の最大値として設定できる下限(0以下にすると戦う前に倒れてしまうため)
	static constexpr int kMinMaxHp = 1;
	// 表示スケールとして設定できる下限(0以下にすると見えず、当たらなくなるため)
	static constexpr float kMinScale = 0.1f;
	// 体力が満タンのときと、残りわずかのときの色(当てるほど赤くなり、効いていることが分かるようにする。ゴールの的と同じ色)
	static constexpr Vector4 kFullHpColor = {1.0f, 0.85f, 0.2f, 1.0f};
	static constexpr Vector4 kLowHpColor = {1.0f, 0.2f, 0.25f, 1.0f};
	// プレイヤーとボスが水平方向で重なっているとみなす距離(向きが定まらないため正面を向け直さない)
	static constexpr float kMinFaceDistance = 0.001f;

	// 1形態目の、攻撃を切り替える順番の要素数と、その順番(末尾まで来たら先頭へ戻る)
	static constexpr size_t kPhase1AttackOrderCount = 3;
	static constexpr AttackType kPhase1AttackOrder[kPhase1AttackOrderCount] = {
		AttackType::Burst,
		AttackType::Fan,
		AttackType::Homing
	};
	// 2形態目の、攻撃を切り替える順番の要素数と、その順番(輪の塗り返しを挟み、別の輪へ飛び移らせる)
	static constexpr size_t kPhase2AttackOrderCount = 5;
	static constexpr AttackType kPhase2AttackOrder[kPhase2AttackOrderCount] = {
		AttackType::Burst,
		AttackType::RingInk,
		AttackType::Fan,
		AttackType::Homing,
		AttackType::RingInk
	};
	// 同時に存在できる弾の最大数(1回りの攻撃で 3 + 5 + 1 発。寿命の間に次の回りの弾が重なっても足りる数にする)
	static constexpr size_t kMaxBulletCount = 32;
	// 弾の表示スケール(雑魚敵の弾(0.2)より大きくして、離れた輪の上からでも見えるようにする)
	static constexpr float kBulletScale = 0.35f;
	// 弾が消滅するまでの生存時間(秒)
	static constexpr float kBulletLifeTime = 4.0f;
	// 弾の色(敵のインクのピンク)
	static constexpr Vector4 kBulletColor = {1.0f, 0.25f, 0.55f, 1.0f};
	// 弾数・弾の速さとして設定できる下限(0以下にすると撃てない・届くまでの時間を求められないため)
	static constexpr int kMinShotCount = 1;
	static constexpr float kMinBulletSpeed = 0.1f;
	// 弾を狙う高さを、プレイヤーの足元より上へずらす量(足元ではなく体の真ん中を狙う)
	static constexpr float kPlayerAimHeight = 0.3f;
	// プレイヤーの速さとして扱う上限(1秒あたりの移動量。やり直しなどで位置が飛んだフレームは、これを超えるため速さ0として扱う)
	static constexpr float kMaxTrackedPlayerSpeed = 40.0f;
};
