#pragma once
#include "objects/Player.h"
#include <memory>

/// <summary>
/// プレイヤーの移動の状態の基底クラス
/// Player は今の状態を1つだけ持ち、ジャンプ・WASD移動・着地判定(UpdateMovement)を今の状態に任せる。
/// 次の状態へ移るかどうかは状態自身が判定し、移るときは次の状態を戻り値で返す(Player が返ってきた状態へ切り替える)。
///
/// レールの上の状態: RailRunState(走行中) / RailHopState(その場で跳ねている) / RailSwitchState(並走レールへ飛び移っている)
/// レールの外の状態: PlatformState(スタート台の上を歩いている) / FallState(ジャンプ・落下中)
/// </summary>
class Player::State{
public:
	virtual ~State() = default;

	/// <summary>
	/// ジャンプ・WASD移動・着地判定を1フレーム進める
	/// </summary>
	/// <param name="owner">この状態を持つプレイヤー</param>
	/// <param name="context">入力・レール・スタート台・カメラの向き・経過時間</param>
	/// <returns>次の状態へ移るときはその状態、この状態を続けるときは nullptr</returns>
	virtual std::unique_ptr<State> UpdateMovement(Player& owner,const MoveContext& context) = 0;

	// オンレール中か(レール上でのジャンプ・並走レールへの乗り換え中もオンレールのままにし、レールに沿った進行を止めない)
	virtual bool IsOnRail() const = 0;

	// 足がレールに着いているか(跳ねている途中・飛び移っている途中は false)
	virtual bool IsGroundedOnRail() const{ return false; }

	// レール上の位置から見た、表示の基準位置のずれ(跳ねている高さ・飛び移り中のずれと山なりの高さ)
	virtual Vector3 GetRailOffset() const{ return {0.0f, 0.0f, 0.0f}; }
};

/// <summary>
/// レールの上で、足がレールに着いて走っている状態
/// SPACE で跳ね(RailHopState)、A/D(ループするレールでは W/S も)を押しながらだと並走レールへ飛び移る(RailSwitchState)
/// </summary>
class Player::RailRunState : public Player::State{
public:
	std::unique_ptr<State> UpdateMovement(Player& owner,const MoveContext& context) override;
	bool IsOnRail() const override{ return true; }
	bool IsGroundedOnRail() const override{ return true; }

	/// <summary>
	/// 足がレールに着いているときのジャンプキー(SPACE)の受け付け(跳ね終わり・飛び移り終わりのフレームでも同じ受け付けをするために分けてある)
	/// </summary>
	/// <param name="owner">プレイヤー</param>
	/// <param name="context">入力・レール・カメラの向き</param>
	/// <returns>ジャンプしたときは跳ねる状態か飛び移る状態、押していないときは nullptr</returns>
	static std::unique_ptr<State> HandleJumpInput(Player& owner,const MoveContext& context);

private:
	/// <summary>
	/// 押した方向のすぐ横を並走しているレールへ、飛び移る動きを付けて乗り換える
	/// </summary>
	/// <param name="owner">プレイヤー</param>
	/// <param name="railEditor">乗り換え先を探すレール</param>
	/// <param name="sideDirXZ">乗り換えたい方向(XZ平面の正規化ベクトル)</param>
	/// <returns>乗り換えられたときは飛び移る状態、乗り換えられないときは nullptr</returns>
	static std::unique_ptr<State> TrySwitchToSideRail(Player& owner,RailEditor* railEditor,const Vector3& sideDirXZ);
};

/// <summary>
/// レールの上で、レールに沿って進みながらその場で跳ねている状態(レールの高さまで戻ったら RailRunState へ戻る)
/// </summary>
class Player::RailHopState : public Player::State{
public:
	std::unique_ptr<State> UpdateMovement(Player& owner,const MoveContext& context) override;
	bool IsOnRail() const override{ return true; }
	Vector3 GetRailOffset() const override{ return {0.0f, hopHeight_, 0.0f}; }

private:
	// レール上の位置から浮き上がっている高さ
	float hopHeight_ = 0.0f;
	// 跳ねているときのY方向の速度(跳ね始めはジャンプ初速)
	float hopVelocityY_ = kJumpSpeed;
};

/// <summary>
/// 並走レール(または終点で自動で飛び移る先のレール)へ、山なりに飛び移っている状態(決めた時間がたったら RailRunState へ戻る)
/// 乗るレールの切り替えはこの状態に入る前に済ませ、見た目の位置だけを乗り換え前の位置から乗り換え先へ寄せていく
/// </summary>
class Player::RailSwitchState : public Player::State{
public:
	/// <param name="startOffset">飛び移り始めた瞬間の、乗り換え先レール上の位置から見た乗り換え前の位置のずれ</param>
	explicit RailSwitchState(const Vector3& startOffset) : startOffset_(startOffset){}

	std::unique_ptr<State> UpdateMovement(Player& owner,const MoveContext& context) override;
	bool IsOnRail() const override{ return true; }
	Vector3 GetRailOffset() const override;

private:
	// 飛び移り始めた瞬間のずれ(これを0まで減らして乗り換え先へ寄せる)
	Vector3 startOffset_;
	// 飛び移り始めてからの経過時間(秒)
	float timer_ = 0.0f;
};

/// <summary>
/// レールの外にいる状態の共通部分(WASDでの水平移動・重力・スタート台とレールへの着地判定)
/// </summary>
class Player::OffRailState : public Player::State{
public:
	bool IsOnRail() const override{ return false; }

protected:
	// 1フレーム動いた結果、どこに立っているか
	enum class FreeMoveResult{
		Airborne,   // 空中にいる
		OnPlatform, // スタート台の上面に立っている
		OnRail,     // レールに乗った(乗るレールと進行度は設定済み)
	};

	/// <summary>
	/// WASDでの水平移動と、重力による垂直移動を行い、スタート台・レールへの着地を判定する
	/// </summary>
	/// <param name="owner">プレイヤー</param>
	/// <param name="context">入力・レール・スタート台・カメラの向き・経過時間</param>
	/// <param name="isStandingOnPlatform">スタート台の上に立って歩いているか(立っている間は台の端から落ちないよう押し戻す)</param>
	/// <returns>動いた結果、どこに立っているか</returns>
	static FreeMoveResult UpdateFreeMovement(Player& owner,const MoveContext& context,bool isStandingOnPlatform);
};

/// <summary>
/// スタート台の上に立って歩いている状態(SPACE でジャンプすると FallState、台と重なったレールの真上まで歩くと RailRunState へ移る)
/// </summary>
class Player::PlatformState : public Player::OffRailState{
public:
	std::unique_ptr<State> UpdateMovement(Player& owner,const MoveContext& context) override;
};

/// <summary>
/// レールの外でジャンプ・落下している状態(スタート台の上面に着地すると PlatformState、レールに着地すると RailRunState へ移る)
/// どこにも着地できずに地面の高さまで落ちたら、Player::UpdateMovement() がリスタートが必要であることを呼び出し側へ伝える
/// </summary>
class Player::FallState : public Player::OffRailState{
public:
	std::unique_ptr<State> UpdateMovement(Player& owner,const MoveContext& context) override;
};
