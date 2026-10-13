#pragma once

#include "MyMath.h"
#include "Title/TitleCameraDirector.h"
#include <memory>

// 前方宣言
class Input;
class Obj3dCommon;
class SpriteCommon;
class TitleLogo;
class TitleUI;
class StageIntroUI;
class StartPlatform;

/// <summary>
/// ステージ開始までの流れ(タイトル → カメラの回り込み → スタート台で準備 → 出発ゲートを開ける → レールへ乗り込む → プレイ)を管理する
/// GameScene からは Update() を呼び、IsPlayable() でプレイ可能か、IsPlayerMovable() で台の上を動けるかを受け取る。
/// 準備中はスタート台の上を自由に動け、ゲートの前の目印で SPACE を押すとゲートが開く。
/// 開いたゲートをくぐってレールに乗ると、GameScene から NotifyBoarded() で知らされ、「GO!」でプレイが始まる。
/// </summary>
class StartSequence{
public:
	// 開始演出の段階
	enum class Phase{
		None,       // 演出を行っていない(Editモード中)
		Title,      // タイトル表示中(スタート入力待ち)
		CameraTurn, // カメラがプレイヤーの背後へ回り込み中
		Standby,    // スタート台の上を自由に動ける(出発ゲートは閉じている。ゲートの前で SPACE を押すと開く)
		GateOpen,   // 出発ゲートを開けている最中(ステージ情報を表示する。プレイヤーは動けない)
		Boarding,   // 出発ゲートが開き、自分で歩いてレールへ乗り込むのを待っている
		Playing,    // プレイ中
	};

	StartSequence();
	~StartSequence();

	/// <summary>
	/// 初期化(タイトルロゴのモデルとタイトル表示・出発までの表示用のスプライト生成)
	/// </summary>
	/// <param name="object3dCommon">3Dオブジェクト共通設定</param>
	/// <param name="spriteCommon">スプライト共通設定</param>
	/// <param name="input">入力</param>
	/// <param name="startPlatform">出発ゲートを開け閉めするスタート台(GameScene が持つ)</param>
	void Initialize(Obj3dCommon* object3dCommon,SpriteCommon* spriteCommon,Input* input,StartPlatform* startPlatform);

	/// <summary>
	/// タイトルから始める(Playに入った瞬間に呼ぶ)
	/// </summary>
	/// <param name="skipTitle">trueならタイトル・カメラの回り込み・出発ゲートを飛ばして、すぐプレイ可能にする(デバッグ用。ゲートは開いた状態になる)</param>
	void Start(bool skipTitle = false);

	// 演出を止めて何も行わない状態に戻す(Editモードに戻ったときに呼ぶ)
	void Stop();

	/// <summary>
	/// 更新処理(入力の受付・段階の切り替え・表示の更新)
	/// </summary>
	/// <param name="deltaTime">経過時間(秒)</param>
	/// <param name="playerPosition">プレイヤーの座標(出発ゲートの前にいるかの判定に使う)</param>
	void Update(float deltaTime,const Vector3& playerPosition);

	// プレイヤーがレールに乗り込んだことを伝える(Boarding 中なら「GO!」を出してプレイを始める)
	void NotifyBoarded();

	/// <summary>
	/// タイトルロゴをプレイヤーの位置・向きに合わせて配置する(IsControllingCamera() が true の間に使う)
	/// </summary>
	/// <param name="pivot">基準にするプレイヤーの座標</param>
	/// <param name="gameplayRotate">プレイ用カメラの向き</param>
	void UpdateLogo(const Vector3& pivot,const Vector3& gameplayRotate);

	// 3D描画処理(タイトルロゴのモデル)
	void Draw3D();

	// 2D描画処理(PRESS SPACE・操作案内・ステージ情報・GO! のスプライト)
	void Draw2D();

	// レール進行・操作・射撃などのゲーム処理を動かしてよいか
	bool IsPlayable() const{ return phase_ == Phase::Playing; }

	// カメラを演出側で動かしている最中か
	bool IsControllingCamera() const{ return phase_ == Phase::Title || phase_ == Phase::CameraTurn; }

	// スタート台の上を自由に動ける段階か(出発ゲートを開けている間は動けない)
	bool IsPlayerMovable() const{ return phase_ == Phase::Standby || phase_ == Phase::Boarding; }

	// 出発ゲートが開き、レールへ乗り込むのを待っている段階か
	bool IsBoarding() const{ return phase_ == Phase::Boarding; }

	// プレイが始まる前の演出中か(タイトルからレールに乗り込むまで。照準を出さないのに使う)
	bool IsIntroRunning() const{ return phase_ != Phase::None && phase_ != Phase::Playing; }

	// タイトル中にプレイヤーをその場で跳ねさせる高さ(描画位置にだけ足す。跳ねていないときは0)
	float GetPlayerHopHeight() const;

	/// <summary>
	/// 演出中のカメラ座標・向きを求める(IsControllingCamera() が true の間に使う)
	/// </summary>
	/// <param name="pivot">回転の中心(プレイヤーの座標)</param>
	/// <param name="gameplayPosition">プレイ用カメラの座標</param>
	/// <param name="gameplayRotate">プレイ用カメラの向き</param>
	/// <param name="outPosition">演出中のカメラ座標</param>
	/// <param name="outRotate">演出中のカメラの向き</param>
	void CalculateCamera(const Vector3& pivot,const Vector3& gameplayPosition,const Vector3& gameplayRotate,Vector3& outPosition,Vector3& outRotate) const;

private:
	Input* input_ = nullptr;

	// 現在の段階
	Phase phase_ = Phase::None;

	// タイトル中・回り込み中のカメラワーク
	TitleCameraDirector cameraDirector_;
	// タイトルロゴ(3Dモデル)
	std::unique_ptr<TitleLogo> titleLogo_;
	// タイトル表示(PRESS SPACE)
	std::unique_ptr<TitleUI> titleUI_;
	// 出発までの表示(操作案内・ステージ情報・GO!・目標)
	std::unique_ptr<StageIntroUI> introUI_;
	// 出発ゲートを開け閉めするスタート台(GameScene が持つ)
	StartPlatform* startPlatform_ = nullptr;

	// プレイヤーが跳ねる周期を数えるための経過時間(秒)
	float playerHopTimer_ = 0.0f;

	// タイトル表示の濃さ(表示中)
	static constexpr float kTitleVisible = 1.0f;
	// プレイヤーが跳ねる間隔(秒)。この周期の終わりに1回跳ねる
	static constexpr float kPlayerHopInterval = 3.0f;
	// 1回の跳ねで浮いている時間(秒)
	static constexpr float kPlayerHopDuration = 0.4f;
	// 跳ねたときの最高の高さ
	static constexpr float kPlayerHopHeight = 0.3f;
	// 円周率(跳ねの高さをsinの半周期で表すために使う)
	static constexpr float kPi = 3.14159265f;
	// タイトルBGMの音量の初期値(0〜1)
	static constexpr float kDefaultBgmVolume = 0.5f;
	// 決定音の音量の初期値(0〜1)
	static constexpr float kDefaultSeVolume = 1.0f;
};
