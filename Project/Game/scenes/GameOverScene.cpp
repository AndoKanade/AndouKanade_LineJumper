#include "GameOverScene.h"

// エンジン/システム関連
#include "Input.h"
#include "SceneManager.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"

// ImGui (マクロ定義がある場合のみ)
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif

// 定数定義 (ファイルパスやパラメータ)
namespace{
	// 背景演出用のテクスチャ(専用素材が無いためクリア画面と同じ丸テクスチャを流用)
	const std::string kBackgroundTexture = "resource/circle.png";
	// 背景スプライトのサイズ
	const float kBackgroundSpriteSize = 200.0f;
	// 画面の名前(「GAME OVER」)と操作の案内を描いた画像(仮素材。tools/generate_result_assets.py で生成する)
	const std::string kTitleTexture = "resource/UI/gameOverTitle.png";
	// 画面の名前の画像の大きさ(画像そのままの大きさで表示する)
	constexpr Vector2 kTitleSpriteSize = {1024.0f, 320.0f};
	// ゲームオーバー演出の背景色(赤がかった色で「失敗」を表現)
	const Vector4 kBackgroundColor = {1.0f, 0.3f, 0.3f, 0.6f};
}

// コンストラクタ
GameOverScene::GameOverScene() = default;

// デストラクタ
GameOverScene::~GameOverScene() = default;

// 初期化処理
void GameOverScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
	// メンバ変数の保持
	object3dCommon_ = object3dCommon;
	input_ = input;
	spriteCommon_ = spriteCommon;

	// --- リソースのロード ---
	TextureManager::GetInstance()->LoadTexture(kBackgroundTexture);
	TextureManager::GetInstance()->LoadTexture(kTitleTexture);

	// --- スプライト生成と設定 ---
	backgroundSprite_ = std::make_unique<Sprite>();
	backgroundSprite_->Initialize(spriteCommon_,kBackgroundTexture);
	backgroundSprite_->SetAnchorPoint({0.5f, 0.5f}); // 中心を基準点にする
	backgroundSprite_->SetSize({kBackgroundSpriteSize, kBackgroundSpriteSize});
	backgroundSprite_->SetPosition({
		static_cast<float>(WinAPI::kClientWidth) * 0.5f,
		static_cast<float>(WinAPI::kClientHeight) * 0.5f
		});
	backgroundSprite_->SetColor(kBackgroundColor);

	// 画面の名前は画面の中央に置く(ImGui が無い Release 構成でも表示される)
	titleSprite_ = std::make_unique<Sprite>();
	titleSprite_->Initialize(spriteCommon_,kTitleTexture);
	titleSprite_->SetAnchorPoint({0.5f, 0.5f}); // 中心を基準点にする
	titleSprite_->SetSize(kTitleSpriteSize);
	titleSprite_->SetPosition({
		static_cast<float>(WinAPI::kClientWidth) * 0.5f,
		static_cast<float>(WinAPI::kClientHeight) * 0.5f
		});
}

// 終了処理
void GameOverScene::Finalize(){
	// unique_ptrにより自動解放されるため処理なし
}

// 更新処理
void GameOverScene::Update(){
	// スプライトの更新
	if(backgroundSprite_){
		backgroundSprite_->Update();
	}
	if(titleSprite_){
		titleSprite_->Update();
	}

	// "GAME OVER"の文字表現(専用のフォント描画機能が無いため、ImGuiでテキスト表示する)
#ifdef USE_IMGUI
	{
		ImGui::SetNextWindowPos({static_cast<float>(WinAPI::kClientWidth) * 0.5f - 100.0f, 80.0f});
		ImGui::Begin("GameOver",nullptr,ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
		ImGui::Text("GAME OVER");
		ImGui::Text("Press SPACE to return to Title");
		ImGui::End();
	}
#endif

	// シーン遷移 (スペースキーでタイトルへ)
	// タイトルはゲームシーン内の開始演出として表示するため、ゲームシーンを最初から始め直す
	if(input_->TriggerKey(DIK_SPACE)){
		SceneManager::GetInstance()->ChangeScene("GAME");
	}
}

// 描画処理
void GameOverScene::Draw(){
	// 2Dスプライト描画
	if(spriteCommon_ && backgroundSprite_){
		spriteCommon_->Draw(); // 描画前処理
		backgroundSprite_->Draw();
		// 画面の名前は背景の上に重ねる
		if(titleSprite_){
			titleSprite_->Draw();
		}
	}
}
