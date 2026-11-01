#include "GameOverScene.h"

// エンジン/システム関連
#include "Input.h"
#include "SceneManager.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"

// 操作説明
#include "UI/HelpOverlay.h"

// コンティニューで再開する位置(GameSceneがゲームオーバー直前に書き込む)
#include "objects/ContinuePoint.h"

// ImGui (マクロ定義がある場合のみ)
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif

// 定数定義 (ファイルパスやパラメータ)
namespace{
	// 画面の名前(「GAME OVER」)と操作の案内を描いた画像(仮素材。tools/generate_result_assets.py で生成する)
	const std::string kTitleTexture = "resource/UI/gameOverTitle.png";
	// 画面の名前の画像の大きさ(画像そのままの大きさで表示する)
	constexpr Vector2 kTitleSpriteSize = {1024.0f, 320.0f};
	// 画面の名前の画像の中心の高さ(画面の高さに対する割合。下に選択肢を並べるため、中央より上に置く)
	constexpr float kTitleCenterYRate = 0.35f;

	// 選べる項目の文言を描いた画像(仮素材。tools/generate_result_assets.py で生成する。GameOverScene::Option と同じ順番にする)
	const std::string kOptionTextures[] = {
		"resource/UI/continueOption.png",
		"resource/UI/toTitleOption.png",
	};
	// 選べる項目の画像の大きさ(画像そのままの大きさ)
	constexpr Vector2 kOptionSpriteSize = {640.0f, 100.0f};
	// 一番上の項目の中心の高さ(画面の高さに対する割合)と、項目どうしの間隔(画面の高さに対する割合)
	constexpr float kFirstOptionCenterYRate = 0.63f;
	constexpr float kOptionSpacingYRate = 0.12f;
	// 選んでいる項目の色(インクの黄色)と、大きさの倍率
	constexpr Vector4 kSelectedOptionColor = {1.0f, 0.85f, 0.15f, 1.0f};
	constexpr float kSelectedOptionScale = 1.1f;
	// 選んでいない項目の色(暗くして、選んでいる項目を目立たせる)と、大きさの倍率(画像そのままの大きさ)
	constexpr Vector4 kUnselectedOptionColor = {0.6f, 0.6f, 0.6f, 0.8f};
	constexpr float kUnselectedOptionScale = 1.0f;
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
	TextureManager::GetInstance()->LoadTexture(kTitleTexture);
	for(const std::string& optionTexture : kOptionTextures){
		TextureManager::GetInstance()->LoadTexture(optionTexture);
	}

	// --- スプライト生成と設定 ---
	// 画面の名前は画面の中央より上に置く(ImGui が無い Release 構成でも表示される)
	titleSprite_ = std::make_unique<Sprite>();
	titleSprite_->Initialize(spriteCommon_,kTitleTexture);
	titleSprite_->SetAnchorPoint({0.5f, 0.5f}); // 中心を基準点にする
	titleSprite_->SetSize(kTitleSpriteSize);
	titleSprite_->SetPosition({
		static_cast<float>(WinAPI::kClientWidth) * 0.5f,
		static_cast<float>(WinAPI::kClientHeight) * kTitleCenterYRate
		});

	// 選べる項目は画面の名前の下に、上から順に並べる
	for(int i = 0; i < kOptionCount; ++i){
		optionSprites_[i] = std::make_unique<Sprite>();
		optionSprites_[i]->Initialize(spriteCommon_,kOptionTextures[i]);
		optionSprites_[i]->SetAnchorPoint({0.5f, 0.5f}); // 中心を基準点にする(大きさを変えても中心がずれないようにする)
		optionSprites_[i]->SetPosition({
			static_cast<float>(WinAPI::kClientWidth) * 0.5f,
			static_cast<float>(WinAPI::kClientHeight) * (kFirstOptionCenterYRate + kOptionSpacingYRate * static_cast<float>(i))
			});
	}
	ApplyOptionLook();

	// 操作説明
	helpOverlay_ = std::make_unique<HelpOverlay>();
	helpOverlay_->Initialize(spriteCommon_);
}

// 終了処理
void GameOverScene::Finalize(){
	// unique_ptrにより自動解放されるため処理なし
}

// 更新処理
void GameOverScene::Update(){
	// スプライトの更新
	if(titleSprite_){
		titleSprite_->Update();
	}
	for(const auto& optionSprite : optionSprites_){
		optionSprite->Update();
	}

	// 操作説明の開閉(H キー)。開いている間は項目を選んだり決定したりしないよう、以降の処理を止める
	helpOverlay_->Update(input_,true);
	if(helpOverlay_->IsOpen()){
		return;
	}

	// "GAME OVER"の文字表現(専用のフォント描画機能が無いため、ImGuiでテキスト表示する)
#ifdef USE_IMGUI
	{
		ImGui::SetNextWindowPos({static_cast<float>(WinAPI::kClientWidth) * 0.5f - 100.0f, 80.0f});
		ImGui::Begin("GameOver",nullptr,ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
		ImGui::Text("GAME OVER");
		ImGui::Text("W/S: Select  SPACE: Decide");
		ImGui::End();
	}
#endif

	// W/S(↑/↓)で項目を選ぶ。端の項目からさらに進めたら、反対側の端の項目へ回り込む
	// 選び直した見た目は、次のフレームのスプライトの更新で反映される
	if(input_->TriggerKey(DIK_W) || input_->TriggerKey(DIK_UP)){
		selectedOption_ = (selectedOption_ + kOptionCount - 1) % kOptionCount;
		ApplyOptionLook();
	}
	if(input_->TriggerKey(DIK_S) || input_->TriggerKey(DIK_DOWN)){
		selectedOption_ = (selectedOption_ + 1) % kOptionCount;
		ApplyOptionLook();
	}

	// シーン遷移 (スペースキーで決定)
	// どちらを選んでもゲームシーンを作り直す。コンティニューなら最後に通ったチェックポイントから、
	// タイトルへならゲームシーン内の開始演出(タイトル)から始める
	if(input_->TriggerKey(DIK_SPACE)){
		ContinuePoint::isRequested = (selectedOption_ == kOptionContinue);
		SceneManager::GetInstance()->ChangeScene("GAME");
	}
}

// 選んでいる項目は色を付けて大きく、選んでいない項目は暗く表示する
void GameOverScene::ApplyOptionLook(){
	for(int i = 0; i < kOptionCount; ++i){
		const bool isSelected = (i == selectedOption_);
		optionSprites_[i]->SetColor(isSelected?kSelectedOptionColor:kUnselectedOptionColor);
		const float scale = isSelected?kSelectedOptionScale:kUnselectedOptionScale;
		optionSprites_[i]->SetSize({kOptionSpriteSize.x * scale, kOptionSpriteSize.y * scale});
	}
}

// 描画処理
void GameOverScene::Draw(){
	// 2Dスプライト描画
	if(spriteCommon_ && titleSprite_){
		spriteCommon_->Draw(); // 描画前処理
		// 画面の名前と選べる項目
		titleSprite_->Draw();
		for(const auto& optionSprite : optionSprites_){
			optionSprite->Draw();
		}
	}

	// 操作説明(閉じている間は左下の案内)を最前面に描画する
	helpOverlay_->Draw();
}
