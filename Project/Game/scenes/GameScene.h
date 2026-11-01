#pragma once

#include "systems/BaseScene.h"
#include "systems/SceneTransition.h"
#include "MyMath.h"
#include <memory>
#include <vector>

// 前方宣言
class Camera;
class Input;
class Obj3dCommon;
class SpriteCommon;
class Skybox;
class SkyboxCommon;
class RailEditor;
class StartSequence;
class Ground;
class Sun;
class StartPlatform;
class Player;
class RailCamera;
class ScreenImpact;
class DebugTopCamera;
class PlayerBulletManager;
class TargetManager;
class GoalTarget;
class EnemyManager;
class Reticle;
class InkEffectManager;
class ComboCounter;
class StageHUD;
class StageClearUI;
class SpeedLines;
class DamageInkEffect;
class HelpOverlay;
class DepartureDirector;
class ReturnJump;
struct DepartureContext;

/// <summary>
/// ゲームシーン
/// ステージ開始演出(タイトル)からプレイ、クリア・ゲームオーバーへの遷移までを管理する。
/// プレイヤー・カメラ・弾・的・敵などの処理はそれぞれのクラスに任せ、このクラスは呼び出す順番と、
/// Edit / Play モードの切り替え、勝敗判定、デバッグ表示を担当する。
/// </summary>
class GameScene : public BaseScene{
public:
	GameScene();
	~GameScene() override;

	// シーンの初期化
	void Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon) override;
	// シーンの終了処理
	void Finalize() override;
	// シーンの更新処理
	void Update() override;
	// シーンの描画処理
	void Draw() override;

private:
	// ゲームを初期状態(レール先頭)から始め直す
	// Playに入った瞬間と、レールから落ちたときのリスタートで共通して使う
	void ResetPlayState();
	// タイトル・スタート演出を始め、デバッグ用の設定に合わせてレールへ乗せる(Playに入った瞬間とRキーのやり直しで共通して使う)
	void StartPlaySequence();
	// ゲームオーバー画面でコンティニューを選んだときに、タイトル・スタート台を飛ばして最後に通ったチェックポイントから始める
	void StartFromContinuePoint();
	// Playに入ったときに覚えておいた制御点の位置からレールに乗せる(途中からPlayするとき用)
	void BoardAtPlayStartPoint();
	// レールを進んでいる間に、チェックポイントの制御点を通ったら、その位置と壊した的を覚えておく
	void UpdateCheckpoint();
	// 最後に通ったチェックポイントの位置からレールに乗せ、壊した的をチェックポイントを通った時点の状態に戻す
	void RespawnAtCheckpoint();
	// 暗転で画面を覆っている間に、最後に通ったチェックポイント(通っていなければスタート台)からやり直す
	void RestartAfterMiss();
	// 落下・体力切れで残機を1つ減らし、残っていればやり直し、尽きたらやられ演出を始める
	void LoseLife();
	// 残機が尽きたときのやられ演出を始める(自機がピンクのインクになって弾け、周りがスローになる)
	void StartDefeat();
	// やられ演出を進め、終わったらゲームオーバー画面へ移る
	void UpdateDefeat();
	/// <summary>
	/// 鍵の的が残っていて出ていないレールを求め、レールの描き方と着地できるレールに反映する
	/// </summary>
	/// <param name="isInstant">true なら管を伸ばす演出を飛ばしてすぐに反映する(リスタート・チェックポイントからのやり直し用)</param>
	void ApplyRailLocks(bool isInstant);
	// 遠くの物を空の色に溶け込ませる距離を、全オブジェクト共通の設定にする(Play中だけ溶け込ませ、Edit中は溶け込ませない)
	void UpdateSceneDistanceFade();

	// 出発の演出が動かす・参照するもの(GameScene が持つもの)をまとめる
	DepartureContext MakeDepartureContext() const;

	/// <summary>
	/// レールに乗り込んで「GO!」でプレイを始める瞬間の演出(足元の輪・インクの跡・しぶき)
	/// 歩いて乗り込んだときと、出発の演出がレールへ乗せ終えたときで共通して使う
	/// </summary>
	/// <param name="boardedPos">乗り込んだ位置</param>
	void PlayBoardedEffects(const Vector3& boardedPos);

	/// <summary>
	/// ゴールの的を壊して大ジャンプでスタート台へ戻ってきたときに、ステージを始め直し、着地の演出を出す
	/// 的・敵・レールを初期状態に戻し、タイトルを飛ばしてスタート台の上を動ける段階から始める
	/// </summary>
	/// <param name="landedPos">着地した位置</param>
	void RestartFromReturnJump(const Vector3& landedPos);

	/// <summary>
	/// 大ジャンプでスタート台に着地した瞬間の演出(足元の輪・インクの跡・しぶき、画面の揺れ、放射ブラー)
	/// </summary>
	/// <param name="landedPos">着地した位置</param>
	void PlayReturnLandEffects(const Vector3& landedPos);

	/// <summary>
	/// 大ジャンプでスタート台に着地したときに、着地の演出を出してクリア画面へ移るのを待ち始める
	/// </summary>
	/// <param name="landedPos">着地した位置</param>
	void StartStageClear(const Vector3& landedPos);
	// 着地の演出を見せ終えたら、結果を書き込んでクリア画面へ移る
	void UpdateStageClear();

	// マウスカーソルの表示/非表示を切り替える
	void UpdateCursorVisibility(bool isPlayMode);

	// プレイ中のゲーム処理(レール進行・カメラ・プレイヤー・敵・弾・的・勝敗判定)
	void UpdateGameplay();

#ifdef USE_IMGUI
	// レール・プレイヤー・敵の状態確認用のデバッグウィンドウを表示する(Playモード中も含めて常に表示する)
	void ShowStatusWindow();
	// Editモード中の編集用パネル(エディターの選択パネル・デバッグ用のメインウィンドウ・ポストプロセス)を表示する
	void ShowEditorPanels();
	// 操作するエディターの選択パネル(Undo/Redo・まとめて保存・未保存の一覧も含む)を表示する
	void ShowEditorSelector();
	// デバッグ用のメインウィンドウの中身(開始設定・演出の切り替えとプレビュー・カメラ・ライティング)を表示する
	void ShowDebugPanelContents(Camera* activeCamera);
#endif

	// 外部から受け取るポインタ
	Obj3dCommon* object3dCommon_ = nullptr;
	Input* input_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;

	// スカイボックス
	std::unique_ptr<SkyboxCommon> skyboxCommon_;
	std::unique_ptr<Skybox> skybox_;

	// レールエディター
	std::unique_ptr<RailEditor> railEditor_;

	// 簡易的な地面
	std::unique_ptr<Ground> ground_;

	// 最初のレールの手前に置くスタート台
	std::unique_ptr<StartPlatform> startPlatform_;

	// 太陽(見た目は持たない光源)
	std::unique_ptr<Sun> sun_;

	// プレイヤー
	std::unique_ptr<Player> player_;

	// レール追従カメラ(プレイ用カメラ)
	std::unique_ptr<RailCamera> railCamera_;
	// 衝撃の瞬間の画面演出(ヒットストップと画面の揺れ)
	std::unique_ptr<ScreenImpact> screenImpact_;

	// 俯瞰デバッグカメラ
	std::unique_ptr<DebugTopCamera> debugTopCamera_;

	// プレイヤーの弾
	std::unique_ptr<PlayerBulletManager> bulletManager_;

	// 的
	std::unique_ptr<TargetManager> targetManager_;

	// ゴールの的(ゴールのレールに乗ると現れて先を並走し、壊すとステージクリア)
	std::unique_ptr<GoalTarget> goalTarget_;

	// 雑魚敵
	std::unique_ptr<EnemyManager> enemyManager_;

	// ステージ開始演出(タイトル → カメラの回り込み → スタート台で準備 → 出発ゲートを開ける → レールへ乗り込む → プレイ)
	std::unique_ptr<StartSequence> startSequence_;
	// 出発の演出(スタート台からステージのレールへ移るときの演出。ImGui で切り替える)
	std::unique_ptr<DepartureDirector> departureDirector_;
	// ゴールの的を壊したあと、大ジャンプでスタート台へ戻る演出
	std::unique_ptr<ReturnJump> returnJump_;

	// 画面中央固定のレティクル(照準)
	std::unique_ptr<Reticle> reticle_;

	// インクのしぶき・的の破片・撃破時の閃光の演出
	std::unique_ptr<InkEffectManager> inkEffect_;

	// 的を続けて壊したときの連鎖数
	std::unique_ptr<ComboCounter> comboCounter_;

	// プレイ中の画面表示(残機・壊した的の数・連鎖数)
	std::unique_ptr<StageHUD> stageHUD_;

	// ゴールの的を壊したときに出す「STAGE CLEAR!」の表示
	std::unique_ptr<StageClearUI> stageClearUI_;

	// レールを進んでいる間に出すスピード線
	std::unique_ptr<SpeedLines> speedLines_;

	// プレイヤーの体力を、画面の縁から迫る敵のインク(ポストプロセス)で表現する
	std::unique_ptr<DamageInkEffect> damageInkEffect_;

	// H キーで開く操作説明(操作方法と画面の流れ)
	std::unique_ptr<HelpOverlay> helpOverlay_;

	// 前フレームがPlayモードだったか(Playに入った瞬間を検出してリセットするのに使う)
	bool wasPlayMode_ = false;

	// マウスカーソルの表示状態(Playモード中にTABキーで切り替え。Editモードでは常に表示する)
	bool isCursorVisible_ = true;
	// マウスカーソルをウィンドウ内に閉じ込めているか(カーソルを隠している間だけ閉じ込め、ウィンドウ外の誤クリックを防ぐ)
	bool isCursorClipped_ = false;

	// Playに入ったときにタイトル・スタート演出を飛ばし、レールの上からすぐプレイを始めるか(デバッグ用。ImGuiのチェックボックスで切り替える)
	bool skipTitle_ = false;
	// Playに入ったときに、Rail Editorで選んでいた制御点からプレイを始めるか(デバッグ用。ImGuiのチェックボックスで切り替える)
	bool startsFromSelectedPoint_ = false;
	// Playに入ったときにRail Editorで編集していたレールと制御点(途中から始める位置と、Editへ戻ったときに選択を戻すのに使う)
	int editRailIndex_ = 0;
	int editPointIndex_ = -1;
	// 今回のPlayを選んだ制御点から始めているか(落下リスタートでも同じ位置から始め直すのに使う)
	bool isPlayingFromPoint_ = false;
	// 次の弾を撃てるようになるまでの残り時間(秒。押しっぱなしの連射の間隔に使う)
	float fireCooldown_ = 0.0f;
	// 残機(落下・体力切れで1つ減り、0になったらゲームオーバー。やり直しの ResetPlayState() では戻さない)
	int lives_ = kMaxLives;
	// 残機が尽きて、やられ演出の最中か(自機を消し、操作・判定を止め、周りをスローにする)
	bool isDefeated_ = false;
	// やられ演出が始まってからの経過時間(秒)
	float defeatTimer_ = 0.0f;
	// 大ジャンプでスタート台に着地し、クリア画面へ移るのを待っている最中か(操作・判定を止める)
	bool isStageCleared_ = false;
	// スタート台に着地してからの経過時間(秒)
	float stageClearTimer_ = 0.0f;

	// 最後に通ったチェックポイント(レールから落ちたら、ここからやり直す)
	struct Checkpoint{
		int railIndex = -1;                  // チェックポイントの制御点があるレール(-1: まだ通っていない)
		int pointIndex = -1;                 // チェックポイントの制御点
		std::vector<bool> targetAliveStates; // 通った時点での的ごとの生存状態
	};
	Checkpoint checkpoint_;

	// レールごとに、鍵の的が残っていて出ていないか(毎フレーム求め直す。配列を作り直さないようメンバーに持つ)
	std::vector<bool> lockedRails_;

	// 1フレームの経過時間(エンジンが固定60fps前提(TimeManagerで60fps固定)なので、そのまま合わせる)
	static constexpr float kDeltaTime = 1.0f / 60.0f;
	// Playを始めたとき・コンティニューしたときの残機
	static constexpr int kMaxLives = 3;
	// 復活したときに、透明な体が元に戻りきるまでの時間(秒)
	static constexpr float kRespawnRevealDuration = 0.8f;
	// やられ演出で、自機が弾けてからゲームオーバー画面への切り替えを始めるまでの時間(秒)
	static constexpr float kDefeatDuration = 1.4f;
	// やられ演出の間の、周り(敵・インクの演出)の時間の進み方(通常の何倍か)
	static constexpr float kDefeatTimeScale = 0.35f;
	// 自機が弾けた瞬間の衝撃(動きを止める時間、画面の揺れの幅・収まるまでの時間)
	static constexpr float kDefeatHitStopDuration = 0.15f;
	static constexpr float kDefeatShakeAmplitude = 0.25f;
	static constexpr float kDefeatShakeDuration = 0.6f;
	// 自機が弾けた瞬間に、自機を中心にかける放射ブラーの強さと消えるまでの時間
	static constexpr float kDefeatRadialBlurWidth = 0.03f;
	static constexpr float kDefeatRadialBlurDuration = 0.5f;
	// ゴールの的を壊した瞬間の衝撃(動きを止める時間、画面の揺れの幅・収まるまでの時間)
	// ステージクリアの瞬間なので、出発ゲートが開く瞬間・やられ演出より強くする
	static constexpr float kGoalBreakHitStopDuration = 0.2f;
	static constexpr float kGoalBreakShakeAmplitude = 0.35f;
	static constexpr float kGoalBreakShakeDuration = 0.7f;
	// ゴールの的を壊した瞬間に、的を中心にかける放射ブラーの強さと消えるまでの時間
	static constexpr float kGoalBreakRadialBlurWidth = 0.04f;
	static constexpr float kGoalBreakRadialBlurDuration = 0.6f;
	// ゴールの的を壊してから大ジャンプで跳ぶまでの間の、破片・しぶきの時間の進み方(通常の何倍か)
	static constexpr float kGoalBreakTimeScale = 0.3f;
	// 射撃に使うマウスボタン(左ボタン)
	static constexpr int kShootMouseButton = 0;
	// 押しっぱなしで連射するときの弾の発射間隔(秒)
	static constexpr float kFireInterval = 0.1f;
	// 残り時間がこれ以下なら撃てるとみなす(小数の誤差で残り時間がわずかに残り、発射が1フレーム遅れないよう半フレーム分の余裕を持たせる)
	static constexpr float kFireIntervalTolerance = kDeltaTime * 0.5f;
	// 弾を撃ったときに手元から飛び散らせるインクのしぶきの数
	static constexpr int kMuzzleSplashCount = 3;
	// レールに乗り込んで出発した瞬間に、足元から飛び散らせるインクのしぶきの数
	static constexpr int kLaunchSplashCount = 10;
	// レールに乗り込んで出発した瞬間に、足元に残すインクの跡の半径と、高く飛び散らせて台に落とすしぶきの数
	static constexpr float kLaunchInkRadius = 1.1f;
	static constexpr int kLaunchBurstCount = 24;
	// 弾がスタート台に当たったときに跳ねさせるインクのしぶきの数
	static constexpr int kBulletLandSplashCount = 4;
	// 出発ゲートの遮断棒が跳ね上がった瞬間に、ゲートから飛び散らせるインクのしぶきの数
	static constexpr int kGateBurstCount = 20;
	// 遮断棒が跳ね上がった瞬間の衝撃(ゲートとインクの動きを止める時間、画面の揺れの幅・収まるまでの時間)
	static constexpr float kGateHitStopDuration = 0.1f;
	static constexpr float kGateShakeAmplitude = 0.12f;
	static constexpr float kGateShakeDuration = 0.4f;
	// 遮断棒が跳ね上がった瞬間に、ゲートを中心にかける放射ブラーの強さと消えるまでの時間
	static constexpr float kGateRadialBlurWidth = 0.02f;
	static constexpr float kGateRadialBlurDuration = 0.35f;
	// 大ジャンプでスタート台に着地した瞬間の衝撃(動きを止める時間、画面の揺れの幅・収まるまでの時間)
	static constexpr float kReturnLandHitStopDuration = 0.1f;
	static constexpr float kReturnLandShakeAmplitude = 0.25f;
	static constexpr float kReturnLandShakeDuration = 0.5f;
	// 大ジャンプでスタート台に着地した瞬間に、着地点を中心にかける放射ブラーの強さと消えるまでの時間
	static constexpr float kReturnLandRadialBlurWidth = 0.025f;
	static constexpr float kReturnLandRadialBlurDuration = 0.4f;
	// 大ジャンプでスタート台に着地してから、クリア画面への切り替えを始めるまでの時間(秒)
	// 着地の揺れ(kReturnLandShakeDuration)が収まり、インクのしぶきが落ちきるのを見せてから移る
	static constexpr float kStageClearDuration = 1.0f;
};
