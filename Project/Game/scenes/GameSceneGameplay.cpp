#include "GameScene.h"
#include "GameSceneInternal.h"
#include "CameraManager.h"
#include "SceneManager.h"
#include "Input.h"
#include "Application.h"
#include "EditorContext.h"

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
#include "objects/Ground.h"
#include "objects/Player.h"
#include "objects/StartPlatform.h"
#include "objects/PlayerBulletManager.h"
#include "objects/TargetManager.h"
#include "objects/GoalTarget.h"
#include "objects/EnemyManager.h"
#include "objects/InkEffectManager.h"
#include "objects/ComboCounter.h"
#include "objects/ContinuePoint.h"
#include "objects/StageInfo.h"
// カメラ
#include "Camera/RailCamera.h"
#include "Camera/DebugTopCamera.h"
#include "Camera/ScreenImpact.h"
#include "Camera/CameraPose.h"
// UI
#include "UI/Reticle.h"
#include "UI/StageHUD.h"
#include "UI/StageClearUI.h"
#include "UI/StageLockedNotice.h"
#include "UI/TutorialHints.h"
#include "UI/RailSwitchPrompt.h"
#include "UI/KeyTargetMarker.h"
#include "UI/SpeedLines.h"
#include "UI/DamageInkEffect.h"
#include <algorithm>
#include <cmath>

// GameSceneのうち、プレイ中の毎フレームのゲーム処理(UpdateGameplay)と、飛び移り方の案内を出したときにゲームをゆっくりにする処理をまとめたファイル

using namespace GameSceneInternal;

namespace{
	// 的・敵を狙えていないときに、弾が画面中央のレティクルへ向かって飛ぶよう、カメラの前方のこの距離の点を狙って撃ち出す
	// (カメラはプレイヤーの後ろ上にあるため、カメラと同じ向きで平行に撃つとレティクルより下にずれる)
	// 弾の射程(PlayerBulletManager::kRange)より手前にして、弾が落ち始める前にこの点を通るようにする
	// (手元はカメラの前方およそ 3 にあるため、手元からはおよそ 9 先の点になる)
	constexpr float kAimConvergeDistance = 12.0f;
	// 弾を向ける的・敵は、手元からカメラの前方へこの距離より先にあるものに限る(すぐ近くや後ろのものへ向けて、弾が横や後ろへ飛ばないようにする)
	constexpr float kMinAimAheadOfMuzzle = 1.0f;
	// レティクルの中心を赤くするかを決めるときは、カメラより前にあるものをすべて狙えているものとして扱う
	constexpr float kReticleAimMinDistance = 0.0f;
}

// プレイ中のゲーム処理
void GameScene::UpdateGameplay(){
	// EditモードではゲームのシミュレーションをUnityのように止める(Playを押して初めて動く)。
	// カメラ位置や的の配置反映などの「表示」は両モードで行い、進行/入力/射撃だけをPlay限定にする。
	const bool isPlayMode = EditorContext::GetInstance()->IsPlayMode();

	// Playに入った瞬間、ゲームを初期状態から開始する(UnityのPlayと同じく毎回リセットして始まる)。
	// これでPlayを押すとレール先頭=編集で見えていた画から始まる。
	// 開始演出もタイトルから始め、Editモードへ戻ったときは止める。
	// 落下リスタートでは ResetPlayState() だけを呼ぶため、タイトルや出発ゲートを挟まずにスタート台の上からすぐ再開する
	if(isPlayMode && !wasPlayMode_){
		// Editモード中は俯瞰デバッグカメラを使っているため、Playに入ったらプレイ用カメラに切り替える
		debugTopCamera_->Disable();
		// Playに入ったらカーソルを隠してウィンドウ内に閉じ込め、照準操作中にウィンドウ外をクリックしないようにする(TABで再表示できる)
		if(isCursorVisible_){
			isCursorVisible_ = false;
			::ShowCursor(FALSE);
		}
		// Rail Editorで編集していたレールと制御点を覚えておく(ResetPlayState()でレールを0番へ戻すと選択が消えるため、その前に取る)
		editRailIndex_ = railEditor_->GetActiveRailIndex();
		editPointIndex_ = railEditor_->GetSelectedPointIndex();
		// 制御点を選んでいないときは、設定がONでもいつも通り始める
		isPlayingFromPoint_ = startsFromSelectedPoint_ && editPointIndex_ >= 0;
		// Playに入り直したら、前回のPlayで通ったチェックポイントは忘れて最初から始める
		// 残機も満タンから始める(コンティニューで戻ってきたときも満タンにする)
		checkpoint_ = Checkpoint{};
		lives_ = kMaxLives;
		// タイトルから新しく始めるときは、クリアしたステージも忘れ、操作の案内も最初から出す(コンティニューのときは残す)
		if(!ContinuePoint::isRequested){
			StageInfo::ResetProgress();
			tutorialHints_->Reset();
			hasPlayedRailSwitchSlow_ = false;
			hasDestroyedFirstKeyTarget_ = false;
		}
		ResetPlayState();
		// ゲームオーバー画面でコンティニューを選んで戻ってきたときは、最後に通ったチェックポイントから始める
		if(ContinuePoint::isRequested){
			StartFromContinuePoint();
		} else{
			StartPlaySequence();
		}
	} else if(!isPlayMode && wasPlayMode_){
		startSequence_->Stop();
		departureDirector_->Reset();
		returnJump_->Reset();
		barrierCutscene_->Reset();
		startPlatform_->ReleaseBarrierHold();
		isPlaySessionClearPending_ = false;
		// Play中に現れたゴールの的が、Editモードで出たままにならないよう消す
		goalTarget_->Reset();
		// Editモードに戻ったら、Play前に見ていた位置のまま俯瞰デバッグカメラに戻す
		debugTopCamera_->Enable();
		// Play前に編集していたレールと制御点を選び直し、続けて編集できるようにする
		railEditor_->SelectPoint(editRailIndex_,editPointIndex_);
	} else if(isPlayMode && input_ && input_->TriggerKey(DIK_R)){
		// 確認用の一時的な処理:Play中にRキーでタイトルから最初からやり直す(選んだ制御点から始めているときは、その制御点からやり直す)
		// 最初からやり直すため、通ったチェックポイントも忘れ、残機も満タンに戻す
		checkpoint_ = Checkpoint{};
		lives_ = kMaxLives;
		StageInfo::ResetProgress();
		tutorialHints_->Reset();
		hasPlayedRailSwitchSlow_ = false;
		hasDestroyedFirstKeyTarget_ = false;
		ResetPlayState();
		StartPlaySequence();
	}
	wasPlayMode_ = isPlayMode;

	// 開始演出の更新(タイトル中のSPACE入力・カメラの回り込み・出発ゲートの前でのSPACE入力)
	// ゲートの前にいるかは、前フレームで動かしたプレイヤーの座標で判定する
	if(isPlayMode){
		startSequence_->Update(kDeltaTime,player_->GetPosition());

		// 出発ゲートを開け始めたら、そのゲートの先のステージに切り替える(ゲートの番号とステージの番号は同じ)
		// 出発の演出はステージのレールを使うため、演出を始めるより先に切り替える
		if(startSequence_->IsGateOpenTriggered()){
			SwitchStage(startPlatform_->GetSelectedGateIndex());
			// 台の上での基準の向きを、そのステージのレールの始点の向きに付け替える
			// 照準オフセットは見えている向きを変えずに0へ戻し、どこを向いていてもカウントダウンの間にレールの正面へ回るようにする
			const Vector3 stageStartRotation = railEditor_->GetRotationOnRail(0.0f);
			player_->SetPlatformBaseRotation(stageStartRotation);
			railCamera_->RecenterAim();
		}

		// ゲートのカウントダウンに合わせて飛び乗る演出は、ゲートを開け始めた瞬間に選んで始める
		// ゲートを映すカメラの代わりに演出のカメラで映すため、始めた瞬間に映っているカメラの画から動かし始める
		if(startSequence_->IsGateOpenTriggered() && departureDirector_->StartsWithGate(GetDepartureStyle())){
			const DepartureContext context = MakeDepartureContext();
			departureDirector_->Prepare(GetDepartureStyle(),context);
			startSequence_->BeginDeparture();
			CameraPose startCamera = {railCamera_->GetPosition(), railCamera_->GetRotation()};
			if(Camera* mainCamera = CameraManager::GetInstance()->GetCamera(kDefaultCameraName)){
				startCamera = {mainCamera->GetTranslate(), mainCamera->GetRotate()};
			}
			departureDirector_->Begin(context,startCamera);
		}
	}

	// レール進行・操作・射撃・勝敗判定は、開始演出が終わってプレイ可能になってから動かす
	// 暗転中(クリア・ゲームオーバーへの切り替え中、落下リスタート中)も、入力や判定が二重に起きないよう止めておく
	// やられ演出の最中も、自機は弾けて消えているため止めておく
	// ゴールの的を壊してスタート台へ大ジャンプで戻っている最中も、自機を演出側で動かすため止めておく
	// スタート台に着地してクリア画面へ移るのを待っている間も、ステージは終わっているため止めておく
	const bool isGameplayActive = isPlayMode && startSequence_->IsPlayable() && !sceneManager_->IsTransitioning() && !isDefeated_ && !returnJump_->IsRunning() && !isStageCleared_;
	// 開始演出のうち、スタート台の上を自由に動ける段階か(出発ゲートの前で準備中と、ゲートが開いてレールへ乗り込むまで)
	// 動き方(WASD・ジャンプ・台から落ちたときのリスタート)はプレイ中と同じで、敵・射撃・勝敗判定はまだ動かさない
	// 通路の壁を壊して見せている間は動かさない(演出を飛ばした SPACE で、そのフレームにジャンプしないよう、演出を進める前に決めておく)
	const bool isPlayerMovable = isPlayMode && startSequence_->IsPlayerMovable() && !sceneManager_->IsTransitioning() && !barrierCutscene_->IsRunning();

	// 最初に飛び移り方の案内を出したときだけ、レールを進む・敵・弾・的・しぶきをゆっくりにする
	// カメラ(照準)とキーの入力は通常の速さのままにして、落ち着いて案内のキーを押せるようにする
	UpdateRailSwitchSlow(isGameplayActive);
	const float worldDeltaTime = kDeltaTime * railSwitchTimeScale_;

	// 出発の演出を使うときは、ゲートが開いてから演出を始めるまでの間だけ、ゲートの先に出発台の目印を出す
	startPlatform_->SetDeparturePadVisible(startSequence_->IsBoarding() && departureDirector_->UsesDeparturePad());

	// マウスカーソルの表示/非表示切り替え
	UpdateCursorVisibility(isPlayMode);

	// レール進行度を時間で進める
	player_->UpdateRailProgress(railEditor_.get(),isGameplayActive,worldDeltaTime);

	// オンレール/オフレールの状態に応じて、カメラ・プレイヤーの基準位置と基準向きを求める
	player_->UpdateBasePose(railEditor_.get());

	// レールを進んでいる間は、通った区間をインクの色で塗る
	// 開始演出中・Edit中・終点に着いた後は進まないため、レールを進んでいる間に含めない
	const bool isRidingRail = isGameplayActive && player_->IsOnRail() && !player_->HasReachedGoal();
	if(isRidingRail){
		railEditor_->PaintActiveRail(player_->GetRailT());
		// チェックポイントの制御点を通ったら、落ちたときにやり直す位置として覚えておく
		UpdateCheckpoint();
	}

	// カメラを基準位置の後ろ上に置き、照準の入力を反映する
	// スタート台の上を動ける間も照準(カメラの向き)を動かせるようにして、周りを見回せるようにする
	railCamera_->Update(player_->GetBasePosition(),player_->GetBaseRotation(),isGameplayActive || isPlayerMovable,input_,kDeltaTime,ground_->GetHeight(),startPlatform_.get());
	const Vector3& cameraPos = railCamera_->GetPosition();
	const Vector3& cameraForward = railCamera_->GetForward();

	// Editモードで始まったときは、Playの最初と同じプレイ用カメラの画から俯瞰デバッグカメラを動かし始める
	// Playから戻ったときは Enable() 済みのため、Play前に見ていた画のまま編集を続けられる
	if(!isPlayMode && !debugTopCamera_->IsEnabled()){
		debugTopCamera_->SetPose({cameraPos, railCamera_->GetRotation()});
		debugTopCamera_->Enable();
	}

#ifdef USE_IMGUI
	// レールの状態確認用デバッグ表示(Playモード中も含めて常に表示する)
	ShowStatusWindow();
#endif

	// 復活したときに透明な体が元に戻っていく演出を進める
	// 暗転で画面を覆っている間は進めず、画面が見えてから元に戻っていくようにする
	if(!sceneManager_->IsTransitioning()){
		player_->UpdateReveal(kDeltaTime);
	}

	// プレイヤーをレールの上に立たせ、照準の方向へ向ける
	// 座標は雑魚敵の検知判定・開始演出にも使う
	player_->UpdateTransform(railCamera_->GetRotation(),startSequence_->GetPlayerHopHeight());
	const Vector3& playerPos = player_->GetPosition();

	// レールを進んでいる間は、足元から後ろへインクのしぶきを飛ばして勢いを見せる
	// 跳ねている間・並走レールへ飛び移っている間は足がレールから離れているため出さない
	if(isRidingRail && player_->IsGroundedOnRail()){
		inkEffect_->EmitRailTrail(playerPos,-railEditor_->GetForwardOnRail(player_->GetRailT()));
	}

	// 出発の演出(スーパージャンプ・インクに潜る・発射台など)を進める
	// プレイヤーの表示とカメラを演出側で動かすため、プレイヤーの行列更新の後・カメラの上書きの前に行う
	// レールへ乗せ終えたら、歩いて乗り込んだときと同じく「GO!」と乗り込みの演出でプレイを始める
	if(isPlayMode && departureDirector_->IsRunning()){
		departureDirector_->Update(screenImpact_->IsHitStopping()?0.0f:kDeltaTime,{cameraPos, railCamera_->GetRotation()});
		Vector3 boardedPos;
		if(departureDirector_->ConsumeBoarded(boardedPos)){
			startSequence_->NotifyBoarded();
			PlayBoardedEffects(boardedPos);
		}
	}

	// ゴールの的を壊したあとの、スタート台へ戻る大ジャンプを進める
	// 出発の演出と同じく、プレイヤーの行列更新の後・カメラの上書きの前に行う
	if(isPlayMode && returnJump_->IsRunning()){
		returnJump_->Update(screenImpact_->IsHitStopping()?0.0f:kDeltaTime,{cameraPos, railCamera_->GetRotation()});
		// 打ち上がった瞬間に、プレイヤーをスタート台の上へ移し、照準を正面に戻しておく
		// プレイ用カメラが着地後の画(スタート台の上の画)へ動き始め、演出のカメラを最後にその画へ戻せる(表示は演出側で上書きする)
		if(returnJump_->ConsumeLaunched()){
			railEditor_->SwitchActiveRail(0);
			player_->Reset();
			player_->StartOnPlatform(startPlatform_->GetSpawnPosition(),railEditor_->GetRotationOnRail(0.0f));
			railCamera_->Reset();
			// 撃破演出を見せ終えてスタート台へ跳ぶときに「STAGE CLEAR!」を押しつけるように出し、ステージをクリアしたことをはっきり伝える
			stageClearUI_->Begin();
		}
		Vector3 landedPos;
		// スタート台へ着地したら、今のステージをクリアしたことにする
		// 最後のステージなら着地の演出を出してクリア画面へ移るのを待ち始め、それ以外はスタート台の上から次のステージを選べるようにする
		if(returnJump_->ConsumeLanded(landedPos)){
			StageInfo::clearedStages[railEditor_->GetStageIndex()] = true;
			if(railEditor_->GetStageIndex() == StageInfo::kFinalStageIndex){
				StartStageClear(landedPos);
			} else{
				// プレイ会用の設定では、壁が壊れるのを見せた後にクリア画面へ移り、次の人がタイトルから遊べるようにする
				// 始め直すと的・連鎖数が初期状態に戻るため、結果は先に書き込んでおく
				if(StageInfo::isPlaySessionMode){
					WriteStageResult();
				}
				RestartFromReturnJump(landedPos);
				isPlaySessionClearPending_ = StageInfo::isPlaySessionMode;
				// クリアしたステージの通路の壁は、演出で壊れるまで立たせておき、カメラを向けて壊れるところを見せる
				startPlatform_->HoldBarrier(railEditor_->GetStageIndex());
				barrierCutscene_->Begin(*startPlatform_,railEditor_->GetStageIndex());
			}
		}
	}

	// 着地した後の、クリアしたステージの通路の壁を壊して見せる演出を進める(SPACE で飛ばせる)
	// 大ジャンプと同じく、プレイヤーの行列更新の後・カメラの上書きの前に行う
	if(isPlayMode && barrierCutscene_->IsRunning()){
		barrierCutscene_->Update(screenImpact_->IsHitStopping()?0.0f:kDeltaTime,input_ && input_->TriggerKey(DIK_SPACE));
		if(barrierCutscene_->ConsumeBroken()){
			PlayBarrierBreakEffects(barrierCutscene_->GetStageIndex(),barrierCutscene_->IsRunning());
		}
	}

	// プレイ会用の設定では、壁が壊れるのを見せ終えたらクリア画面へ移る
	if(isPlayMode && isPlaySessionClearPending_ && !barrierCutscene_->IsRunning() && !sceneManager_->IsTransitioning()){
		isPlaySessionClearPending_ = false;
		sceneManager_->ChangeScene("CLEAR",GetTransitionStyle(kClearTransitionKey));
	}

	// 着地の演出を見せ終えたら、クリア画面へ移る(Editモードへ戻ったときは止める)
	if(isPlayMode && isStageCleared_){
		UpdateStageClear();
	}

	// 開始演出中は、プレイ用カメラをプレイヤー中心に回転させた位置・向きでカメラを上書きする
	// (カメラの行列はシーン更新の後にまとめて更新されるため、ここで上書きしてもこのフレームの描画に反映される)
	// タイトルロゴも同じくプレイヤーの位置・向きを基準に配置する
	CameraPose departureCamera;
	CameraPose returnJumpCamera;
	CameraPose barrierCamera;
	if(isPlayMode && startSequence_->IsControllingCamera()){
		startSequence_->UpdateLogo(playerPos,railCamera_->GetRotation());

		Vector3 directedCameraPos;
		Vector3 directedCameraRot;
		startSequence_->CalculateCamera(playerPos,cameraPos,railCamera_->GetRotation(),directedCameraPos,directedCameraRot);
		if(Camera* mainCamera = CameraManager::GetInstance()->GetCamera(kDefaultCameraName)){
			mainCamera->SetTranslate(directedCameraPos);
			mainCamera->SetRotate(directedCameraRot);
		}
	} else if(isPlayMode && departureDirector_->GetCameraPose(departureCamera)){
		// 出発の演出中は、演出が決めた画にする(ゲートを映すカメラが戻りきる前に始めても、映っていた画から動かし始める)
		if(Camera* mainCamera = CameraManager::GetInstance()->GetCamera(kDefaultCameraName)){
			mainCamera->SetTranslate(departureCamera.position);
			mainCamera->SetRotate(departureCamera.rotate);
		}
	} else if(isPlayMode && returnJump_->GetCameraPose(returnJumpCamera)){
		// スタート台へ戻る大ジャンプの最中は、演出が決めた画にする(跳んでいるプレイヤーを追いかけ、最後にスタート台の上の画へ戻す)
		if(Camera* mainCamera = CameraManager::GetInstance()->GetCamera(kDefaultCameraName)){
			mainCamera->SetTranslate(returnJumpCamera.position);
			mainCamera->SetRotate(returnJumpCamera.rotate);
		}
	} else if(isPlayMode && barrierCutscene_->GetCameraPose({cameraPos, railCamera_->GetRotation()},barrierCamera)){
		// 通路の壁を壊して見せている間は、プレイ用カメラから壁を映す画へ回し、壊れたらプレイ用カメラへ戻す
		if(Camera* mainCamera = CameraManager::GetInstance()->GetCamera(kDefaultCameraName)){
			mainCamera->SetTranslate(barrierCamera.position);
			mainCamera->SetRotate(barrierCamera.rotate);
		}
	} else if(isPlayMode && startSequence_->IsGateCameraActive()){
		// 出発ゲートを開けている間は、ゲートの斜め前からの画へ寄せ、開ききったらプレイ用カメラへ戻す
		const CameraPose directedCamera = startSequence_->CalculateGateCamera({cameraPos, railCamera_->GetRotation()});
		if(Camera* mainCamera = CameraManager::GetInstance()->GetCamera(kDefaultCameraName)){
			mainCamera->SetTranslate(directedCamera.position);
			mainCamera->SetRotate(directedCamera.rotate);
		}
	}

	// 画面の揺れを、このフレームのカメラの位置に足す
	// 演出カメラで上書きした後に足すことで、どの画でも同じように揺れるようにする
	// (カメラの位置はレール追従カメラか演出カメラが毎フレーム設定し直すため、揺れが積み重なることはない)
	if(screenImpact_->IsShaking()){
		if(Camera* mainCamera = CameraManager::GetInstance()->GetCamera(kDefaultCameraName)){
			mainCamera->SetTranslate(mainCamera->GetTranslate() + screenImpact_->GetShakeOffset(railCamera_->GetRight(),kWorldUp));
		}
	}

	// 雑魚敵の更新
	// Edit中・開始演出中はゲームを静止させるため、経過時間を0にして表示更新のみ行わせる
	// やられ演出の最中は、周りがスローで動き続けるように見せる
	float enemyDeltaTime = 0.0f;
	if(isGameplayActive){
		enemyDeltaTime = worldDeltaTime;
	} else if(isDefeated_){
		enemyDeltaTime = kDeltaTime * kDefeatTimeScale;
	}
	enemyManager_->Update(playerPos,enemyDeltaTime);

	// 敵弾とプレイヤーの当たり判定
	// プレイ可能な間のみ判定する。連続被弾で一瞬に体力が尽きないよう、被弾後は一定時間無敵にする
	if(isGameplayActive){
		// 無敵時間の経過を進める
		player_->UpdateInvincible(kDeltaTime);

		// 命中した弾は無敵中でもここで消滅させ、すり抜けて後から当たらないようにする
		// プレイヤーの行列はこのフレームの配置処理でUpdate()済み
		int hitCount = enemyManager_->CheckHitToPlayer(player_->GetHitSphere());

		// 同時に複数当たっても体力の減少は1回分だけにする
		if(hitCount > 0){
			player_->TakeDamage();
		}
	}

	// やられ演出を進め、終わったらゲームオーバー画面へ移る(Editモードへ戻ったときは止める)
	if(isPlayMode && isDefeated_){
		UpdateDefeat();
	}

	// 体力切れの判定
	// 体力が0になったら残機を1つ減らしてやり直す(残機が尽きたら、やられ演出のあとゲームオーバー画面へ遷移する)
	// クリア判定と同じく、以降の落下リスタートなどの処理を行わないようここで抜ける
	// やり直しの暗転中はプレイ可能でなくなるため、残機が二重に減ることはない
	if(isGameplayActive && player_->IsDead()){
		LoseLife();
		return;
	}

	// プレイヤーの自立(ジャンプ+WASD移動)
	// どのレールにも乗れないまま地面の高さまで落ちたら、残機を1つ減らしてやり直す(残機が尽きたら、やられ演出のあとゲームオーバー画面へ遷移する)
	// 開始演出でスタート台の上を動ける間も、同じ動き方で動かす
	// 出発前にスタート台から落ちたときは、まだプレイを始めていないため残機を減らさずにやり直す
	if(isGameplayActive || isPlayerMovable){
		bool needsRestart = player_->UpdateMovement(input_,railEditor_.get(),startPlatform_.get(),cameraForward,railCamera_->GetRight(),ground_->GetHeight(),worldDeltaTime);
		if(needsRestart){
			if(isGameplayActive){
				LoseLife();
			} else{
				RestartAfterMiss();
			}
		}

		// 出発の演出を使うときは、ゲートの先の出発台の手前の線を越えたら演出を始める
		// (線は歩いてレールに乗れる範囲より手前にあるため、歩いて乗り込むより先に必ず越える)
		// 始めた瞬間に映っているカメラの画から、演出のカメラへ動かし始める
		if(startSequence_->IsBoarding() && departureDirector_->UsesDeparturePad() && startPlatform_->IsPastDepartureLine(player_->GetPosition())){
			startSequence_->BeginDeparture();
			CameraPose startCamera = {cameraPos, railCamera_->GetRotation()};
			if(Camera* mainCamera = CameraManager::GetInstance()->GetCamera(kDefaultCameraName)){
				startCamera = {mainCamera->GetTranslate(), mainCamera->GetRotate()};
			}
			departureDirector_->Begin(MakeDepartureContext(),startCamera);
		}

		// 出発ゲートをくぐってレールに乗り込んだ瞬間に、乗り込みの演出と「GO!」でプレイを始める
		if(startSequence_->IsBoarding() && player_->IsOnRail()){
			const Vector3 boardedPos = railEditor_->GetPositionOnRail(player_->GetRailT());
			startSequence_->NotifyBoarded();
			PlayBoardedEffects(boardedPos);
		}
	}

	// 弾の発射処理(左クリックを押している間、決まった間隔で撃ち続ける。押した瞬間はすぐに1発目を撃つ)
	// スタート台の上を動ける間も試し撃ちとして撃てるようにし、台に当たったところを塗れるようにする
	// 試し撃ちの弾は的・敵には当たらないようにして、プレイが始まる前に的を壊したり敵を倒したりしないようにする
	fireCooldown_ = (std::max)(fireCooldown_ - kDeltaTime,0.0f);
	bool shootTriggered = (isGameplayActive || isPlayerMovable) && input_ && input_->PushMouseButton(kShootMouseButton) && fireCooldown_ <= kFireIntervalTolerance;
	if(shootTriggered){
		fireCooldown_ = kFireInterval;

		// プレイヤーの手元から、レティクルの線(カメラの前方へ伸ばした線)の上の点へ向けて撃ち出す
		// 手元とカメラの位置がずれているため、決まった距離の点を狙うだけだと、それより手前や奥の的ではレティクルとずれる
		// そこで、的・敵をレティクルで狙えているときは、レティクルの線の上の、一番手前のものと同じ奥行きの点を狙う
		// (的・敵の中心そのものを狙うと、レティクルが少し外れているときに弾がそちらへ曲がり、レティクルの通りに飛ばなくなる)
		// ただし手元より手前(カメラとプレイヤーの間)にあるものを狙うと弾が後ろへ飛ぶため、手元より少し先にあるものだけを狙う
		Vector3 muzzlePos = player_->GetMuzzlePosition();
		float aimDistance = kAimConvergeDistance;
		const float minAimDistance = Dot(muzzlePos - cameraPos,cameraForward) + kMinAimAheadOfMuzzle;
		Vector3 aimedTargetPos;
		float aimedTargetDistance = 0.0f;
		const bool isAimingTarget = targetManager_->FindAimedTarget(cameraPos,cameraForward,minAimDistance,aimedTargetPos,aimedTargetDistance);
		// 的・敵・ゴールの的のどれかを狙えているか(狙えているもののうち、一番手前のものの奥行きを狙う)
		bool isAimingAny = isAimingTarget;
		if(isAimingTarget){
			aimDistance = aimedTargetDistance;
		}
		Vector3 aimedEnemyPos;
		float aimedEnemyDistance = 0.0f;
		if(enemyManager_->FindAimedEnemy(cameraPos,cameraForward,minAimDistance,aimedEnemyPos,aimedEnemyDistance) && (!isAimingAny || aimedEnemyDistance < aimDistance)){
			aimDistance = aimedEnemyDistance;
			isAimingAny = true;
		}
		// ゴールの的は左右・上下に揺れながら並走するため、狙えているときは同じように奥行きを合わせて当てやすくする
		float aimedGoalDistance = 0.0f;
		if(goalTarget_->FindAimed(cameraPos,cameraForward,minAimDistance,aimedGoalDistance) && (!isAimingAny || aimedGoalDistance < aimDistance)){
			aimDistance = aimedGoalDistance;
		}
		Vector3 aimPoint = cameraPos + cameraForward * aimDistance;
		Vector3 shootDirection = Normalize(aimPoint - muzzlePos);
		bulletManager_->Fire(muzzlePos,shootDirection,isGameplayActive);

		// 手元から撃つ向きへインクを少し飛び散らせる
		inkEffect_->EmitInkSplash(muzzlePos,shootDirection,kMuzzleSplashCount);
	}

	// 弾の移動更新と生存時間チェック
	bulletManager_->Move(worldDeltaTime);

	// 連鎖が途切れるまでの時間を進める(Edit中・開始演出中は止めておく)
	comboCounter_->Update(isGameplayActive?worldDeltaTime:0.0f);

	// 的の照準判定と弾との当たり判定
	bool isAimingAtAnyTarget = targetManager_->Update(cameraPos,cameraForward,*bulletManager_,worldDeltaTime);

	// 壊れた的ごとに、破片・しぶき・閃光を出して連鎖数を増やす
	for(const Vector3& destroyedPos : targetManager_->GetDestroyedPositions()){
		inkEffect_->EmitTargetBreak(destroyedPos);
		comboCounter_->AddHit();
	}

	// ゴールの的の並走と弾との当たり判定
	// プレイヤーがゴールのレールに乗っている間だけ、プレイヤーより先を並走させる(Edit中・開始演出中は出さない)
	const bool isOnGoalRail = isGameplayActive && player_->IsOnRail() && railEditor_->IsGoalRail(railEditor_->GetActiveRailIndex());
	goalTarget_->Update(railEditor_.get(),player_->GetRailT(),isOnGoalRail,cameraPos,*bulletManager_,isGameplayActive?worldDeltaTime:0.0f);
	// ゴールの的が現れた瞬間に「撃ちまくれ!!!」を出し、体力の多い的を撃ち続ければよいと伝える
	if(goalTarget_->IsAppeared() && !wasGoalTargetAppeared_){
		goalBannerUI_->Begin();
	}
	wasGoalTargetAppeared_ = goalTarget_->IsAppeared();

	// クリア判定
	// ゴールの的の体力を0にして壊したらステージクリアとし、撃破演出を見せてから大ジャンプでスタート台へ戻る
	// スタート台へ戻る大ジャンプの最中はプレイ可能でなくなるため、クリアの処理が二重に起きることはない
	if(isGameplayActive && goalTarget_->IsDestroyed()){
		// 普通の的より大きな破片・多くのしぶき・大きな閃光を出し、連鎖数を増やす
		const Vector3 goalBreakPosition = goalTarget_->GetPosition();
		inkEffect_->EmitGoalBreak(goalBreakPosition);
		comboCounter_->AddHit();
		// 一瞬動きを止めて画面を大きく揺らし、的を中心に放射ブラーをかけて、倒した手応えを見せる
		screenImpact_->Trigger(kGoalBreakHitStopDuration,kGoalBreakShakeAmplitude,kGoalBreakShakeDuration);
		if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
			Application::GetInstance()->TriggerRadialBlur(CameraPoseMath::WorldToScreenUv(goalBreakPosition,activeCamera->GetViewProjectionMatrix()),kGoalBreakRadialBlurWidth,kGoalBreakRadialBlurDuration);
		}
		// 壊れた的の破片を少し見せてから、大ジャンプでスタート台へ戻る
		returnJump_->Begin(MakeDepartureContext());
	}

	// 鍵の的を壊したら、そのレールを出す(始点から終点へ管を伸ばす)
	ApplyRailLocks(false);

	// 雑魚敵への被弾判定
	enemyManager_->CheckHitByBullets(*bulletManager_);

	// 飛んでいる弾の描画用の行列を更新する(命中・生存時間切れの弾は未使用に戻り、次の発射で使い回す)
	bulletManager_->UpdateTransforms();

	// 狙えているときはレティクル中心を赤くする(ゴールの的は、レティクルの方向が当たり判定の球を通っていれば狙えているとする)
	float goalAimDistance = 0.0f;
	reticle_->SetAiming(isAimingAtAnyTarget || goalTarget_->FindAimed(cameraPos,cameraForward,kReticleAimMinDistance,goalAimDistance));

	// 演出の更新(このフレームで出した分も含めて行列を求めるため、出す処理がすべて終わってから行う)
	// ヒットストップ中は、飛び散ったインクも空中で止める
	// やられ演出の最中は、弾けたインクがゆっくり飛び散っていくように見せる
	// ゴールの的を壊してから跳ぶまでの間も、壊れた的の破片がゆっくり飛び散っていくように見せる
	float inkTimeScale = 1.0f;
	if(isDefeated_){
		inkTimeScale = kDefeatTimeScale;
	} else if(returnJump_->IsWaiting()){
		inkTimeScale = kGoalBreakTimeScale;
	}
	inkEffect_->Update(screenImpact_->IsHitStopping()?0.0f:worldDeltaTime * inkTimeScale);

	// スピード線はレールを進んでいる間だけ出す
	speedLines_->Update(isRidingRail,kDeltaTime);

	// 体力の表現の更新(Play中だけ出し、Editモードでは外す)
	damageInkEffect_->Update(player_->GetHp(),player_->GetMaxHp(),isPlayMode,kDeltaTime);

	// 画面表示の更新(左上のアイコンには残機を出す)
	stageHUD_->Update(lives_,kMaxLives,targetManager_->GetDestroyedCount(),targetManager_->GetTotalCount(),*comboCounter_);
	// 「STAGE CLEAR!」の表示の動きを進める(出していないときは何もしない)
	stageClearUI_->Update(kDeltaTime);
	goalBannerUI_->Update(kDeltaTime);
	// プレイ会用の設定で閉じているゲートの前に、台の上を歩いて来たら「いまは遊べないよ!!」を出す
	stageLockedNotice_->Update(isPlayerMovable && startPlatform_->IsInFrontOfPlaySessionClosedGate(player_->GetPosition()),kDeltaTime);
	// レールを進んでいる間に、生きている鍵の的が近くにあるかを調べる(鍵の的の目印と案内に使う)
	Vector3 nearestKeyTargetPos{};
	const bool isKeyTargetNear = isGameplayActive && player_->IsOnRail() && targetManager_->FindNearestKeyTarget(playerPos,kKeyTargetGuideDistance,nearestKeyTargetPos);
	if(targetManager_->HasDestroyedKeyTarget()){
		hasDestroyedFirstKeyTarget_ = true;
	}
	// 最初の鍵の的を壊すまでは、近くの鍵の的の上に「▼」を出して気づいてもらう
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		keyTargetMarker_->Update(isKeyTargetNear && !hasDestroyedFirstKeyTarget_,nearestKeyTargetPos,cameraPos,cameraForward,activeCamera->GetViewProjectionMatrix(),kDeltaTime);
	}

	// 操作の案内を、台の上・レールの上での操作に合わせて進める
	TutorialHints::Situation tutorialSituation;
	tutorialSituation.isOnPlatform = isPlayerMovable;
	tutorialSituation.isRiding = isGameplayActive && player_->IsOnRail();
	tutorialSituation.playerPosition = player_->GetPosition();
	tutorialSituation.hasFired = shootTriggered;
	tutorialSituation.hasOpenedGate = startSequence_->IsGateOpenTriggered();
	tutorialSituation.hasDestroyedTarget = !targetManager_->GetDestroyedPositions().empty();
	tutorialSituation.isKeyTargetNear = isKeyTargetNear;
	tutorialSituation.hasDestroyedKeyTarget = targetManager_->HasDestroyedKeyTarget();
	tutorialHints_->Update(tutorialSituation,kDeltaTime);

	// 鍵の的を壊して出た、まだ乗っていないレールへ飛び移れるときは、そのレールがある側に押すキーを出す
	// 乗っているレールは乗ったことにし、一度乗った鍵のレールには案内を出さない(戻る向きに出さないようにする)
	const int activeRailIndex = railEditor_->GetActiveRailIndex();
	if(isGameplayActive && player_->IsOnRail() && activeRailIndex >= 0 && activeRailIndex < static_cast<int>(riddenRails_.size())){
		riddenRails_[activeRailIndex] = true;
	}
	int railSwitchSide = 0;
	if(isGameplayActive && player_->IsGroundedOnRail()){
		int sideRailIndex = -1;
		const int side = player_->FindSideRailDirection(railEditor_.get(),railCamera_->GetRight(),sideRailIndex);
		const bool isRidden = sideRailIndex >= 0 && sideRailIndex < static_cast<int>(riddenRails_.size()) && riddenRails_[sideRailIndex];
		if(side != 0 && targetManager_->IsKeyRail(sideRailIndex) && !isRidden){
			railSwitchSide = side;
		}
	}
	railSwitchPrompt_->Update(railSwitchSide,kDeltaTime);
	lastRailSwitchSide_ = railSwitchSide;
}

// 最初に飛び移り方の案内を出したときだけ、ゲームをゆっくりにする
// 速さは目標の速さへ指数的に近づけ、急に止まったように見えないようにする(実際の時間で変える)
void GameScene::UpdateRailSwitchSlow(bool isGameplayActive){
	if(!isGameplayActive){
		isRailSwitchSlowActive_ = false;
	} else if(!hasPlayedRailSwitchSlow_ && lastRailSwitchSide_ != 0){
		hasPlayedRailSwitchSlow_ = true;
		isRailSwitchSlowActive_ = true;
		railSwitchSlowTimer_ = 0.0f;
	} else if(isRailSwitchSlowActive_){
		railSwitchSlowTimer_ += kDeltaTime;
		// 飛び移った(案内が消えた)か、飛び移れなくなったか、最大の時間がたったら戻す
		if(lastRailSwitchSide_ == 0 || railSwitchSlowTimer_ >= kRailSwitchSlowMaxDuration){
			isRailSwitchSlowActive_ = false;
		}
	}

	const float targetScale = isRailSwitchSlowActive_?kRailSwitchSlowTimeScale:1.0f;
	const float changeRate = 1.0f - std::exp(-kRailSwitchSlowSharpness * kDeltaTime);
	railSwitchTimeScale_ += (targetScale - railSwitchTimeScale_) * changeRate;
}
