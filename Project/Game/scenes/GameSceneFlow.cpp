#include "GameScene.h"
#include "GameSceneInternal.h"
#include "CameraManager.h"
#include "SceneManager.h"
#include "Application.h"
#include "GlobalVariables.h"

// レールエディター
#include "Editor/RailEditor.h"
// ステージ開始演出(タイトル → カメラの回り込み → スタート台で準備 → レールへ乗り込む)
#include "Title/StartSequence.h"
// 出発の演出(スタート台からステージのレールへ移るときの演出)
#include "Title/Departure/DepartureDirector.h"
// ゴールの的を壊したあと、大ジャンプでスタート台へ戻る演出
#include "Title/ReturnJump.h"
// 着地した後、クリアしたステージの通路の壁を壊して見せる演出
#include "Title/BarrierBreakCutscene.h"
// ゲームオブジェクト
#include "objects/Player.h"
#include "objects/StartPlatform.h"
#include "objects/PlayerBulletManager.h"
#include "objects/TargetManager.h"
#include "objects/GoalTarget.h"
#include "objects/EnemyManager.h"
#include "objects/InkEffectManager.h"
#include "objects/ComboCounter.h"
#include "objects/ContinuePoint.h"
#include "objects/StageResult.h"
#include "objects/StageInfo.h"
// カメラ
#include "Camera/RailCamera.h"
#include "Camera/ScreenImpact.h"
#include "Camera/CameraPose.h"
// UI
#include "UI/StageClearUI.h"
#include "UI/StageLockedNotice.h"
#include "UI/RailSwitchPrompt.h"
#include "UI/KeyTargetMarker.h"
#include "UI/SpeedLines.h"
#include <vector>

// GameSceneのうち、ゲームの流れ(始め直し・レールへの乗り込み・チェックポイント・ミスと残機・ステージの切り替えと鍵のレール・出発と帰還・クリア・壁を壊す演出)をまとめたファイル

using namespace GameSceneInternal;

namespace{
	// 鍵の的を壊して出たレールの管を伸ばす速さを、レールを進む全体の速さより速くする量(1秒あたりの長さ)
	constexpr float kRailUnlockExtraSpeed = 3.0f;
}

// ゲームを初期状態(レール先頭)から始め直す
// Playに入った瞬間と、レールから落ちたときのリスタートで共通して使う
void GameScene::ResetPlayState(){
	// レール位置・オンレール状態・体力を初期状態に戻す
	player_->Reset();
	// 照準を正面に戻す
	railCamera_->Reset();
	// 的を復活させる
	targetManager_->Reset();
	// ゴールの的も、体力満タンでゴールのレールに乗るまで出ていない状態に戻す
	goalTarget_->Reset();
	// 開始時に残っている弾もリセットし、次に押したときすぐ撃てるようにする
	bulletManager_->Clear();
	fireCooldown_ = 0.0f;

	// レールを0番へ戻し、インクで塗った区間も消す
	if(railEditor_){
		railEditor_->SwitchActiveRail(0);
		railEditor_->ClearPaint();

		// プレイヤーを台の真ん中から始めさせる(今のステージのレールの向き、つまりそのステージのゲートの方を向かせる)
		player_->StartOnPlatform(startPlatform_->GetSpawnPosition(),railEditor_->GetRotationOnRail(0.0f));

		// 的をすべて復活させたため、鍵の的があるレールは出ていない状態にすぐ戻す
		ApplyRailLocks(true);
		ResetOtherStageRailLocks();
		// 始め直したら、どのレールにもまだ乗っていないことにする
		riddenRails_.assign(static_cast<size_t>(railEditor_->GetRailCount()),false);
	}

	// 出ている演出・連鎖数・スピード線・画面の揺れ・出発の演出も消す
	inkEffect_->Reset();
	screenImpact_->Reset();
	departureDirector_->Reset();
	returnJump_->Reset();
	// 壁を壊す演出の途中でやり直したときは、演出を止め、クリアしたステージの壁は壊れた状態にする
	barrierCutscene_->Reset();
	startPlatform_->ReleaseBarrierHold();
	comboCounter_->Reset();
	speedLines_->Reset();
	stageClearUI_->Reset();
	goalBannerUI_->Reset();
	wasGoalTargetAppeared_ = false;
	stageLockedNotice_->Reset();
	railSwitchPrompt_->Reset();
	keyTargetMarker_->Reset();

	// 雑魚敵も撃破前の初期状態(体力満タン)から始める
	enemyManager_->Reset();

	// やられ演出の最中にRキーなどでやり直したときも、自機を出して操作できる状態に戻す
	isDefeated_ = false;
	defeatTimer_ = 0.0f;
	isStageCleared_ = false;
	stageClearTimer_ = 0.0f;
	isPlaySessionClearPending_ = false;

	// やり直したら、ゆっくりにしている途中でも元の速さに戻す(最初の1回を済ませたかは残す)
	isRailSwitchSlowActive_ = false;
	railSwitchSlowTimer_ = 0.0f;
	railSwitchTimeScale_ = 1.0f;
	lastRailSwitchSide_ = 0;
}

// タイトル・スタート演出を始め、デバッグ用の設定に合わせてレールへ乗せる
void GameScene::StartPlaySequence(){
	// 選んだ制御点から始めるときも、タイトル・スタート台を飛ばしてすぐプレイを始める
	startSequence_->Start(skipTitle_ || isPlayingFromPoint_);
	if(isPlayingFromPoint_){
		BoardAtPlayStartPoint();
	} else if(skipTitle_){
		// タイトルを飛ばすときはスタート台の上も飛ばし、レールの始点に乗せた状態からすぐ始める
		player_->BoardRailStart();
	}
}

// ゲームオーバー画面でコンティニューを選んだときに、タイトル・スタート台を飛ばして最後に通ったチェックポイントから始める
// ResetPlayState()でレールを0番へ戻し、的もすべて復活させているため、その後に呼ぶ
void GameScene::StartFromContinuePoint(){
	// 1回使ったら下ろし、タイトルからやり直したときなどに同じ位置から始めないようにする
	ContinuePoint::isRequested = false;
	startSequence_->Start(true);

	// 再開した後にまたレールから落ちたときも、同じチェックポイントからやり直せるよう覚え直す
	checkpoint_.railIndex = ContinuePoint::railIndex;
	checkpoint_.pointIndex = ContinuePoint::pointIndex;
	checkpoint_.targetAliveStates = ContinuePoint::targetAliveStates;
	if(checkpoint_.railIndex >= 0){
		RespawnAtCheckpoint();
	} else{
		// チェックポイントを通る前に残機が尽きたときは、レールの始点から始める
		player_->BoardRailStart();
	}
	// 落ちたときのやり直しと同じく、透明な体がだんだん元に戻る見せ方で始める
	player_->StartReveal(GetRespawnRevealStyle(),kRespawnRevealDuration);
}

// Playに入ったときに覚えておいた制御点の位置からレールに乗せる
// ResetPlayState()でレールを0番へ戻しているため、制御点のあるレールへ切り替えてから乗せる
void GameScene::BoardAtPlayStartPoint(){
	railEditor_->SwitchActiveRail(editRailIndex_);
	player_->BoardRailStart(railEditor_->GetTOfControlPoint(editPointIndex_));
}

// レールを進んでいる間に、チェックポイントの制御点を通ったら、その位置と壊した的を覚えておく
// 同じチェックポイントを通り直したとき(チェックポイントからやり直した直後など)は覚え直さない
void GameScene::UpdateCheckpoint(){
	const int pointIndex = railEditor_->FindPassedCheckpoint(player_->GetRailT());
	if(pointIndex < 0){
		return;
	}
	const int railIndex = railEditor_->GetActiveRailIndex();
	if(railIndex == checkpoint_.railIndex && pointIndex == checkpoint_.pointIndex){
		return;
	}
	// 選んだ制御点から始めているときは、始めた位置より手前のチェックポイントは通ったものとしない(落ちても選んだ制御点から始め直せるようにする)
	if(isPlayingFromPoint_ && railIndex == editRailIndex_ && pointIndex <= editPointIndex_){
		return;
	}
	checkpoint_.railIndex = railIndex;
	checkpoint_.pointIndex = pointIndex;
	checkpoint_.targetAliveStates = targetManager_->GetAliveStates();
}

// 最後に通ったチェックポイントの位置からレールに乗せ、壊した的をチェックポイントを通った時点の状態に戻す
// ResetPlayState()でレールを0番へ戻し、的もすべて復活させているため、その後に呼ぶ
void GameScene::RespawnAtCheckpoint(){
	railEditor_->SwitchActiveRail(checkpoint_.railIndex);
	player_->BoardRailStart(railEditor_->GetTOfControlPoint(checkpoint_.pointIndex));
	targetManager_->RestoreAliveStates(checkpoint_.targetAliveStates);
	// チェックポイントを通った時点で出ていたレールは出たまま、出ていなかったレールは出ていない状態にすぐ戻す
	ApplyRailLocks(true);
}

// 暗転で画面を覆っている間に、最後に通ったチェックポイント(通っていなければスタート台)からやり直す
// 一瞬で始点へ戻ると何が起きたか分かりにくいため、覆い終わってから戻す
void GameScene::RestartAfterMiss(){
	sceneManager_->StartTransition([this](){
		ResetPlayState();
		// チェックポイントを通っていれば、そこから始め直す
		// 通っていなくても、選んだ制御点から始めているときは、落ちても同じ制御点から始め直す
		if(checkpoint_.railIndex >= 0){
			RespawnAtCheckpoint();
		} else if(isPlayingFromPoint_){
			BoardAtPlayStartPoint();
		}
		// 透明な体がだんだん元に戻る見せ方で、復活したことを分かりやすくする
		player_->StartReveal(GetRespawnRevealStyle(),kRespawnRevealDuration);
	},GetTransitionStyle(kFallTransitionKey));
}

// 落下・体力切れで残機を1つ減らし、残っていればやり直し、尽きたらやられ演出を始める
void GameScene::LoseLife(){
	--lives_;
	if(lives_ > 0){
		RestartAfterMiss();
		return;
	}

	// ゲームオーバー画面でコンティニューを選んだら最後に通ったチェックポイントから始められるよう、シーンが作り直される前に書き込んでおく
	ContinuePoint::railIndex = checkpoint_.railIndex;
	ContinuePoint::pointIndex = checkpoint_.pointIndex;
	ContinuePoint::targetAliveStates = checkpoint_.targetAliveStates;
	// すぐに切り替えず、やられ演出で「やられた」ことを見せてからゲームオーバー画面へ移る
	StartDefeat();
}

// 残機が尽きたときのやられ演出を始める
// 自機をピンクのインクにして弾けさせ、一瞬動きを止めて画面を揺らし、自機を中心に放射ブラーをかける
void GameScene::StartDefeat(){
	isDefeated_ = true;
	defeatTimer_ = 0.0f;

	const Vector3 burstPosition = player_->GetHitSphere().center;
	inkEffect_->EmitDefeatBurst(burstPosition);
	screenImpact_->Trigger(kDefeatHitStopDuration,kDefeatShakeAmplitude,kDefeatShakeDuration);
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		Application::GetInstance()->TriggerRadialBlur(CameraPoseMath::WorldToScreenUv(burstPosition,activeCamera->GetViewProjectionMatrix()),kDefeatRadialBlurWidth,kDefeatRadialBlurDuration);
	}
}

// やられ演出を進め、終わったらゲームオーバー画面へ移る
// ゲームオーバーは暗いインクが上から垂れてきて画面を覆う演出で切り替える
void GameScene::UpdateDefeat(){
	const bool wasWaiting = defeatTimer_ < kDefeatDuration;
	defeatTimer_ += kDeltaTime;
	if(wasWaiting && defeatTimer_ >= kDefeatDuration){
		sceneManager_->ChangeScene("GAMEOVER",SceneTransition::Style::InkDrip);
	}
}

// 鍵の的が残っていて出ていないレールを求め、レールの描き方と着地できるレールに反映する
// レールが出ているかは的が壊れているかから毎回求めるため、チェックポイントで的を戻すとレールも一緒に戻る
void GameScene::ApplyRailLocks(bool isInstant){
	lockedRails_.assign(static_cast<size_t>(railEditor_->GetRailCount()),false);
	targetManager_->FillLockedRails(lockedRails_);
	if(isInstant){
		railEditor_->ResetRailLocks(lockedRails_);
	} else{
		// 出たレールの管は、プレイヤーがレールを進む速さより少し速く伸ばし、追いつかれずに先へ伸びていくように見せる
		const float growSpeed = GlobalVariables::GetInstance()->GetFloatValue(kGameSceneGroup,kRailSpeedKey) + kRailUnlockExtraSpeed;
		railEditor_->UpdateRailLocks(lockedRails_,growSpeed,kDeltaTime);
	}
}

// 編集・プレイするステージを切り替える
void GameScene::SwitchStage(int stageIndex){
	if(!StageInfo::IsValidStageIndex(stageIndex)){
		return;
	}

	// シーンが作り直されても同じステージから始めるよう、シーンをまたいで残る値にも書き込む
	StageInfo::currentStageIndex = stageIndex;
	railEditor_->SetStageIndex(stageIndex);
	targetManager_->SetStageIndex(stageIndex);
	enemyManager_->SetStageIndex(stageIndex);

	// Playから戻ったときに選び直すレール・制御点は前のステージのものなので、切り替えた先の0番のレールにする
	editRailIndex_ = 0;
	editPointIndex_ = -1;
	// 通ったチェックポイントは前のステージのレールのものなので忘れる
	checkpoint_ = Checkpoint{};

	// 的をすべて復活させたため、鍵の的があるレールは出ていない状態にすぐ戻す
	ApplyRailLocks(true);
	ResetOtherStageRailLocks();
	// 乗ったレールの記録は前のステージのレールのものなので、切り替えた先のレールの本数で作り直す
	riddenRails_.assign(static_cast<size_t>(railEditor_->GetRailCount()),false);
}

// 今のステージ以外のレールの出方を、そのステージの鍵の的がすべて残っている状態にする
// 今のステージ以外の的は壊せないため、ステージを切り替えたときとやり直すときに1回求めれば足りる
void GameScene::ResetOtherStageRailLocks(){
	std::vector<bool> stageLockedRails;
	for(int stage = 0; stage < StageInfo::kStageCount; ++stage){
		if(stage == railEditor_->GetStageIndex()){
			continue;
		}
		stageLockedRails.assign(static_cast<size_t>(railEditor_->GetStageRailCount(stage)),false);
		targetManager_->FillStageLockedRails(stage,stageLockedRails);
		railEditor_->ResetStageRailLocks(stage,stageLockedRails);
	}
}

// 出発の演出が動かす・参照するものをまとめる
DepartureContext GameScene::MakeDepartureContext() const{
	DepartureContext context;
	context.player = player_.get();
	context.railEditor = railEditor_.get();
	context.startPlatform = startPlatform_.get();
	context.inkEffect = inkEffect_.get();
	context.screenImpact = screenImpact_.get();
	context.sceneManager = sceneManager_;
	context.camera = CameraManager::GetInstance()->GetCamera(kDefaultCameraName);
	return context;
}

// レールに乗り込んで「GO!」でプレイを始める瞬間の演出
// 足元から輪とインクのしぶきを広げ、足元には大きなインクの跡を残し、高く飛び散ったしぶきは台に落ちたところに跡を残す
void GameScene::PlayBoardedEffects(const Vector3& boardedPos){
	startPlatform_->EmitLaunchRing(boardedPos);
	startPlatform_->PaintInk(boardedPos,kLaunchInkRadius);
	inkEffect_->EmitInkSplash(boardedPos,kWorldUp,kLaunchSplashCount);
	inkEffect_->EmitLaunchBurst(boardedPos,kLaunchBurstCount);
}

// ゴールの的を壊して大ジャンプでスタート台へ戻ってきたときに、ステージを始め直し、着地の演出を出す
void GameScene::RestartFromReturnJump(const Vector3& landedPos){
	// 新しく始め直すため、通ったチェックポイントも忘れ、残機も満タンに戻す
	checkpoint_ = Checkpoint{};
	lives_ = kMaxLives;
	// 的・敵・レールの塗り・ゴールの的などを初期状態に戻し、プレイヤーをスタート台の上に立たせる
	// (演出・画面の揺れもここで消えるため、着地の演出はこの後に出す)
	ResetPlayState();
	// タイトルを飛ばし、出発ゲートを閉じてスタート台の上を動ける段階から始める
	startSequence_->StartFromStandby();

	PlayReturnLandEffects(landedPos);
}

// 大ジャンプでスタート台に着地した瞬間の演出
// 着地点から輪とインクのしぶきを広げて台にインクの跡を残し、一瞬動きを止めて画面を揺らし、着地点を中心に放射ブラーをかける
void GameScene::PlayReturnLandEffects(const Vector3& landedPos){
	PlayBoardedEffects(landedPos);
	screenImpact_->Trigger(kReturnLandHitStopDuration,kReturnLandShakeAmplitude,kReturnLandShakeDuration);
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		Application::GetInstance()->TriggerRadialBlur(CameraPoseMath::WorldToScreenUv(landedPos,activeCamera->GetViewProjectionMatrix()),kReturnLandRadialBlurWidth,kReturnLandRadialBlurDuration);
	}
}

// 最後のステージのゴールの的を壊し、大ジャンプでスタート台に着地したときに、着地の演出を出してクリア画面へ移るのを待ち始める
void GameScene::StartStageClear(const Vector3& landedPos){
	isStageCleared_ = true;
	stageClearTimer_ = 0.0f;
	PlayReturnLandEffects(landedPos);
}

// 着地の演出を見せ終えたら、結果を書き込んでクリア画面へ移る
// シーンが作り直される前に、クリア画面で出す結果を書き込んでおく
void GameScene::UpdateStageClear(){
	const bool wasWaiting = stageClearTimer_ < kStageClearDuration;
	stageClearTimer_ += kDeltaTime;
	if(wasWaiting && stageClearTimer_ >= kStageClearDuration){
		WriteStageResult();
		sceneManager_->ChangeScene("CLEAR",GetTransitionStyle(kClearTransitionKey));
	}
}

// クリア画面で出す結果を書き込む
void GameScene::WriteStageResult(){
	StageResult::destroyedTargetCount = targetManager_->GetDestroyedCount();
	StageResult::totalTargetCount = targetManager_->GetTotalCount();
	StageResult::maxCombo = comboCounter_->GetMaxCombo();
}

// 最後のステージへ続く通路の壁が壊れた瞬間の演出
// 壁を消し、壁の中心から大きな破片を飛ばし、高く飛び散らせたしぶきは台に落ちたところに跡を残す
// 演出を見せている最中なら、一瞬動きを止めて画面を揺らし、壊れた手応えを見せる
void GameScene::PlayBarrierBreakEffects(int stageIndex,bool isShown){
	startPlatform_->ReleaseBarrierHold();
	const Vector3 barrierCenter = startPlatform_->GetBarrierCenter(stageIndex);
	inkEffect_->EmitGoalBreak(barrierCenter);
	inkEffect_->EmitLaunchBurst(barrierCenter,kBarrierBurstCount);
	if(isShown){
		screenImpact_->Trigger(kBarrierBreakHitStopDuration,kBarrierBreakShakeAmplitude,kBarrierBreakShakeDuration);
	}
}
