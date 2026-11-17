#include "ClearScene.h"

// エンジン/システム関連
#include "Input.h"
#include "SceneManager.h"
#include "SpriteCommon.h"

// 操作説明
#include "UI/HelpOverlay.h"

// 結果を順に見せる表示
#include "UI/ClearResultUI.h"

// プレイの結果(GameSceneがクリア直前に全ステージの合計を書き込む)
#include "objects/StageResult.h"

// プレイ会用の設定か
#include "objects/StageInfo.h"

#include <random>

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

	// ランクは、GameScene が書き込んだ全ステージの合計から決める
	// プレイ会用の設定では、結果がステージ3の分(的1つ)だけでほぼ C になるため、ランクをランダムに決めて、どのランクの演出も見られるようにする
	ClearResultUI::Rank rank = ClearResultUI::CalculateRank(StageResult::destroyedTargetCount,StageResult::totalTargetCount,StageResult::maxCombo);
	if(StageInfo::isPlaySessionMode){
		std::random_device randomDevice;
		std::uniform_int_distribution<int> distribution(static_cast<int>(ClearResultUI::Rank::S),static_cast<int>(ClearResultUI::Rank::C));
		rank = static_cast<ClearResultUI::Rank>(distribution(randomDevice));
	}

	// 結果を順に見せる表示(全ステージの合計と、決めたランクを渡す)
	resultUI_ = std::make_unique<ClearResultUI>();
	resultUI_->Initialize(spriteCommon_,StageResult::destroyedTargetCount,StageResult::totalTargetCount,StageResult::maxCombo,rank);

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
	// 操作説明の開閉(H キー)。開いている間は SPACE でタイトルへ戻らないよう、以降の処理を止める
	helpOverlay_->Update(input_,true);
	if(helpOverlay_->IsOpen()){
		return;
	}

	// クリア画面へ切り替える演出が画面を覆っている間は、結果を出し始めない(出だしを見逃さないようにする)
	if(SceneManager::GetInstance()->IsTransitioning()){
		resultUI_->Update(0.0f);
		return;
	}
	resultUI_->Update(kDeltaTime);

	// SPACE キー: 結果を出している途中なら飛ばしてすべて出し、出し終えて少したっていればタイトルへ戻る
	// 出し終えた直後は押しても何もしない(飛ばした直後の連打で、結果を見ないまま戻らないようにする)
	// タイトルはゲームシーン内の開始演出として表示するため、ゲームシーンを最初から始め直す
	if(input_->TriggerKey(DIK_SPACE)){
		if(!resultUI_->IsFinished()){
			resultUI_->Skip();
			return;
		}
		if(resultUI_->CanReturnToTitle()){
			SceneManager::GetInstance()->ChangeScene("GAME");
		}
	}
}

// 描画処理
void ClearScene::Draw(){
	// 結果の表示(描画前処理も含む)
	resultUI_->Draw();

	// 操作説明(閉じている間は左下の案内)を最前面に描画する
	helpOverlay_->Draw();
}
