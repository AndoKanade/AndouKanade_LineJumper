#pragma once

#include "systems/BaseScene.h"
#include "systems/SceneTransition.h"
#include "MyMath.h"
#include <memory>

// 前方宣言
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
class EnemyManager;
class Reticle;
class InkEffectManager;
class ComboCounter;
class StageHUD;
class SpeedLines;
class HelpOverlay;
class DepartureDirector;
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

	// 出発の演出が動かす・参照するもの(GameScene が持つもの)をまとめる
	DepartureContext MakeDepartureContext() const;

	/// <summary>
	/// レールに乗り込んで「GO!」でプレイを始める瞬間の演出(足元の輪・インクの跡・しぶき)
	/// 歩いて乗り込んだときと、出発の演出がレールへ乗せ終えたときで共通して使う
	/// </summary>
	/// <param name="boardedPos">乗り込んだ位置</param>
	void PlayBoardedEffects(const Vector3& boardedPos);

	// マウスカーソルの表示/非表示を切り替える
	void UpdateCursorVisibility(bool isPlayMode);

	// プレイ中のゲーム処理(レール進行・カメラ・プレイヤー・敵・弾・的・勝敗判定)
	void UpdateGameplay();

#ifdef USE_IMGUI
	// レール・プレイヤー・敵の状態確認用のデバッグウィンドウを表示する(Playモード中も含めて常に表示する)
	void ShowStatusWindow();
	// Editモード中の編集用パネル(カメラ・ライティング・的のHierarchy/Inspector)を表示する
	void ShowEditorPanels();
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

	// 雑魚敵
	std::unique_ptr<EnemyManager> enemyManager_;

	// ステージ開始演出(タイトル → カメラの回り込み → スタート台で準備 → 出発ゲートを開ける → レールへ乗り込む → プレイ)
	std::unique_ptr<StartSequence> startSequence_;
	// 出発の演出(スタート台からステージのレールへ移るときの演出。ImGui で切り替える)
	std::unique_ptr<DepartureDirector> departureDirector_;

	// 画面中央固定のレティクル(照準)
	std::unique_ptr<Reticle> reticle_;

	// インクのしぶき・的の破片・撃破時の閃光の演出
	std::unique_ptr<InkEffectManager> inkEffect_;

	// 的を続けて壊したときの連鎖数
	std::unique_ptr<ComboCounter> comboCounter_;

	// プレイ中の画面表示(体力・壊した的の数・連鎖数)
	std::unique_ptr<StageHUD> stageHUD_;

	// レールを進んでいる間に出すスピード線
	std::unique_ptr<SpeedLines> speedLines_;

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

	// 1フレームの経過時間(エンジンが固定60fps前提(TimeManagerで60fps固定)なので、そのまま合わせる)
	static constexpr float kDeltaTime = 1.0f / 60.0f;
	// 射撃に使うマウスボタン(左ボタン)
	static constexpr int kShootMouseButton = 0;
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
};
