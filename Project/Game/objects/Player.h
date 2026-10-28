#pragma once

#include "MyMath.h"
#include "Model.h"
#include "RailEditor.h"
#include <memory>
#include <string>

// 前方宣言
class Input;
class Obj3D;
class Obj3dCommon;
class StartPlatform;

/// <summary>
/// プレイヤー
/// スタート台の上から始まり、台の上をWASDで自由に動いて、台と重なったレールの始点へ歩いて乗る(出発ゲートが閉じている間はゲートより先へ進めない)。
/// レールに沿って自動で進み、レール上ではWASDで自由に動けない(射撃に専念させる)。
/// レール上でジャンプするとレールに沿って進みながらその場で跳ね、A/Dを押しながらジャンプすると並走しているレールへ飛び移る。
/// スタート台から落下中にレールの真上でレールの高さを跨ぐと、そのレールに乗る。
/// 体力を持ち、敵弾が当たると減る(被弾後は一定時間無敵)。
/// </summary>
class Player{
public:
	Player();
	~Player();

	/// <summary>
	/// 初期化処理(モデルの生成と、調整項目のGlobalVariablesへの登録)
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	/// <param name="paramGroup">調整項目を登録するGlobalVariablesのグループ名</param>
	void Initialize(Obj3dCommon* objCommon,const std::string& paramGroup);

	// レール先頭・オンレール・体力満タンの初期状態に戻す
	void Reset();

	/// <summary>
	/// スタート台の上に立たせ、オフレールの状態から始める(Reset()の後に呼ぶ)
	/// </summary>
	/// <param name="startPosition">立たせる位置(台の上面)</param>
	/// <param name="startRotation">カメラ・プレイヤーの基準向き(レールの始点の向きにそろえる)</param>
	void StartOnPlatform(const Vector3& startPosition,const Vector3& startRotation);

	// レールの進行度startTの位置にオンレールの状態で乗せる(出発の演出でレールへ乗せるときに使う。体力はそのまま)
	// 始点以外を指定すると、Editモードで選んだ制御点から途中のプレイを始められる
	void BoardRailStart(float startT = 0.0f);

	/// <summary>
	/// 出発の演出中に、プレイヤーの表示を演出側で決めた位置・向き・大きさにする(UpdateTransform() の後に呼んで上書きする)
	/// </summary>
	/// <param name="position">表示する位置(足元)</param>
	/// <param name="rotate">モデルの向き</param>
	/// <param name="scaleRate">通常の大きさに掛ける倍率(軸ごと)</param>
	void SetDirectedPose(const Vector3& position,const Vector3& rotate,const Vector3& scaleRate);

	/// <summary>
	/// レールを進む速さを一時的に上げ、時間とともに元の速さへ戻す(発射台から飛び出した直後に使う)
	/// </summary>
	/// <param name="rate">かけ始めの速さの倍率</param>
	/// <param name="duration">元の速さへ戻りきるまでの時間(秒)</param>
	void StartRailBoost(float rate,float duration);

	/// <summary>
	/// レールの進行(プレイ可能かつオンレール中、かつ終端未到達のときだけ進める)
	/// </summary>
	/// <param name="railEditor">進行するレール</param>
	/// <param name="isGameplayActive">プレイ可能かどうか</param>
	/// <param name="deltaTime">経過時間(秒)</param>
	void UpdateRailProgress(const RailEditor* railEditor,bool isGameplayActive,float deltaTime);

	// オンレール/オフレールの状態に応じて、カメラ・プレイヤーの基準位置と基準向きを求める
	void UpdateBasePose(const RailEditor* railEditor);

	/// <summary>
	/// 表示位置の計算とモデルの行列更新
	/// </summary>
	/// <param name="cameraRotation">カメラの向き(体を照準の方向へ向けるために使う)</param>
	/// <param name="hopHeight">タイトル中の跳ねの高さ(見た目だけに反映する)</param>
	void UpdateTransform(const Vector3& cameraRotation,float hopHeight);

	// 被弾後の無敵時間を進める
	void UpdateInvincible(float deltaTime);

	/// <summary>
	/// 透明な体がだんだん元に戻る、現れる演出を始める(復活したとき用。Reset() で止まる)
	/// </summary>
	/// <param name="style">見せ方(横じまが太くなって埋まる / 光る境界線が下から上へ通る)</param>
	/// <param name="duration">元に戻りきるまでの時間(秒)</param>
	void StartReveal(Model::RevealStyle style,float duration);

	// 現れる演出を進める
	void UpdateReveal(float deltaTime);

	// 敵弾が当たったときの処理(無敵中でなければ体力を1減らし、無敵時間を始める)
	void TakeDamage();

	/// <summary>
	/// ジャンプ・WASD移動・着地判定(レール上ではWASD移動はせず、その場でのジャンプと並走レールへの乗り換えのみ)
	/// </summary>
	/// <param name="input">入力</param>
	/// <param name="railEditor">着地先を探すレール</param>
	/// <param name="startPlatform">上に立てるスタート台(出発ゲートが閉じている間はゲートより先へ進めない)</param>
	/// <param name="cameraForward">カメラの前方ベクトル(WASD移動の基準)</param>
	/// <param name="cameraRight">カメラの右方向ベクトル(WASD移動の基準)</param>
	/// <param name="groundHeight">地面の高さ(ここまで落ちたらリスタートが必要)</param>
	/// <param name="deltaTime">経過時間(秒)</param>
	/// <returns>どのレールにも乗れずに地面まで落ち、リスタートが必要になったときtrue</returns>
	bool UpdateMovement(Input* input,RailEditor* railEditor,const StartPlatform* startPlatform,const Vector3& cameraForward,const Vector3& cameraRight,float groundHeight,float deltaTime);

	// 描画処理
	void Draw();

	// 当たり判定用に、モデルから求めたワールド座標系の境界球を取得
	Model::BoundingSphere GetHitSphere() const;

	// 表示位置を取得(敵の検知・開始演出の基準に使う。タイトル中の跳ねは含まない)
	const Vector3& GetPosition() const{ return position_; }
	// カメラ・プレイヤーの共通の基準位置を取得
	const Vector3& GetBasePosition() const{ return basePosition_; }
	// カメラ・プレイヤーの共通の基準向きを取得
	const Vector3& GetBaseRotation() const{ return baseRotation_; }
	// 弾を撃ち出す位置を取得(足元から構えた手元の高さまで上げた位置)
	Vector3 GetMuzzlePosition() const{ return {position_.x, position_.y + kMuzzleHeight, position_.z}; }

	// ゴールのレールの終点に着いて止まったかどうか(オンレール中に終端まで進んだらtrue。ゴール以外のレールは終点でレールから外れるためtrueにならない)
	bool HasReachedGoal() const{ return isOnRail_ && isRailFinished_; }
	// 体力が尽きたかどうか
	bool IsDead() const{ return hp_ <= 0; }

	// デバッグ表示用の取得
	float GetRailT() const{ return railT_; }
	bool IsOnRail() const{ return isOnRail_; }
	// 足がレールに着いているかどうか(オンレール中で、跳ねている途中でも並走レールへ飛び移っている途中でもないときtrue)
	bool IsGroundedOnRail() const{ return isOnRail_ && !isRailHopping_ && !isSwitchingRail_; }
	float GetFreeVelocityY() const{ return freeVelocityY_; }
	int GetHp() const{ return hp_; }
	int GetMaxHp() const{ return kMaxHp; }
	float GetInvincibleTimer() const{ return invincibleTimer_; }

	// 足の裏の左右の幅の半分(モデルの足の裏の幅0.5の半分0.25に、表示スケール0.3を掛けた値)
	static constexpr float kFootHalfWidth = 0.075f;
	// 着地判定で使う水平方向(XZ平面)の許容距離
	// 見た目どおりに、足の裏が管に少しでも掛かっていれば乗れるよう、管の半径と足の裏の幅の半分を足した距離にする
	// レールエディターで着地できる範囲を可視化するのにも使う
	static constexpr float kOnRailHorizontalThreshold = RailEditor::kTubeRadius + kFootHalfWidth;
	// ゴールのレールのインデックス(rail5。ほかのレールは終点でレールから外れるが、このレールは終点で止まる)
	// 乗るとゴールの的が現れて先を並走し、終点で止まった後もゴールの的を壊すまで撃ち続けられる
	static constexpr int kGoalRailIndex = 5;

private:
	/// <summary>
	/// 押した方向のすぐ横を並走しているレールへ、飛び移る動きを付けて乗り換える
	/// </summary>
	/// <param name="railEditor">乗り換え先を探すレール</param>
	/// <param name="sideDirXZ">乗り換えたい方向(XZ平面の正規化ベクトル)</param>
	/// <returns>乗り換えられたときtrue</returns>
	bool TrySwitchToSideRail(RailEditor* railEditor,const Vector3& sideDirXZ);

	/// <summary>
	/// レール上でのジャンプ(その場で跳ねる)と、並走レールへ飛び移る動きを進める
	/// </summary>
	/// <param name="deltaTime">経過時間(秒)</param>
	void UpdateRailJump(float deltaTime);

	// 描画用の3Dオブジェクト(三人称視点用のプレイヤーのモデル)
	std::unique_ptr<Obj3D> obj_;

	// 調整項目を登録したGlobalVariablesのグループ名
	std::string paramGroup_;

	// レール移動の進行度(0〜1)
	float railT_ = 0.0f;
	// レール終端(railT_=1.0)に到達したかどうか(到達後はループせず停止させる)
	bool isRailFinished_ = false;
	// 終点に着いてレールから外れたレールのインデックス(-1: なし)。落ちている間はこのレールに着地させない
	int endedRailIndex_ = -1;

	// レール座標によるオンレール判定
	// レール上でのジャンプ・並走レールへの乗り換え中もオンレールのままにし、レールに沿った進行を止めない
	bool isOnRail_ = true;

	// レール上でのジャンプ用の状態
	// レール上でジャンプして跳ねている途中か(跳ねている間は次のジャンプを受け付けない)
	bool isRailHopping_ = false;
	// レール上の位置から浮き上がっている高さ
	float railHopHeight_ = 0.0f;
	// レール上で跳ねているときのY方向の速度
	float railHopVelocityY_ = 0.0f;

	// 並走レールへ飛び移る動きの状態
	// 並走レールへ飛び移っている途中か(飛び移っている間は次のジャンプを受け付けない)
	bool isSwitchingRail_ = false;
	// 飛び移り始めてからの経過時間(秒)
	float railSwitchTimer_ = 0.0f;
	// 飛び移り始めた瞬間の、乗り換え先レール上の位置から見た乗り換え前の位置のずれ(これを0まで減らして乗り換え先へ寄せる)
	Vector3 railSwitchStartOffset_ = {0.0f, 0.0f, 0.0f};

	// プレイヤーの自立(ジャンプ+WASD移動)用の状態
	// オフレール中の基準座標(オンレール中のレール上の座標に相当する、カメラ・プレイヤーの共通の基準点)
	Vector3 freePosition_ = {0.0f, 0.0f, 0.0f};
	// オフレール中のY方向の速度(ジャンプ初速・重力の適用に使用)
	float freeVelocityY_ = 0.0f;
	// オフレール中の水平方向(XZ平面)の速度(レールの終点から飛び出した勢い。着地・リスタートで0に戻す)
	Vector3 freeVelocityXZ_ = {0.0f, 0.0f, 0.0f};
	// オフレール中の基準向き(レールを離れた瞬間の向きを固定して保持する)
	Vector3 freeBaseRotation_ = {0.0f, 0.0f, 0.0f};
	// スタート台の上に立っているか(立っている間はジャンプでき、重なったレールへ歩いて乗れる)
	bool isOnPlatform_ = false;

	// カメラ・プレイヤーの共通の基準位置と基準向き(UpdateBasePose()で求める)
	Vector3 basePosition_ = {0.0f, 0.0f, 0.0f};
	Vector3 baseRotation_ = {0.0f, 0.0f, 0.0f};
	// 表示位置(UpdateTransform()で求める)
	Vector3 position_ = {0.0f, 0.0f, 0.0f};

	// 現在の体力(0になるとそれ以上減らない)	
	int hp_ = kMaxHp;
	// 被弾後の無敵時間の残り(秒)。0より大きい間は敵弾が当たっても体力が減らない
	float invincibleTimer_ = 0.0f;

	// 現れる演出の見せ方(None のときは演出をしていない)・経過時間・元に戻りきるまでの時間(秒)
	Model::RevealStyle revealStyle_ = Model::RevealStyle::None;
	float revealTimer_ = 0.0f;
	float revealDuration_ = 0.0f;

	// レールを進む速さを一時的に上げる倍率(かけ始め)と、元の速さへ戻りきるまでの時間・経過時間(秒)
	float railBoostRate_ = 1.0f;
	float railBoostDuration_ = 0.0f;
	float railBoostTimer_ = 0.0f;

	// レール全体の進行速度の初期値(1秒あたりに進むワールド座標の距離。保存済みJSONがあればそちらが優先される)
	static constexpr float kDefaultRailSpeed = 6.0f;
	// レール上で跳ねている間・並走レールへ飛び移っている間に、レールに沿って進む速さに掛ける割合(0で空中では前へ進まない)
	static constexpr float kRailAirborneSpeedRate = 0.3f;
	// モデルの表示スケール
	static constexpr float kScale = 0.3f;
	// 弾を撃ち出す高さ(足元からの高さ。モデルの身長1.66にkScaleを掛けた約0.5のうち、胸の高さにする)
	static constexpr float kMuzzleHeight = 0.35f;
	// 体力の最大値(Reset()でこの値へ戻す)
	static constexpr int kMaxHp = 3;
	// 被弾してから次に被弾できるようになるまでの無敵時間(秒)
	static constexpr float kInvincibleTime = 1.0f;
	// ジャンプ初速(上方向、1秒あたりの速度)
	static constexpr float kJumpSpeed = 6.0f;
	// 重力加速度(1秒あたりの下方向への速度変化量)
	static constexpr float kGravity = 9.8f;
	// オフレール中のWASD移動速度(1秒あたりの移動量)
	static constexpr float kMoveSpeed = 5.0f;
	// レールの終端の進行度
	static constexpr float kRailEndT = 1.0f;
	// ゴール以外のレールの終点から飛び出すときに、レールを進んでいた速さに掛ける割合(1で同じ速さのまま飛び出す)
	static constexpr float kRailEndLaunchSpeedRate = 0.4f;
	// ゴール以外のレールの終点から飛び出すときの、上向きの初速(通常のジャンプより小さくして飛びすぎないようにする)
	static constexpr float kRailEndJumpSpeed = 2.5f;
	// 並走レールへの乗り換えができる水平方向(XZ平面)の最大距離
	static constexpr float kSideRailSwitchMaxDistance = 8.0f;
	// 並走レールへの乗り換えができる高さの差の最大値
	static constexpr float kSideRailSwitchMaxHeightDiff = 3.0f;
	// 並走レールが押した方向の真横にあるとみなす、方向ベクトルとの内積の下限(1に近いほど真横に限定される)
	static constexpr float kSideRailDirectionThreshold = 0.7f;
	// 並走レールへ飛び移るのにかける時間(秒)
	static constexpr float kRailSwitchDuration = 0.4f;
	// 並走レールへ飛び移るときの山なりの最も高い位置の高さ
	static constexpr float kRailSwitchArcHeight = 1.0f;
	// 進行度0〜1の放物線 s*(1-s) の頂点(s=0.5で0.25)を1にそろえるための係数
	static constexpr float kParabolaPeakScale = 4.0f;
	// スタート台の上からレールへ歩いて乗れる、レールとの高さの差の最大値
	static constexpr float kPlatformBoardHeightRange = 0.5f;
};
