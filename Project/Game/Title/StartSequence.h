#pragma once

#include "MyMath.h"
#include "Title/TitleCameraDirector.h"
#include "Title/GateCameraDirector.h"
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
/// 出発の演出(スーパージャンプなど)を使うときは、ゲートの先の出発台で BeginDeparture() が呼ばれ、演出がレールへ乗せ終えたら NotifyBoarded() が呼ばれる。
/// ゲートのカウントダウンに合わせて飛び乗る演出では、ゲートを開け始めた瞬間に BeginDeparture() が呼ばれる。
///
/// 各段階(タイトル → 回り込み → 準備 → ゲートを開ける → 乗り込み待ち → 出発の演出 → プレイ中)は、State を継承した状態クラス(StartSequenceState.h)に分けてある。
/// このクラスは今の段階を1つだけ持ち、更新・描画・問い合わせへの答えを今の段階に任せる。次の段階へ進むかは段階自身が判定する。
/// </summary>
class StartSequence{
public:
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
	/// <param name="skipTitle">trueならタイトル・カメラの回り込み・出発ゲートを飛ばして、すぐプレイ可能にする(デバッグ用。ゲートは閉じたままで、プレイヤーは GameScene 側でレールの始点に乗せる)</param>
	void Start(bool skipTitle = false);

	// タイトル・カメラの回り込みを飛ばして、スタート台の上を動ける段階(出発ゲートは閉じている)から始める
	// ゴールの的を壊し、大ジャンプでスタート台へ戻ってきたときに使う
	void StartFromStandby();

	// 演出を止めて何も行わない状態に戻す(Editモードに戻ったときに呼ぶ)
	void Stop();

	/// <summary>
	/// 更新処理(入力の受付・段階の切り替え・表示の更新)
	/// </summary>
	/// <param name="deltaTime">経過時間(秒)</param>
	/// <param name="playerPosition">プレイヤーの座標(出発ゲートの前にいるかの判定に使う)</param>
	void Update(float deltaTime,const Vector3& playerPosition);

	// プレイヤーがレールに乗り込んだことを伝える(Boarding・Departure 中なら「GO!」を出してプレイを始める)
	void NotifyBoarded();

	// 出発の演出を始めたことを伝える(Boarding・GateOpen 中なら Departure へ進め、プレイヤーを動けなくする)
	void BeginDeparture();

	// このフレームで、ゲートの前の目印で SPACE を押して出発ゲートを開け始めたか(開けたゲートは StartPlatform::GetSelectedGateIndex() で分かる)
	bool IsGateOpenTriggered() const{ return isGateOpenTriggered_; }

	// 出発ゲートを開けている最中か
	bool IsOpeningGate() const;

	// 出発の演出でレールへ向かっている最中か(撃てないため照準を出さないのに使う)
	bool IsDeparting() const;

	/// <summary>
	/// タイトルロゴをプレイヤーの位置・向きに合わせて配置する(IsControllingCamera() が true の間に使う)
	/// </summary>
	/// <param name="pivot">基準にするプレイヤーの座標</param>
	/// <param name="gameplayRotate">プレイ用カメラの向き</param>
	void UpdateLogo(const Vector3& pivot,const Vector3& gameplayRotate);

	// 3D描画処理(タイトルロゴのモデル)
	void Draw3D();

	// 2D描画処理(PRESS SPACE・操作案内・GO! のスプライト)
	void Draw2D();

	// レール進行・操作・射撃などのゲーム処理を動かしてよいか
	bool IsPlayable() const;

	// カメラを演出側で動かしている最中か
	bool IsControllingCamera() const;

	// スタート台の上を自由に動ける段階か(出発ゲートを開けている間は動けない)
	bool IsPlayerMovable() const;

	// 出発ゲートが開き、レールへ乗り込むのを待っている段階か
	bool IsBoarding() const;

	// プレイが始まる前の演出中か(タイトルからレールに乗り込むまで。照準を出さないのに使う)
	bool IsIntroRunning() const;

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

	// 出発ゲートを開ける様子を映すカメラで、プレイ用カメラを上書きしている最中か
	bool IsGateCameraActive() const{ return gateCameraDirector_.IsActive(); }

	/// <summary>
	/// 出発ゲートを開ける様子を映すカメラの座標・向きを求める(IsGateCameraActive() が true の間に使う)
	/// ゲートを映す画の位置は、調整項目(Title グループの gateCameraPosition / gateCameraTarget)で変えられる
	/// </summary>
	/// <param name="gameplayCamera">プレイ用カメラの位置と向き</param>
	/// <returns>演出中のカメラの位置と向き</returns>
	CameraPose CalculateGateCamera(const CameraPose& gameplayCamera) const;

	// 効果音の音量(0〜1。調整項目の Title グループの seVolume。出発の演出の効果音でも同じ音量を使う)
	static float GetSeVolume();

private:
	// 段階(状態)の基底クラスと、段階ごとの状態クラス(定義は StartSequenceState.h)
	// このクラスの中で宣言し、各段階からこのクラスの演出・表示を直接動かせるようにする
	class State;
	class IdleState;
	class TitleState;
	class CameraTurnState;
	class StandbyState;
	class GateOpenState;
	class BoardingState;
	class DepartureState;
	class PlayingState;

	// Start()・StartFromStandby()で共通の、始める前の状態に戻す処理(演出の経過・表示を戻し、出発ゲートを閉じて台のインクを消す)
	void ResetForStart();

	/// <summary>
	/// 段階を切り替え、新しい段階の始まりの処理(State::Enter)を呼ぶ
	/// </summary>
	/// <param name="nextState">切り替え先の段階(nullptr のときは切り替えない)</param>
	void ChangeState(std::unique_ptr<State> nextState);

	// 「GO!」を出し、合わせて効果音を鳴らす(レールに乗り込んでプレイを始めるとき用)
	void BeginPlayWithGo();

	Input* input_ = nullptr;

	// 現在の段階
	std::unique_ptr<State> state_;

	// タイトル中・回り込み中のカメラワーク
	TitleCameraDirector cameraDirector_;
	// 出発ゲートを開けるときのカメラワーク
	GateCameraDirector gateCameraDirector_;
	// タイトルロゴ(3Dモデル)
	std::unique_ptr<TitleLogo> titleLogo_;
	// タイトル表示(PRESS SPACE)
	std::unique_ptr<TitleUI> titleUI_;
	// 出発までの表示(操作案内・GO!)
	std::unique_ptr<StageIntroUI> introUI_;
	// 出発ゲートを開け閉めするスタート台(GameScene が持つ)
	StartPlatform* startPlatform_ = nullptr;

	// プレイヤーが跳ねる周期を数えるための経過時間(秒)
	float playerHopTimer_ = 0.0f;
	// このフレームで出発ゲートを開け始めたか
	bool isGateOpenTriggered_ = false;
	// 前のフレームのカウントダウンの数(数が変わった瞬間に合図の音を鳴らすのに使う)
	int lastCountdownNumber_ = 0;

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
	// ゲートを映す画のカメラ座標と注目する座標の初期値
	// (ゲートの中央の根元を原点にした、台の向きでの座標。x: 右、y: 上、z: 前(レールの方向))
	// プレイヤーの右斜め後ろの低めの位置から、床を走るインク・柱・遮断棒がすべて入るようにゲートを見上げる
	static constexpr Vector3 kDefaultGateCameraPosition = {2.2f, 1.0f, -2.8f};
	static constexpr Vector3 kDefaultGateCameraTarget = {-0.2f, 0.75f, 0.0f};
};
