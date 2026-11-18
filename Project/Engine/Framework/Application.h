#pragma once
#include "Framework.h"
#include "SceneManager.h"
#include "RenderTexture.h"
#include "PostProcess.h"
#include "PostEffectState.h"
#include <memory>
#include <string>

// 前方宣言
class QuitConfirmMenu;

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

	// 指定した種類のポストプロセスをかけ続ける状態へ切り替える
	void SetCurrentPPType(PostProcess::Type type);
	// ディゾルブで画面を消していく状態へ切り替える
	void StartDissolveAnimation();
	// グリッチを一瞬だけかける状態へ切り替える
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

protected:
	/// <summary>
	/// ESC キーで終了確認メニューを開き、「はい」を選んだら終了リクエストを送る
	/// </summary>
	/// <returns>メニューを開いている間(開閉の動きの間も含む)は true(ゲームを止める)</returns>
	bool UpdateQuitRequest() override;

private:
	/// <summary>
	/// ポストエフェクトの動作モード(状態)を切り替え、新しい状態の始まりの値をポストプロセスに書き込む
	/// </summary>
	/// <param name="nextState">切り替え先の状態</param>
	void ChangePostEffectState(std::unique_ptr<PostEffectState> nextState);

	// --- 静的メンバ変数 ---
	static Application* instance_;

	// --- ポストプロセス関連リソース ---
	std::unique_ptr<RenderTexture> renderTexture_;
	std::unique_ptr<PostProcess> postProcess_;
	std::string currentMaskPath_;

	// --- ポストエフェクトの動作モード(状態) ---
	// 描く種類と、グリッチ・放射ブラー・ディゾルブの進め方は今の状態に任せる(Stateパターン)
	std::unique_ptr<PostEffectState> postEffectState_;

	// --- 体力の表現用管理 ---
	bool isDamageEffectVisible_ = false;  // 体力の表現をかけるか(減り具合か色づきが 0 より大きい間)

	// --- 終了確認メニュー ---
	std::unique_ptr<QuitConfirmMenu> quitConfirmMenu_;
};