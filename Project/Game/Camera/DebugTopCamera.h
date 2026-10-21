#pragma once

#include "MyMath.h"
#include "Camera/CameraPose.h"

// 前方宣言
class Input;
class Obj3dCommon;

/// <summary>
/// レール全体を確認するためのデバッグカメラ
/// Editモード中は常に有効で、Playに入るとプレイ用カメラに切り替わる。
/// 起動時はPlayの最初と同じ視点から始め、以降はWASD(平面移動)+QE(高さ)+右ドラッグ(向き)で自由に動かせる。
/// </summary>
class DebugTopCamera{
public:
	DebugTopCamera();
	~DebugTopCamera();

	// カメラの生成と初期位置の設定
	void Initialize(Obj3dCommon* objCommon);

	// 有効な間の移動処理(WASDで向いている方向基準の平面移動、QEで高さ、右ドラッグで向きの変更)
	void UpdateMove(Input* input);

	// 俯瞰デバッグカメラが有効かどうかを取得
	bool IsEnabled() const{ return isEnabled_; }

	// 俯瞰デバッグカメラをONにして、アクティブなカメラにする(Editモードに入ったときに呼ぶ)
	void Enable();

	// 俯瞰デバッグカメラをOFFにして、プレイ用カメラに戻す(Playを押したときに呼ぶ)
	void Disable();

	// 俯瞰デバッグカメラをONにして、今の向きのままtargetPositionの手前へ寄せる(レールエディターの「Focus」用)
	void FocusOnPoint(const Vector3& targetPosition);

	// カメラの位置と向きを指定した画に合わせる(右ドラッグで変える向きもその画の向きから始める)
	void SetPose(const CameraPose& pose);

private:
	// ON/OFFの状態に合わせて、アクティブなカメラを俯瞰デバッグカメラとプレイ用カメラで切り替える
	void ApplyActiveCamera();

	// マウスカーソルがゲーム画面の上にあるか(編集パネルの上で右ボタンを押したときに回転させないための判定)
	bool IsMouseOnGameView() const;

	Obj3dCommon* objCommon_ = nullptr;

	// 俯瞰デバッグカメラON/OFF状態
	bool isEnabled_ = false;

	// 右ドラッグで変更するカメラの向き
	float pitch_ = kLookDownRotateX; // 上下(X軸回転)
	float yaw_ = 0.0f;               // 左右(Y軸回転)

	// 右ドラッグで向きを変えている最中か
	bool isRotating_ = false;
	// 前フレームで右ボタンが押されていたか(押した瞬間を検出するのに使う)
	bool wasRotateButtonDown_ = false;

	// 初期位置の高さ
	static constexpr float kInitialHeight = 30.0f;
	// 真下を向かせるためのX軸回転(90度=pi/2)
	static constexpr float kLookDownRotateX = 3.14159265f * 0.5f;
	// FocusOnPoint()で寄せたときの、対象からカメラまでの距離(制御点の目印と周りのレールが見える程度)
	static constexpr float kFocusDistance = 8.0f;
	// 1秒あたりの移動量
	static constexpr float kMoveSpeed = 10.0f;
	// 右ドラッグ時のマウス1移動量あたりの回転量(ラジアン)
	static constexpr float kRotateSensitivity = 0.003f;
	// 向きを変えるときに押すマウスボタン(右ボタン)
	static constexpr int kRotateMouseButton = 1;
	// 1フレームの経過時間(エンジンが固定60fps前提)
	static constexpr float kDeltaTime = 1.0f / 60.0f;
};
