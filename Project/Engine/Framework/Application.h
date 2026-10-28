#pragma once
#include "Framework.h"
#include "SceneManager.h"
#include "RenderTexture.h"
#include "PostProcess.h"
#include <memory>
#include <string>

/// <summary>
/// アプリケーションクラス
/// Frameworkクラスを継承し、ゲーム固有のシーン管理やメインループ動作を実装します。
/// </summary>
class Application : public Framework{
public:
	// ポストプロセスのデバッグUIのパネル名(下段の選択パネルで選ぶときに使う)
	static constexpr const char* kPostProcessPanelName = "PostProcess Settings";

	// --- コンストラクタ・デストラクタ ---

	Application();
	~Application() override;

	// --- シングルトンアクセサ ---
	static Application* GetInstance(){ return instance_; }

	// --- Frameworkのオーバーライド ---

	/// <summary>
	/// 初期化処理
	/// アプリケーション起動時に一度だけ実行され、リソースやシーンの設定を行います。
	/// </summary>
	void Initialize() override;

	/// <summary>
	/// ゲーム固有システムの初期化
	/// </summary>
	void InitializeGameSystems() override;

	/// <summary>
	/// 終了処理
	/// リソースの解放を行います。
	/// </summary>
	void Finalize() override;

	/// <summary>
	/// 更新処理
	/// 毎フレームの計算、入力、アニメーションの更新を行います。
	/// </summary>
	void Update() override;

	/// <summary>
	/// 描画処理
	/// 毎フレームの描画コマンドを発行します。
	/// </summary>
	void Draw() override;

	// --- デバッグUI・演出関連 ---

	/// <summary>
	/// ポストプロセスのデバッグUIを表示します。
	/// </summary>
	void ShowPostProcessUI();

	void SetCurrentPPType(PostProcess::Type type){ currentPPType_ = type; }
	void StartDissolveAnimation(){ isDissolving_ = true; dissolveTimer_ = 0.0f; }
	void TriggerGlitch();

	/// <summary>
	/// 放射ブラーを一瞬かけ、時間とともに弱めて通常の表示に戻す(衝撃の瞬間の演出用)
	/// </summary>
	/// <param name="center">ブラーの中心(画面の UV 座標。左上が 0,0、右下が 1,1)</param>
	/// <param name="width">かけ始めのブラーの強さ(1サンプルごとに中心からずらす割合)</param>
	/// <param name="duration">ブラーが消えるまでの時間(秒)</param>
	void TriggerRadialBlur(const Vector2& center,float width,float duration);

	/// <summary>
	/// 体力の表現(画面の縁から迫る敵のインク)の強さを設定する
	/// どちらも 0 のときはかけない。ほかのポストプロセス(放射ブラーなど)をかけている間は、そちらを優先する
	/// </summary>
	/// <param name="amount">体力の減り具合(0: 満タン 〜 1: 尽きた)</param>
	/// <param name="flash">被弾した瞬間の画面の色づき(0〜1)</param>
	void SetDamageEffect(float amount,float flash);

private:
	// --- 静的メンバ変数 ---
	static Application* instance_;

	// --- ポストプロセス関連リソース ---
	std::unique_ptr<RenderTexture> renderTexture_;
	std::unique_ptr<PostProcess> postProcess_;
	PostProcess::Type currentPPType_ = PostProcess::Type::PostProcess;
	std::string currentMaskPath_;

	// --- Dissolveアニメーション用管理 ---
	bool isDissolving_ = false;           // アニメーション中フラグ
	float dissolveTimer_ = 0.0f;          // 経過時間
	const float kDissolveDuration = 2.0f; // 合計時間（秒）

	// --- グリッチ演出用管理 ---
	bool isGlitchActive_ = false;         // グリッチ演出中フラグ
	float glitchTimer_ = 0.0f;            // 残り時間タイマー
	const float kGlitchDuration = 0.15f;  // 演出時間（秒）

	// --- 放射ブラー演出用管理 ---
	bool isRadialBlurActive_ = false;     // 演出中フラグ
	float radialBlurTimer_ = 0.0f;        // 経過時間
	float radialBlurDuration_ = 0.0f;     // ブラーが消えるまでの時間（秒）
	float radialBlurStartWidth_ = 0.0f;   // かけ始めのブラーの強さ

	// --- 体力の表現用管理 ---
	bool isDamageEffectVisible_ = false;  // 体力の表現をかけるか(減り具合か色づきが 0 より大きい間)
};