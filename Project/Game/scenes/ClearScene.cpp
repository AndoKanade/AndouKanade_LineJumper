#include "ClearScene.h"

// エンジン/システム関連
#include "Input.h"
#include "SceneManager.h"
#include "SoundManager.h"
#include "SpriteCommon.h"

// 結果を順に見せる表示
#include "UI/ClearResultUI.h"

// プレイの結果(GameSceneがクリア直前に全ステージの合計を書き込む)
#include "objects/StageResult.h"

// 効果音の音量
#include "Title/StartSequence.h"

// 定数定義 (ファイルパス)
namespace{
	// タイトルへ戻るときの決定音(タイトル画面・ESC の終了確認メニューと同じ音)
	const std::string kDecideSePath = "resource/music/se/decide.mp3";
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

	// ランクは、GameScene が書き込んだ全ステージの合計から決める
	const ClearResultUI::Rank rank = ClearResultUI::CalculateRank(StageResult::destroyedTargetCount,StageResult::totalTargetCount,StageResult::maxCombo);

	// 結果を順に見せる表示(全ステージの合計と、決めたランクを渡す)
	resultUI_ = std::make_unique<ClearResultUI>();
	resultUI_->Initialize(spriteCommon_,StageResult::destroyedTargetCount,StageResult::totalTargetCount,StageResult::maxCombo,rank);

	// タイトルへ戻るときの決定音
	SoundManager::GetInstance()->SoundLoadFile(kDecideSePath);
}

// 終了処理
void ClearScene::Finalize(){
	// unique_ptrにより自動解放されるため処理なし
}

// 更新処理
void ClearScene::Update(){
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
			SoundManager::GetInstance()->PlayAudio(kDecideSePath,StartSequence::GetSeVolume());
			SceneManager::GetInstance()->ChangeScene("GAME");
		}
	}
}

// 描画処理
void ClearScene::Draw(){
	// 結果の表示(描画前処理も含む)
	resultUI_->Draw();
}
