#pragma once

// --- 標準ライブラリ ---
#include <string>
#include <memory>
#include <functional>

// --- エンジン内ヘッダー ---
#include "BaseScene.h"
#include "AbstractSceneFactory.h"
#include "SceneTransition.h"

// --- 前方宣言 ---
class Obj3dCommon;
class SpriteCommon;
class Input;

/// <summary>
/// シーン管理クラス (シングルトンパターン)
/// ゲーム内のシーン遷移、現在のシーンの更新・描画を一括管理します。
/// </summary>
class SceneManager{
private:
	// シングルトンパターンのため、コンストラクタとデストラクタは隠蔽
	SceneManager() = default;
	~SceneManager() = default;

public:
	// コピーコンストラクタと代入演算子を無効化 (複製の禁止)
	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	// -------------------------------------------------
	// メインループ・ライフサイクル
	// -------------------------------------------------

	/// <summary>
	/// インスタンスの取得 (Meyers Singleton)
	/// </summary>
	static SceneManager* GetInstance();

	/// <summary>
	/// 終了処理
	/// 保持しているシーンやリソースの解放を行います。
	/// </summary>
	void Finalize();

	/// <summary>
	/// 更新処理
	/// 現在のシーンの更新と、シーン切り替えリクエストの処理を行います。
	/// </summary>
	void Update();

	/// <summary>
	/// 描画処理
	/// 現在のシーンの描画メソッドを呼び出します。
	/// </summary>
	void Draw();

	/// <summary>
	/// 次のシーンへの切り替え予約
	/// 画面を暗転で覆い終わった後の Update() の冒頭で切り替わります。
	/// 最初のシーンだけは、まだ何も映っていないため暗転せずに切り替えます。
	/// 暗転中に呼ばれた場合は無視します(同じ遷移が二重に起きないようにするため)。
	/// </summary>
	/// <param name="sceneName">遷移先のシーン名</param>
	void ChangeScene(const std::string& sceneName);

	/// <summary>
	/// シーンを切り替えずに暗転だけを行う(落下リスタートなど、シーンの中で画面のつながりが切れるとき用)
	/// 画面を覆い終わった瞬間に onCovered が1回だけ呼ばれます。暗転中なら何もせず false を返します。
	/// </summary>
	/// <param name="onCovered">画面を覆い終わったときに呼ぶ処理</param>
	bool StartTransition(std::function<void()> onCovered);

	/// <summary>
	/// 暗転中かどうか
	/// 暗転中はシーン側で入力・ゲームの進行を止めるために使います。
	/// </summary>
	bool IsTransitioning() const{ return transition_.IsActive(); }

	// -------------------------------------------------
	// セットアップ (初期化時に使用)
	// -------------------------------------------------

	/// <summary>
	/// シーン生成ファクトリーの登録
	/// </summary>
	void SetFactory(AbstractSceneFactory* factory){
		sceneFactory_ = factory;
	}

	/// <summary>
	/// 共通リソースポインタの設定
	/// 各シーンに渡す基盤システムのポインタを保持します。
	/// </summary>
	void SetCommonPtr(Obj3dCommon* common,Input* input,SpriteCommon* spriteCommon);

private:
	// -------------------------------------------------
	// メンバ変数
	// -------------------------------------------------

	// シーン生成用ファクトリー (Frameworkが所有権を持つため、ここはポインタ参照のみ)
	AbstractSceneFactory* sceneFactory_ = nullptr;

	// 現在実行中のシーン
	// unique_ptr により、シーン切り替え時や終了時に自動的にメモリが解放されます
	std::unique_ptr<BaseScene> scene_;

	// 次に遷移するシーンの名前 (空文字列でない場合、遷移リクエストありと判断)
	std::string nextSceneName_ = "";

	// シーンの切り替え・落下リスタートなどで画面のつながりが切れる瞬間を隠す暗転
	// シーンを切り替えても途中で消えないよう、シーンではなくここで持つ
	SceneTransition transition_;

	// --- 共通リソース (借用ポインタ) ---
	Obj3dCommon* object3dCommon_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;
	Input* input_ = nullptr;

	// 暗転を進める1フレームの時間(秒)。フレームレートは TimeManager で60fpsに固定している
	static constexpr float kDeltaTime = 1.0f / 60.0f;
};