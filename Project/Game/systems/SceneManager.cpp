#include "SceneManager.h"
#include <cassert>
#include <utility>

// -------------------------------------------------
// シングルトン管理
// -------------------------------------------------

SceneManager* SceneManager::GetInstance(){
	// 静的ローカル変数としてインスタンスを保持 (Meyers Singleton)
	// 初回呼び出し時に生成され、プログラム終了時に自動的に破棄されます。
	static SceneManager instance;
	return &instance;
}

void SceneManager::Finalize(){
	// 現在アクティブなシーンがあれば終了処理を実行
	if(scene_){
		scene_->Finalize();
	}
	// unique_ptr をリセットしてメモリを解放
	scene_.reset();

	// SceneManager はプログラム終了まで残るため、暗転用のスプライトは DirectX の終了前にここで解放する
	transition_.Finalize();
}

void SceneManager::SetCommonPtr(Obj3dCommon* common,Input* input,SpriteCommon* spriteCommon){
	object3dCommon_ = common;
	input_ = input;
	spriteCommon_ = spriteCommon;

	// 暗転はすべてのシーンで共通して使うため、スプライト描画の共通設定を受け取った時点で用意しておく
	transition_.Initialize(spriteCommon_);
}

// -------------------------------------------------
// メインループ処理
// -------------------------------------------------

void SceneManager::Update(){
	// --- 暗転の更新 ---
	// 画面を覆い終わった瞬間に、シーンの切り替え予約やリスタートなどの処理が呼ばれる
	transition_.Update(kDeltaTime);

	// --- シーン切り替えリクエストの処理 ---
	// 次のシーン名が設定されている場合、切り替え処理を行う
	if(!nextSceneName_.empty()){

		// 1. 現在のシーンの終了処理
		if(scene_){
			scene_->Finalize();
		}

		// 2. 新しいシーンの生成
		// unique_ptr::reset() を使用して、ファクトリーが生成したインスタンスの所有権を受け取る
		if(sceneFactory_){
			scene_ = sceneFactory_->CreateScene(nextSceneName_);
		}

		// 3. 新しいシーンの初期化と依存関係の注入
		if(scene_){
			// シーンマネージャ自身の参照をセット (シーン遷移リクエスト用)
			scene_->SetSceneManager(this);

			// 共通リソース (モデル、入力、スプライト等) が揃っているか確認して渡す
			if(object3dCommon_ && input_ && spriteCommon_){
				scene_->Initialize(object3dCommon_,input_,spriteCommon_);
			}
		}

		// リクエストをクリア (切り替え完了)
		nextSceneName_.clear();
	}

	// --- 現在のシーンの更新 ---
	if(scene_){
		scene_->Update();
	}
}

void SceneManager::Draw(){
	// 現在のシーンがあれば描画処理を実行
	if(scene_){
		scene_->Draw();
	}

	// 暗転はすべてのシーンの表示の上から重ねる
	transition_.Draw();
}

// -------------------------------------------------
// シーン制御
// -------------------------------------------------

void SceneManager::ChangeScene(const std::string& sceneName){
	// ファクトリーがセットされていない状態で切り替えはできないためアサート
	assert(sceneFactory_);

	// 最初のシーンはまだ何も映っていないため、暗転せずに次の Update() の冒頭ですぐ切り替える
	if(!scene_){
		nextSceneName_ = sceneName;
		return;
	}

	// 画面を覆い終わった瞬間に次のシーン名を保存する
	// ※実際の切り替え処理は、その直後の Update() の冒頭で行われるため、重い初期化も真っ暗な間に済む
	transition_.Start([this,sceneName](){
		nextSceneName_ = sceneName;
	});
}

bool SceneManager::StartTransition(std::function<void()> onCovered,SceneTransition::Style style){
	return transition_.Start(std::move(onCovered),style);
}