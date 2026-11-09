#pragma once

// --- 標準ライブラリ ---
#include <memory>

// --- 基盤・システムヘッダー ---
#include "WinAPI.h"
#include "DXCommon.h"
#include "Input.h"

// --- マネージャー・共通クラス ---
#include "SpriteCommon.h"
#include "Obj3DCommon.h"

// 前方宣言
class SrvManager;
class ImGuiManager;
class SoundManager;
class CameraManager;
class ParticleManager;
class TextureManager;
class ModelManager;
class TimeManager;
class AbstractSceneFactory;

// --- シーン関連 ---
#include "BaseScene.h"

/// <summary>
/// ゲームエンジンの基盤クラス
/// ウィンドウ生成、DirectX初期化、入力管理、メインループの流れを制御します。
/// 継承先のクラスで具体的なゲームロジックと描画を実装します。
/// </summary>
class Framework{
public: // --- 公開メソッド ---

	/// <summary>
	/// 仮想デストラクタ
	/// </summary>
	virtual ~Framework() = default;

	/// <summary>
	/// 初期化処理
	/// ウィンドウ、DirectX、各マネージャの生成と初期化を行います。
	/// </summary>
	virtual void Initialize();

	/// <summary>
	/// ゲーム固有システムの初期化（継承先で実装）
	/// </summary>
	virtual void InitializeGameSystems(){}

	/// <summary>
	/// 終了処理
	/// 確保したリソースの解放や、システムの終了処理を行います。
	/// </summary>
	virtual void Finalize();

	/// <summary>
	/// 毎フレームの更新処理
	/// 入力情報の更新や、共通システムの更新を行います。
	/// </summary>
	virtual void Update();

	/// <summary>
	/// 描画処理（純粋仮想関数）
	/// 実装は継承先のクラスで行います。
	/// </summary>
	virtual void Draw() = 0;

	/// <summary>
	/// ゲームループ終了リクエストの確認
	/// </summary>
	/// <returns>終了フラグが立っていれば true</returns>
	virtual bool IsEndRequest(){ return endRequest_; }

protected: // --- 継承先で実装する処理 ---

	/// <summary>
	/// ESC キーによる終了の受け付け(入力の更新の後、シーンの更新より前に毎フレーム呼ぶ)
	/// 既定では ESC キーですぐに終了リクエストを送る。継承先で終了確認メニューなどに差し替える
	/// </summary>
	/// <returns>このフレームのシーンの更新を止めるなら true</returns>
	virtual bool UpdateQuitRequest();

protected: // --- メンバ変数（継承先からもアクセス可能） ---

	// ゲームループ終了フラグ
	bool endRequest_ = false;

	// --- コアシステム群 (所有権を保持) ---
	std::unique_ptr<WinAPI> winApi_;         // ウィンドウ管理
	std::unique_ptr<DXCommon> dxCommon_;     // DirectX基本設定
	std::unique_ptr<Input> input_;           // 入力管理

	// --- 描画共通設定 (所有権を保持) ---
	std::unique_ptr<SpriteCommon> spriteCommon_;     // スプライト共通設定
	std::unique_ptr<Obj3dCommon> object3dCommon_;    // 3Dオブジェクト共通設定

	// --- シーン管理 ---
	std::unique_ptr<AbstractSceneFactory> sceneFactory_; // シーン生成ファクトリー
};