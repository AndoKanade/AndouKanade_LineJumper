#include "GameScene.h"
#include "GameSceneInternal.h"
#include "CameraManager.h"
#include "ParticleManager.h"
#include "TextureManager.h"
#include "Skybox.h"
#include "SkyboxCommon.h"
#include "Input.h"
#include "Obj3dCommon.h"
#include "SpriteCommon.h"
#include "WinAPI.h"
#include "Application.h"
#include "GlobalVariables.h"
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
#include "objects/Sun.h"
#include "objects/Player.h"
#include "objects/StartPlatform.h"
#include "objects/PlayerBulletManager.h"
#include "objects/TargetManager.h"
#include "objects/GoalTarget.h"
#include "objects/EnemyManager.h"
#include "objects/InkEffectManager.h"
#include "objects/ComboCounter.h"
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
#include "UI/HelpOverlay.h"
#include <algorithm>
#include <memory>
#include <string>

// GameSceneのうち、シーンの骨組み(初期化・終了・毎フレームの更新の入口・カーソルの表示・遠くの物の溶け込み・描画)をまとめたファイル
// プレイ中のゲーム処理はGameSceneGameplay.cpp、始め直し・チェックポイント・ステージの切り替え・クリアの流れはGameSceneFlow.cpp、
// ImGuiのデバッグ用の画面はGameSceneDebugUI.cppにある

using namespace GameSceneInternal;

namespace{
	// スカイボックスのテクスチャパス
	// 天球(霧で白んだ水色の空と遠景の建造物)の画像。tools/generate_celestial_sphere.py で生成する
	const std::string kSkyboxTexture = "resource/Skybox/celestialSphere.dds";

	// プレイ用カメラの初期位置
	constexpr Vector3 kDefaultCameraPosition = {0.0f, 0.0f, -30.0f};

	// 撃破演出(パーティクル)に使用するテクスチャパス
	const std::string kHitParticleTexture = "resource/circle.png";
	// ParticleManager::EmitSpark()が内部で使用するグループ名と合わせる必要がある
	const char* kHitParticleGroupName = "Spark";

	// プレイ会用の設定にするかの調整項目名と初期値(ON にすると、ステージ1だけを遊べ、クリアしたらクリア画面へ移る)
	const char* kPlaySessionModeKey = "playSessionMode";
	constexpr bool kDefaultPlaySessionMode = false;
	// Play中に、遠くのレール・スタート台・的・敵などを空の色に溶け込ませ始める距離・溶け込みきる距離の調整項目名と初期値(カメラからの距離)
	// レールは進む先や飛び移れるレールを見せる道しるべで、敵は25以内でプレイヤーに気づくため、地面(45〜95)より遠くから溶け込ませる
	const char* kObjectFadeStartKey = "objectFadeStart";
	const char* kObjectFadeEndKey = "objectFadeEnd";
	constexpr float kDefaultObjectFadeStart = 60.0f;
	constexpr float kDefaultObjectFadeEnd = 95.0f;
	// 演出の初期値(保存済みJSONがあればそちらが優先される)
	constexpr SceneTransition::Style kDefaultFallTransitionStyle = SceneTransition::Style::RailStripes;
	constexpr SceneTransition::Style kDefaultClearTransitionStyle = SceneTransition::Style::InkBurst;
}

GameScene::GameScene() = default;
GameScene::~GameScene() = default;

// シーンの初期化
void GameScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
	object3dCommon_ = object3dCommon;
	input_ = input;
	spriteCommon_ = spriteCommon;

	// カメラの生成と設定
	CameraManager::GetInstance()->CreateCamera(kDefaultCameraName,object3dCommon_->GetDxCommon()->GetDevice());
	auto* defaultCamera = CameraManager::GetInstance()->GetCamera(kDefaultCameraName);
	defaultCamera->SetTranslate(kDefaultCameraPosition);
	CameraManager::GetInstance()->SetActiveCamera(kDefaultCameraName);
	object3dCommon_->SetDefaultCamera(CameraManager::GetInstance()->GetActiveCamera());

	// テクスチャの読み込み
	TextureManager::GetInstance()->LoadTexture(kSkyboxTexture);

	// スカイボックスの生成と初期化
	skyboxCommon_ = std::make_unique<SkyboxCommon>();
	skyboxCommon_->Initialize(object3dCommon_->GetDxCommon());
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(skyboxCommon_.get(),kSkyboxTexture);

	// レールエディターの生成と初期化
	railEditor_ = std::make_unique<RailEditor>();
	railEditor_->Initialize(object3dCommon_);

	// 着地できる範囲の可視化に、着地判定で使っている許容距離をそのまま渡す
	railEditor_->SetLandingRangeRadius(Player::kOnRailHorizontalThreshold);

	// 簡易的な地面の生成(ワールドに固定したスタート台の位置と正面の向きを基準に並べる)
	ground_ = std::make_unique<Ground>();
	ground_->Initialize(object3dCommon_,StartPlatform::kTopCenter,StartPlatform::kFrontDirection,kGameSceneGroup);

	// スタート台の生成(ワールドに固定した位置に置き、各ステージのレールの始点は台の辺に合わせて作る)
	startPlatform_ = std::make_unique<StartPlatform>();
	startPlatform_->Initialize(object3dCommon_);

	// 調整項目(GlobalVariables)のグループを作成する
	// 各クラスがこのグループに自分の調整項目を登録する
	// ImGuiの "Global Variables" ウィンドウから実行中に編集・保存でき、
	// resource/GlobalVariables/GameScene.json を外部で書き換えると自動反映される(ホットリロード)
	GlobalVariables::GetInstance()->CreateGroup(kGameSceneGroup);

	// 落下リスタート・クリア画面への切り替えの演出(ImGui で選んだものを "Global Variables" の Save で保存できる)
	GlobalVariables::GetInstance()->AddItem(kGameSceneGroup,kFallTransitionKey,static_cast<int32_t>(kDefaultFallTransitionStyle));
	GlobalVariables::GetInstance()->AddItem(kGameSceneGroup,kClearTransitionKey,static_cast<int32_t>(kDefaultClearTransitionStyle));
	GlobalVariables::GetInstance()->AddItem(kGameSceneGroup,kRespawnRevealKey,static_cast<int32_t>(kDefaultRespawnRevealStyle));
	GlobalVariables::GetInstance()->AddItem(kGameSceneGroup,kDepartureStyleKey,static_cast<int32_t>(kDefaultDepartureStyle));
	// プレイ会用の設定(Global Variables の Save で保存し、プレイ会では保存した JSON をそのまま使う)
	GlobalVariables::GetInstance()->AddItem(kGameSceneGroup,kPlaySessionModeKey,kDefaultPlaySessionMode);
	// 遠くの物を空の色に溶け込ませる距離(Play中だけ使う)
	GlobalVariables::GetInstance()->AddItem(kGameSceneGroup,kObjectFadeStartKey,kDefaultObjectFadeStart);
	GlobalVariables::GetInstance()->AddItem(kGameSceneGroup,kObjectFadeEndKey,kDefaultObjectFadeEnd);

	// 太陽の生成(方角はスタート台の正面の向きを基準にする)
	sun_ = std::make_unique<Sun>();
	sun_->Initialize(kGameSceneGroup,StartPlatform::kFrontDirection);

	// プレイヤーの生成
	player_ = std::make_unique<Player>();
	player_->Initialize(object3dCommon_,kGameSceneGroup);

	// レール追従カメラの生成
	railCamera_ = std::make_unique<RailCamera>();
	railCamera_->Initialize(kGameSceneGroup);
	// 衝撃の瞬間の画面演出(出発ゲートが開く瞬間のヒットストップ・画面の揺れ)
	screenImpact_ = std::make_unique<ScreenImpact>();

	// 俯瞰用のデバッグカメラを生成
	debugTopCamera_ = std::make_unique<DebugTopCamera>();
	debugTopCamera_->Initialize(object3dCommon_);

	// プレイヤーの弾の生成
	bulletManager_ = std::make_unique<PlayerBulletManager>();
	bulletManager_->Initialize(object3dCommon_);

	// 的の生成(保存済みの配置が無い初回起動時はレール沿いに自動配置する)
	targetManager_ = std::make_unique<TargetManager>();
	targetManager_->Initialize(object3dCommon_,kGameSceneGroup);

	// ゴールの的の生成(ゴールのレールに乗るまでは出さない)
	goalTarget_ = std::make_unique<GoalTarget>();
	goalTarget_->Initialize(object3dCommon_,kGameSceneGroup);

	// 雑魚敵の生成(保存済みの配置が無い初回起動時はレールの中間地点に自動配置する)
	enemyManager_ = std::make_unique<EnemyManager>();
	enemyManager_->Initialize(object3dCommon_);

	// 画面中央固定のレティクルを生成
	reticle_ = std::make_unique<Reticle>();
	reticle_->Initialize(spriteCommon_);

	// インクのしぶき・的の破片・閃光の演出(使い回す3Dオブジェクトをここでまとめて生成する)
	inkEffect_ = std::make_unique<InkEffectManager>();
	inkEffect_->Initialize(object3dCommon_);
	// インクのしぶきがスタート台の上面に落ちたら、落ちたところにインクを塗る
	inkEffect_->SetLandingHandler([this](const Vector3& from,const Vector3& to,float inkRadius){
		return startPlatform_->TryLandInk(from,to,inkRadius);
	});
	// 弾がスタート台の上面に当たったら、当たったところにインクを塗り、しぶきを跳ねさせる
	bulletManager_->SetLandingHandler([this](const Vector3& from,const Vector3& to,float inkRadius){
		Vector3 landedPosition;
		if(!startPlatform_->TryLandInk(from,to,inkRadius,&landedPosition)){
			return false;
		}
		inkEffect_->EmitInkSplash(landedPosition,kWorldUp,kBulletLandSplashCount);
		return true;
	});

	// 的を続けて壊したときの連鎖数
	comboCounter_ = std::make_unique<ComboCounter>();

	// プレイ中の画面表示(残機・壊した的の数・連鎖数)
	stageHUD_ = std::make_unique<StageHUD>();
	stageHUD_->Initialize(spriteCommon_);

	// ゴールの的を壊したときに出す「STAGE CLEAR!」の表示
	stageClearUI_ = std::make_unique<StageClearUI>();
	stageClearUI_->Initialize(spriteCommon_);
	// ゴールの的はプレイヤーの先(画面の真ん中あたり)を並走するため、「撃ちまくれ!!!」は的に重ならないよう画面の上の方に出す
	goalBannerUI_ = std::make_unique<StageClearUI>();
	goalBannerUI_->Initialize(spriteCommon_,kGoalBannerTexturePath,kGoalBannerHeightRate);
	stageLockedNotice_ = std::make_unique<StageLockedNotice>();
	stageLockedNotice_->Initialize(spriteCommon_);
	tutorialHints_ = std::make_unique<TutorialHints>();
	tutorialHints_->Initialize(spriteCommon_);
	railSwitchPrompt_ = std::make_unique<RailSwitchPrompt>();
	railSwitchPrompt_->Initialize(spriteCommon_);
	keyTargetMarker_ = std::make_unique<KeyTargetMarker>();
	keyTargetMarker_->Initialize(spriteCommon_);

	// レールを進んでいる間に出すスピード線
	speedLines_ = std::make_unique<SpeedLines>();
	speedLines_->Initialize(spriteCommon_);

	// プレイヤーの体力を、画面の縁から迫る敵のインク(ポストプロセス)で表現する
	damageInkEffect_ = std::make_unique<DamageInkEffect>();

	// H キーで開く操作説明(操作方法と画面の流れ)
	helpOverlay_ = std::make_unique<HelpOverlay>();
	helpOverlay_->Initialize(spriteCommon_);

	// 的の撃破時に発生させる火花パーティクルのグループを事前に生成しておく
	TextureManager::GetInstance()->LoadTexture(kHitParticleTexture);
	ParticleManager::GetInstance()->CreateParticleGroup(kHitParticleGroupName,kHitParticleTexture);

	// ステージ開始演出の生成(Playに入った瞬間にタイトルから始める)
	startSequence_ = std::make_unique<StartSequence>();
	// スタート台を渡し、出発ゲートの開け閉めを開始演出に任せる
	startSequence_->Initialize(object3dCommon_,spriteCommon_,input_,startPlatform_.get());

	// 出発の演出(使う演出は出発ゲートが開いた瞬間に、ImGui で選んだものに決める)
	departureDirector_ = std::make_unique<DepartureDirector>();
	departureDirector_->Initialize();

	// ゴールの的を壊したあと、大ジャンプでスタート台へ戻る演出
	returnJump_ = std::make_unique<ReturnJump>();

	// 着地した後、クリアしたステージの通路の壁を壊して見せる演出(各段階の長さを調整項目に登録する)
	barrierCutscene_ = std::make_unique<BarrierBreakCutscene>();
	barrierCutscene_->Initialize(kGameSceneGroup);
}

// シーンの終了処理
void GameScene::Finalize(){
	// Play中にTABでカーソルを隠したままシーンが切り替わると、次のシーンでもカーソルが消えたままになるため表示に戻す
	if(!isCursorVisible_){
		isCursorVisible_ = true;
		::ShowCursor(TRUE);
	}
	// カーソルをウィンドウ内に閉じ込めたままだと他のシーンやアプリで操作できなくなるため解除する
	if(isCursorClipped_){
		isCursorClipped_ = false;
		::ClipCursor(nullptr);
	}
	// 遠くの物を溶け込ませる距離は全オブジェクト共通の設定のため、他のシーンに持ち越さないよう元に戻す
	object3dCommon_->SetSceneDistanceFade(0.0f,0.0f);
	// 体力の表現はポストプロセスにかけているため、クリア画面・ゲームオーバー画面に持ち越さないよう外す
	damageInkEffect_->Reset();
}

// シーンの更新処理
void GameScene::Update(){
	// プレイ会用の設定かを、調整項目の今の値に合わせる(ゲートを開けられるか・クリアの後の流れに使う)
	StageInfo::isPlaySessionMode = GlobalVariables::GetInstance()->GetBoolValue(kGameSceneGroup,kPlaySessionModeKey);

	// 操作説明の開閉(H キー)
	// Editモードでは ImGui の文字入力で H を打っても開かないよう、Play中だけ開けるようにする
	// 開いている間は落ち着いて読めるよう、ゲームの進行と入力をすべて止める
	helpOverlay_->Update(input_,EditorContext::GetInstance()->IsPlayMode());
	if(helpOverlay_->IsOpen()){
		return;
	}

	// パーティクルの更新(発生タイミング・経過時間の進行。カメラ行列は描画時に反映する)
	ParticleManager::GetInstance()->Update();

	// 地面タイルの更新(霧のUVスクロールも行う)
	ground_->Update(kDeltaTime);

	// Play中だけ、遠くのレール・スタート台・的・敵などを空の色に溶け込ませ、カメラの遠平面で急に現れたり消えたりして見えないようにする
	// Edit中は俯瞰カメラで遠くの制御点も編集するため、溶け込ませない(地面は独自の距離で溶け込ませる)
	UpdateSceneDistanceFade();

	// 衝撃の瞬間の画面演出を進める
	// ヒットストップ中は、出発ゲートとインクの演出の時間を止めて、衝撃の瞬間を強調する
	screenImpact_->Update(kDeltaTime);
	const float effectDeltaTime = screenImpact_->IsHitStopping()?0.0f:kDeltaTime;

	// スタート台の出発ゲートの更新(ゲートへ流し込むインク・ランプ・遮断棒・足元の目印・出発の輪の動き)
	startPlatform_->Update(effectDeltaTime);
	// 遮断棒が跳ね上がった瞬間に、ゲートからインクを高く飛び散らせる(台に落ちたところにインクの跡が残る)
	// 同時に一瞬だけ動きを止め、画面を揺らし、ゲートを中心に放射ブラーをかけて、ゲートが開く衝撃を見せる
	if(startPlatform_->IsBarBurstTriggered()){
		// 使う出発の演出は、ゲートが開いた瞬間に ImGui で選んでいるものに決める(インク溜まりを塗るなど、出発台の準備もここで行う)
		// タイトルを飛ばしたときはゲートを開ける演出を挟まないため、出発の演出も使わない
		if(startSequence_->IsOpeningGate()){
			departureDirector_->Prepare(GetDepartureStyle(),MakeDepartureContext());
		}
		const Vector3 burstPosition = startPlatform_->GetGateBurstPosition();
		inkEffect_->EmitLaunchBurst(burstPosition,kGateBurstCount);
		screenImpact_->Trigger(kGateHitStopDuration,kGateShakeAmplitude,kGateShakeDuration);
		if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
			Application::GetInstance()->TriggerRadialBlur(CameraPoseMath::WorldToScreenUv(burstPosition,activeCamera->GetViewProjectionMatrix()),kGateRadialBlurWidth,kGateRadialBlurDuration);
		}
	}

	// 太陽の光の向き・色・明るさを調整項目の最新値で更新する
	sun_->Update();

	// レティクルの更新
	reticle_->Update();

	// レールエディターの更新
	if(railEditor_){
		// 走り切るまでの秒数の表示に使う全体の速さを、Playerの調整項目から渡す
		railEditor_->SetBaseRailSpeed(GlobalVariables::GetInstance()->GetFloatValue(kGameSceneGroup,kRailSpeedKey));
		railEditor_->Update();

		// Rail Editorの「Focus」・一覧のダブルクリックで、選択中の制御点に俯瞰デバッグカメラを寄せる
		Vector3 focusPosition{};
		if(railEditor_->ConsumeFocusRequest(focusPosition)){
			debugTopCamera_->FocusOnPoint(focusPosition);
		}

		// Rail Editorの一覧でステージを選び直したら、的・敵のエディターも一緒にそのステージへ切り替える
		int requestedStageIndex = 0;
		if(railEditor_->ConsumeStageChangeRequest(requestedStageIndex)){
			SwitchStage(requestedStageIndex);
		}
	}

	// 的・敵の配置エディターの更新(配置モード中のドラッグ移動と、編集結果の反映もここで処理される)
	targetManager_->UpdateEditor();
	enemyManager_->UpdateEditor();

	// Target Editorの「Focus」・F キー・一覧のダブルクリックで、選択中の的に俯瞰デバッグカメラを寄せる
	Vector3 targetFocusPosition{};
	if(targetManager_->ConsumeFocusRequest(targetFocusPosition)){
		debugTopCamera_->FocusOnPoint(targetFocusPosition);
	}

	// レール進行・カメラ・プレイヤー・敵・弾・的の更新
	if(railEditor_){
		UpdateGameplay();
	}

	// このフレームでスタート台に塗ったインクを塗りテクスチャに描き込む
	// しぶきの着地(UpdateGameplay の中の演出の更新)で塗る分も同じフレームで反映するよう、その後に行う
	startPlatform_->FlushInk();

	// ImGuiが無い環境でも表示切り替えができるよう、キー操作で各種デバッグ表示をトグルする
	if(input_ && input_->TriggerKey(DIK_F4) && railEditor_){
		railEditor_->ToggleShowControlPointModels();
	}
	if(input_ && input_->TriggerKey(DIK_F5) && railEditor_){
		railEditor_->ToggleShowCurve();
	}

	// Editモード中は俯瞰デバッグカメラをWASD(平面移動)+QE(高さ)+右ドラッグ(向き)で自由に動かせるようにする
	debugTopCamera_->UpdateMove(input_);

#ifdef USE_IMGUI
	// Playモード中は編集用パネルをすべて隠す(クリーンな実行画面にするため)
	if(!EditorContext::GetInstance()->IsPlayMode()){
		ShowEditorPanels();
	}
#endif
}

// マウスカーソルの表示/非表示切り替え
// Editモードでは常に表示する(ImGui操作にカーソルが必要なため、Play中に消していても強制的に戻す)
// Playモード中はTABキーで切り替えられるようにする(照準はマウスの移動量のみで行うため、カーソル表示が邪魔になることがある)
void GameScene::UpdateCursorVisibility(bool isPlayMode){
	if(!isPlayMode){
		if(!isCursorVisible_){
			isCursorVisible_ = true;
			::ShowCursor(TRUE);
		}
	} else if(input_ && input_->TriggerKey(DIK_TAB)){
		isCursorVisible_ = !isCursorVisible_;
		::ShowCursor(isCursorVisible_?TRUE:FALSE);
	}

	// カーソルを隠している間は、ウィンドウの外へ出てミスクリックしないようクライアント領域内に閉じ込める
	// 照準はDirectInputのマウス移動量で行うため、カーソルが端で止まっても振り向きは途切れない
	// ClipCursorは画面全体に効くため、ウィンドウが非アクティブになったら解除し、Alt+Tabなどで他のアプリを操作できるようにする
	// (GetActiveWindowは自分のウィンドウがアクティブでない間はnullptrを返す)
	HWND activeWindow = ::GetActiveWindow();
	const bool shouldClip = !isCursorVisible_ && activeWindow != nullptr;
	if(shouldClip){
		// クライアント領域をスクリーン座標に変換して閉じ込める範囲にする
		RECT clientRect{};
		::GetClientRect(activeWindow,&clientRect);
		POINT topLeft{clientRect.left,clientRect.top};
		POINT bottomRight{clientRect.right,clientRect.bottom};
		::ClientToScreen(activeWindow,&topLeft);
		::ClientToScreen(activeWindow,&bottomRight);
		RECT clipRect{topLeft.x,topLeft.y,bottomRight.x,bottomRight.y};

		// 閉じ込めはウィンドウの移動や他のアプリの操作などで Windows 側から外されることがあるため、
		// 一度かけて終わりにせず毎フレーム今の範囲と比べ、外れていたり範囲がずれていたりしたらかけ直す
		RECT currentClipRect{};
		::GetClipCursor(&currentClipRect);
		if(!::EqualRect(&currentClipRect,&clipRect)){
			::ClipCursor(&clipRect);
		}
		isCursorClipped_ = true;
	} else if(isCursorClipped_){
		::ClipCursor(nullptr);
		isCursorClipped_ = false;
	}
}

// 遠くの物を空の色に溶け込ませる距離を、全オブジェクト共通の設定にする
void GameScene::UpdateSceneDistanceFade(){
	if(!EditorContext::GetInstance()->IsPlayMode()){
		object3dCommon_->SetSceneDistanceFade(0.0f,0.0f);
		return;
	}

	// カメラの遠平面より先は描画されず、そこで切れて見えるため、溶け込みきる距離は遠平面より遠くしない
	// (カメラからの距離は、遠平面で切れる位置の奥行きより必ず長いため、遠平面の距離で比べれば足りる)
	GlobalVariables* gv = GlobalVariables::GetInstance();
	const float requestedFadeEnd = gv->GetFloatValue(kGameSceneGroup,kObjectFadeEndKey);
	// カメラが無いときは遠平面が分からないため、調整項目の値をそのまま使う
	float maxFadeEnd = requestedFadeEnd;
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		maxFadeEnd = activeCamera->GetFarClip();
	}
	const float fadeEnd = std::clamp(requestedFadeEnd,0.0f,maxFadeEnd);
	const float fadeStart = std::clamp(gv->GetFloatValue(kGameSceneGroup,kObjectFadeStartKey),0.0f,fadeEnd);
	object3dCommon_->SetSceneDistanceFade(fadeStart,fadeEnd);
}

// シーンの描画処理
void GameScene::Draw(){
	// スカイボックス(天球の画像)を一番最初に描く
	// 深度は常に最奥(1.0)で書き込まないため、後から描く3Dオブジェクトがすべて手前に表示される
	// Update()の時点ではカメラ行列が前フレームのままのため、カメラ更新後の描画時に行列を更新して空が1フレーム遅れて回るのを防ぐ
	if(skybox_){
		skybox_->Update(*CameraManager::GetInstance()->GetActiveCamera());
		skybox_->Draw();
	}

	object3dCommon_->Draw();

	// 雲海の雲の部分を描画(他のオブジェクトより先に描く)
	ground_->Draw();

	// スタート台を描画
	startPlatform_->Draw();

	// レールエディターの描画
	if(railEditor_){
		railEditor_->Draw();
	}

	// プレイヤー(人型モデル)を描画
	// やられ演出の最中は、インクになって弾けたため描かない
	if(!isDefeated_){
		player_->Draw();
	}

	// タイトルロゴを描画(タイトル中・カメラの回り込み中のみ)
	if(startSequence_){
		startSequence_->Draw3D();
	}

	// 雑魚敵を描画(撃破済みのときはEnemy側で描画をスキップする)
	enemyManager_->Draw();

	// 生存している的だけ描画
	targetManager_->Draw();

	// ゴールの的を描画(現れていて、まだ壊れていないときだけ)
	goalTarget_->Draw();

	// 発射中の弾を描画
	bulletManager_->Draw();

	// インクのしぶき・的の破片・閃光を描画
	inkEffect_->Draw();

	// スタート台の足元の目印と出発の輪を描画(半透明なので、不透明な3Dオブジェクトの後に描く)
	startPlatform_->DrawEffects();

	// 雲の上の霧を描画(半透明なので、奥にあるオブジェクトが透けて見えるよう3Dオブジェクトの最後に描く)
	ground_->DrawFog();

	// 撃破演出パーティクルの描画(3Dオブジェクトの後、2Dレティクルの前に描画する)
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		ParticleManager::GetInstance()->Draw(activeCamera);
	}

	// 画面中央固定のレティクルを描画
	// タイトル中・カメラの回り込み中・出発の演出でレールへ向かっている間は撃てないため表示しない
	// スタート台の上では試し撃ちができるため表示する(ゲートが開く間も、出たり消えたりしないよう表示したままにする)
	bool isTitleShowing = startSequence_ && startSequence_->IsControllingCamera();
	bool isDeparting = startSequence_ && startSequence_->IsDeparting();
	// スタート台へ戻る大ジャンプの最中も撃てないため表示しない
	bool isReturning = returnJump_ && returnJump_->IsRunning();
	// 着地した後に通路の壁を壊して見せている間も撃てないため表示しない
	bool isBreakingBarrier = barrierCutscene_ && barrierCutscene_->IsRunning();

	// スピード線はレティクル・画面表示より奥に見えるよう先に描く
	speedLines_->Draw();

	if(!isTitleShowing && !isDeparting && !isReturning && !isBreakingBarrier){
		reticle_->Draw();
	}

	// 体力・壊した的の数・連鎖数はPlay中だけ表示する(タイトル中・カメラの回り込み中は出さない)
	if(EditorContext::GetInstance()->IsPlayMode() && !isTitleShowing){
		stageHUD_->Draw();
		// 「STAGE CLEAR!」は画面表示より手前に出す
		stageClearUI_->Draw();
		goalBannerUI_->Draw();
		stageLockedNotice_->Draw();
		keyTargetMarker_->Draw();
		tutorialHints_->Draw();
		railSwitchPrompt_->Draw();
	}

	// 開始演出のタイトル表示(PRESS SPACE)と、出発までの表示(操作案内・GO!)を最前面に描画する
	if(startSequence_){
		startSequence_->Draw2D();
	}

	// 操作説明(閉じている間は左下の案内)は、タイトル表示よりさらに手前に描画する
	helpOverlay_->Draw();
}
