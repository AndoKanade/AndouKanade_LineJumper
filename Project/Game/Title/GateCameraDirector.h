#pragma once

#include "MyMath.h"

/// <summary>
/// 出発ゲートを開けるときのカメラワーク
/// ゲートを開け始めたら、プレイ用カメラからゲートの斜め前の画へ寄せ、インクが柱を塗り上がる様子と遮断棒が跳ね上がる瞬間を大きく見せる。
/// ゲートが開ききったら少しだけその画で止めてから、プレイ用カメラへ戻す。
/// プレイ用カメラとゲートを映す画を割合で混ぜて求めるため、戻りきったときにプレイ用カメラと完全に一致する。
/// </summary>
class GateCameraDirector{
public:
	// 何もしていない状態に戻す
	void Reset();

	// ゲートを映す画へ寄せ始める
	void BeginShot();

	// プレイ用カメラへ戻し始める(少し止めてから戻す)
	void BeginReturn();

	// 寄せる・戻す動きを進める
	void Update(float deltaTime);

	// プレイ用カメラを上書きしている最中か(寄せ始めてから戻りきるまで)
	bool IsActive() const{ return state_ != State::None; }

	/// <summary>
	/// 演出中のカメラ座標・向きを求める
	/// </summary>
	/// <param name="shotPosition">ゲートを映す画のカメラ座標</param>
	/// <param name="shotTarget">ゲートを映す画で注目する座標</param>
	/// <param name="gameplayPosition">プレイ用カメラの座標</param>
	/// <param name="gameplayRotate">プレイ用カメラの向き</param>
	/// <param name="outPosition">演出中のカメラ座標</param>
	/// <param name="outRotate">演出中のカメラの向き</param>
	void Calculate(const Vector3& shotPosition,const Vector3& shotTarget,const Vector3& gameplayPosition,const Vector3& gameplayRotate,Vector3& outPosition,Vector3& outRotate) const;

private:
	// カメラの動きの段階
	enum class State{
		None,   // 演出していない(プレイ用カメラのまま)
		Shot,   // ゲートを映す画へ寄せている・寄せきって映している
		Return, // プレイ用カメラへ戻している
	};

	// ゲートを映す画を混ぜる割合(0: プレイ用カメラ、1: ゲートを映す画)
	float GetShotWeight() const;

	// 寄せきるまでの時間(秒)
	static constexpr float kShotInDuration = 0.5f;
	// ゲートが開ききってから戻り始めるまで、その画で止めておく時間(秒)
	static constexpr float kReturnHoldDuration = 0.3f;
	// 戻りきるまでの時間(秒)
	static constexpr float kReturnDuration = 0.6f;
	// 1周分の角度(ラジアン)
	static constexpr float kFullTurn = 6.28318530718f;

	State state_ = State::None;
	// 今の段階に入ってからの経過時間(秒。戻すときは止めておく時間の分だけマイナスから始める)
	float timer_ = 0.0f;
};
