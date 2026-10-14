#include "ClearScene.h"

// エンジン/システム関連
#include "Input.h"
#include "SceneManager.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"

// 操作説明
#include "UI/HelpOverlay.h"

// プレイの結果(GameSceneがクリア直前に書き込む)
#include "objects/StageResult.h"

// ImGui (マクロ定義がある場合のみ)
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif

// 定数定義 (ファイルパスやパラメータ)
namespace{
	// 背景演出用のテクスチャ(専用素材が無いため既存の丸テクスチャを流用)
	const std::string kBackgroundTexture = "resource/circle.png";
	// 背景スプライトのサイズ
	const float kBackgroundSpriteSize = 200.0f;
	// 画面の名前(「GAME CLEAR!」)と操作の案内を描いた画像(仮素材。tools/generate_result_assets.py で生成する)
	const std::string kTitleTexture = "resource/UI/clearTitle.png";
	// 画面の名前の画像の大きさ(画像そのままの大きさで表示する)
	constexpr Vector2 kTitleSpriteSize = {1024.0f, 320.0f};
	// クリア演出の背景色(緑がかった色で「成功」を表現)
	const Vector4 kBackgroundColor = {0.4f, 1.0f, 0.4f, 0.6f};
}

// コンストラクタ
ClearScene::ClearScene() = default;

// デストラクタ
ClearScene::~ClearScene() = default;

// 初期化処理
void ClearScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
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

	// 操作説明
	helpOverlay_ = std::make_unique<HelpOverlay>();
	helpOverlay_->Initialize(spriteCommon_);
}

// 終了処理
void ClearScene::Finalize(){
	// unique_ptrにより自動解放されるため処理なし
}

// 更新処理
void ClearScene::Update(){
	// スプライトの更新
	if(backgroundSprite_){
		backgroundSprite_->Update();
	}
	if(titleSprite_){
		titleSprite_->Update();
	}

	// 操作説明の開閉(H キー)。開いている間は SPACE でタイトルへ戻らないよう、以降の処理を止める
	helpOverlay_->Update(input_,true);
	if(helpOverlay_->IsOpen()){
		return;
	}

	// "CLEAR"の文字表現(専用のフォント描画機能が無いため、ImGuiでテキスト表示する)
#ifdef USE_IMGUI
	{
		ImGui::SetNextWindowPos({static_cast<float>(WinAPI::kClientWidth) * 0.5f - 100.0f, 80.0f});
		ImGui::Begin("Clear",nullptr,ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
		ImGui::Text("STAGE CLEAR!");
		ImGui::Text("Targets: %d / %d",StageResult::destroyedTargetCount,StageResult::totalTargetCount);
		ImGui::Text("Max Combo: %d",StageResult::maxCombo);
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
void ClearScene::Draw(){
	// 2Dスプライト描画
	if(spriteCommon_ && backgroundSprite_){
		spriteCommon_->Draw(); // 描画前処理
		backgroundSprite_->Draw();
		// 画面の名前は背景の上に重ねる
		if(titleSprite_){
			titleSprite_->Draw();
		}
	}

	// 操作説明(閉じている間は左下の案内)を最前面に描画する
	helpOverlay_->Draw();
}
