#include "GameScene.h"
#include "CameraManager.h"
#include "ImGuiManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Skybox.h"
#include "SkyboxCommon.h"
#include "Input.h"
#include "Obj3dCommon.h"
#include "SpriteCommon.h"
#include "WinAPI.h"
#include "Application.h"
#include "GlobalVariables.h"
#include "EditorWidgets.h"
#include "EditorContext.h"

// レールエディター
#include "Editor/RailEditor.h"
#include "Editor/EditorDocumentManager.h"
// ステージ開始演出(タイトル → カメラの回り込み → スタート台で準備 → レールへ乗り込む)
#include "Title/StartSequence.h"
// 出発の演出(スタート台からステージのレールへ移るときの演出)
#include "Title/Departure/DepartureDirector.h"
// ゲームオブジェクト
#include "objects/Ground.h"
#include "objects/Sun.h"
#include "objects/Player.h"
#include "objects/StartPlatform.h"
#include "objects/PlayerBulletManager.h"
#include "objects/TargetManager.h"
#include "objects/EnemyManager.h"
#include "objects/InkEffectManager.h"
#include "objects/ComboCounter.h"
#include "objects/StageResult.h"
// カメラ
#include "Camera/RailCamera.h"
#include "Camera/DebugTopCamera.h"
#include "Camera/ScreenImpact.h"
#include "Camera/CameraPose.h"
// UI
#include "UI/Reticle.h"
#include "UI/StageHUD.h"
#include "UI/SpeedLines.h"
#include "UI/HelpOverlay.h"
#include <algorithm>
#include <iterator>

namespace{
	// スカイボックスのテクスチャパス
	// 天球(霧で白んだ水色の空と遠景の建造物)の画像。tools/generate_celestial_sphere.py で生成する
	const std::string kSkyboxTexture = "resource/Skybox/celestialSphere.dds";
	// GlobalVariablesのグループ名(GameSceneの調整項目)
	const char* kGameSceneGroup = "GameScene";

	// プレイ用カメラの名前
	const char* kDefaultCameraName = "default";
	// プレイ用カメラの初期位置
	constexpr Vector3 kDefaultCameraPosition = {0.0f, 0.0f, -30.0f};

	// 撃破演出(パーティクル)に使用するテクスチャパス
	const std::string kHitParticleTexture = "resource/circle.png";
	// ParticleManager::EmitSpark()が内部で使用するグループ名と合わせる必要がある
	const char* kHitParticleGroupName = "Spark";

	// 弾が画面中央のレティクルへ向かって飛ぶよう、カメラの前方のこの距離の点を狙って撃ち出す
	// (カメラはプレイヤーの後ろ上にあるため、カメラと同じ向きで平行に撃つとレティクルより下にずれる)
	constexpr float kAimConvergeDistance = 30.0f;

	// レールに乗り込んで出発した瞬間のインクのしぶきを飛ばす向き(真上)
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};

	// 落下リスタート・クリア画面への切り替えの演出を保存する調整項目名(SceneTransition::Style の番号で保存する)
	// GameScene はシーンが切り替わるたびに作り直されるため、ImGui で選んだ演出を調整項目に持たせて保存・保持する
	const char* kFallTransitionKey = "fallTransitionStyle";
	const char* kClearTransitionKey = "clearTransitionStyle";
	// 出発の演出を保存する調整項目名(DepartureStyle の番号で保存する)と、初期値
	const char* kDepartureStyleKey = "departureStyle";
	constexpr DepartureStyle kDefaultDepartureStyle = DepartureStyle::CountdownFlip;
	// レールを進む全体の速さの調整項目名(Playerが登録する。Rail Editorで走り切るまでの秒数を出すのに使う)
	const char* kRailSpeedKey = "railSpeed";
	// 鍵の的を壊して出たレールの管を伸ばす速さを、レールを進む全体の速さより速くする量(1秒あたりの長さ)
	constexpr float kRailUnlockExtraSpeed = 3.0f;
	// Play中に、遠くのレール・スタート台・的・敵などを空の色に溶け込ませ始める距離・溶け込みきる距離の調整項目名と初期値(カメラからの距離)
	// レールは進む先や飛び移れるレールを見せる道しるべで、敵は25以内でプレイヤーに気づくため、地面(45〜95)より遠くから溶け込ませる
	const char* kObjectFadeStartKey = "objectFadeStart";
	const char* kObjectFadeEndKey = "objectFadeEnd";
	constexpr float kDefaultObjectFadeStart = 60.0f;
	constexpr float kDefaultObjectFadeEnd = 95.0f;
	// 演出の初期値(保存済みJSONがあればそちらが優先される)
	constexpr SceneTransition::Style kDefaultFallTransitionStyle = SceneTransition::Style::RailStripes;
	constexpr SceneTransition::Style kDefaultClearTransitionStyle = SceneTransition::Style::InkBurst;

	// 調整項目に保存した番号から演出の種類を取得する
	SceneTransition::Style GetTransitionStyle(const char* key){
		return static_cast<SceneTransition::Style>(GlobalVariables::GetInstance()->GetIntValue(kGameSceneGroup,key));
	}

	// 調整項目に保存した番号から出発の演出の種類を取得する
	DepartureStyle GetDepartureStyle(){
		return static_cast<DepartureStyle>(GlobalVariables::GetInstance()->GetIntValue(kGameSceneGroup,kDepartureStyleKey));
	}

#ifdef USE_IMGUI
	// 落下リスタートの演出の名前(ImGui の切り替えに表示する。SceneTransition::Style と同じ順番にする)
	const char* kFallTransitionNames[] = {"Fade", "RailStripes", "FallAndLift", "RetryCutIn"};

	// クリア画面へ切り替えるときの演出の候補と、ImGui の切り替えに表示する名前(同じ順番にする)
	constexpr SceneTransition::Style kClearTransitionStyles[] = {
		SceneTransition::Style::InkBurst,
		SceneTransition::Style::RailPaint,
		SceneTransition::Style::GoalStamp,
		SceneTransition::Style::CloudRise,
	};
	const char* kClearTransitionNames[] = {"InkBurst", "RailPaint", "GoalStamp", "CloudRise"};
	static_assert(std::size(kClearTransitionStyles) == std::size(kClearTransitionNames),"クリアの演出の候補と名前の数が合っていない");
#endif
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

	// 簡易的な地面の生成(レール開始地点を基準に並べるため、レールエディターの初期化後に行う)
	ground_ = std::make_unique<Ground>();
	ground_->Initialize(object3dCommon_,railEditor_->GetPositionOnRail(0.0f),railEditor_->GetForwardOnRail(0.0f),kGameSceneGroup);

	// 最初のレールの手前に置くスタート台の生成(Edit中にも見えるよう、ここで一度レールの始点に合わせて置いておく)
	startPlatform_ = std::make_unique<StartPlatform>();
	startPlatform_->Initialize(object3dCommon_);
	startPlatform_->Place(railEditor_->GetPositionOnRail(0.0f),railEditor_->GetForwardOnRail(0.0f));

	// 調整項目(GlobalVariables)のグループを作成する
	// 各クラスがこのグループに自分の調整項目を登録する
	// ImGuiの "Global Variables" ウィンドウから実行中に編集・保存でき、
	// resource/GlobalVariables/GameScene.json を外部で書き換えると自動反映される(ホットリロード)
	GlobalVariables::GetInstance()->CreateGroup(kGameSceneGroup);

	// 落下リスタート・クリア画面への切り替えの演出(ImGui で選んだものを "Global Variables" の Save で保存できる)
	GlobalVariables::GetInstance()->AddItem(kGameSceneGroup,kFallTransitionKey,static_cast<int32_t>(kDefaultFallTransitionStyle));
	GlobalVariables::GetInstance()->AddItem(kGameSceneGroup,kClearTransitionKey,static_cast<int32_t>(kDefaultClearTransitionStyle));
	GlobalVariables::GetInstance()->AddItem(kGameSceneGroup,kDepartureStyleKey,static_cast<int32_t>(kDefaultDepartureStyle));
	// 遠くの物を空の色に溶け込ませる距離(Play中だけ使う)
	GlobalVariables::GetInstance()->AddItem(kGameSceneGroup,kObjectFadeStartKey,kDefaultObjectFadeStart);
	GlobalVariables::GetInstance()->AddItem(kGameSceneGroup,kObjectFadeEndKey,kDefaultObjectFadeEnd);

	// 太陽の生成(方角はレール開始地点の進行方向を基準にする)
	sun_ = std::make_unique<Sun>();
	sun_->Initialize(kGameSceneGroup,railEditor_->GetForwardOnRail(0.0f));

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
	targetManager_->Initialize(object3dCommon_,railEditor_.get(),kGameSceneGroup);

	// 雑魚敵の生成(保存済みの配置が無い初回起動時はレールの中間地点に自動配置する)
	enemyManager_ = std::make_unique<EnemyManager>();
	enemyManager_->Initialize(object3dCommon_,railEditor_.get());

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

	// プレイ中の画面表示(体力・壊した的の数・連鎖数)
	stageHUD_ = std::make_unique<StageHUD>();
	stageHUD_->Initialize(spriteCommon_);

	// レールを進んでいる間に出すスピード線
	speedLines_ = std::make_unique<SpeedLines>();
	speedLines_->Initialize(spriteCommon_);

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
	// 開始時に残っている弾もリセットする
	bulletManager_->Clear();

	// レールを0番へ戻し、インクで塗った区間も消す
	if(railEditor_){
		railEditor_->SwitchActiveRail(0);
		railEditor_->ClearPaint();

		// スタート台を最初のレールの手前に置き直し、プレイヤーを台の上から始めさせる
		// Edit中にレールを動かしても反映されるよう、始め直すたびに置き直す
		startPlatform_->Place(railEditor_->GetPositionOnRail(0.0f),railEditor_->GetForwardOnRail(0.0f));
		player_->StartOnPlatform(startPlatform_->GetSpawnPosition(),railEditor_->GetRotationOnRail(0.0f));

		// 的をすべて復活させたため、鍵の的があるレールは出ていない状態にすぐ戻す
		ApplyRailLocks(true);
	}

	// 出ている演出・連鎖数・スピード線・画面の揺れ・出発の演出も消す
	inkEffect_->Reset();
	screenImpact_->Reset();
	departureDirector_->Reset();
	comboCounter_->Reset();
	speedLines_->Reset();

	// 雑魚敵も撃破前の初期状態(体力満タン)から始める
	enemyManager_->Reset();
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

// シーンの更新処理
void GameScene::Update(){
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
	}

	// 的・敵の配置エディターの更新(配置モード中のドラッグ移動と、編集結果の反映もここで処理される)
	targetManager_->UpdateEditor();
	enemyManager_->UpdateEditor();

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
	if(shouldClip && !isCursorClipped_){
		// クライアント領域をスクリーン座標に変換して閉じ込める範囲にする
		RECT clientRect{};
		::GetClientRect(activeWindow,&clientRect);
		POINT topLeft{clientRect.left,clientRect.top};
		POINT bottomRight{clientRect.right,clientRect.bottom};
		::ClientToScreen(activeWindow,&topLeft);
		::ClientToScreen(activeWindow,&bottomRight);
		RECT clipRect{topLeft.x,topLeft.y,bottomRight.x,bottomRight.y};
		::ClipCursor(&clipRect);
		isCursorClipped_ = true;
	} else if(!shouldClip && isCursorClipped_){
		::ClipCursor(nullptr);
		isCursorClipped_ = false;
	}
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
		checkpoint_ = Checkpoint{};
		ResetPlayState();
		StartPlaySequence();
	} else if(!isPlayMode && wasPlayMode_){
		startSequence_->Stop();
		departureDirector_->Reset();
		// Editモードに戻ったら、Play前に見ていた位置のまま俯瞰デバッグカメラに戻す
		debugTopCamera_->Enable();
		// Play前に編集していたレールと制御点を選び直し、続けて編集できるようにする
		railEditor_->SelectPoint(editRailIndex_,editPointIndex_);
	} else if(isPlayMode && input_ && input_->TriggerKey(DIK_R)){
		// 確認用の一時的な処理:Play中にRキーでタイトルから最初からやり直す(選んだ制御点から始めているときは、その制御点からやり直す)
		// 最初からやり直すため、通ったチェックポイントも忘れる
		checkpoint_ = Checkpoint{};
		ResetPlayState();
		StartPlaySequence();
	}
	wasPlayMode_ = isPlayMode;

	// 開始演出の更新(タイトル中のSPACE入力・カメラの回り込み・出発ゲートの前でのSPACE入力)
	// ゲートの前にいるかは、前フレームで動かしたプレイヤーの座標で判定する
	if(isPlayMode){
		startSequence_->Update(kDeltaTime,player_->GetPosition());

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
	const bool isGameplayActive = isPlayMode && startSequence_->IsPlayable() && !sceneManager_->IsTransitioning();
	// 開始演出のうち、スタート台の上を自由に動ける段階か(出発ゲートの前で準備中と、ゲートが開いてレールへ乗り込むまで)
	// 動き方(WASD・ジャンプ・台から落ちたときのリスタート)はプレイ中と同じで、敵・射撃・勝敗判定はまだ動かさない
	const bool isPlayerMovable = isPlayMode && startSequence_->IsPlayerMovable() && !sceneManager_->IsTransitioning();

	// 出発の演出を使うときは、ゲートが開いてから演出を始めるまでの間だけ、ゲートの先に出発台の目印を出す
	startPlatform_->SetDeparturePadVisible(startSequence_->IsBoarding() && departureDirector_->UsesDeparturePad());

	// マウスカーソルの表示/非表示切り替え
	UpdateCursorVisibility(isPlayMode);

	// レール進行度を時間で進める
	player_->UpdateRailProgress(railEditor_.get(),isGameplayActive,kDeltaTime);

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
	railCamera_->Update(player_->GetBasePosition(),player_->GetBaseRotation(),isGameplayActive || isPlayerMovable,input_,kDeltaTime);
	const Vector3& cameraPos = railCamera_->GetPosition();
	const Vector3& cameraForward = railCamera_->GetForward();

	// Editモードで始まったときは、Playの最初と同じプレイ用カメラの画から俯瞰デバッグカメラを動かし始める
	// Playから戻ったときは Enable() 済みのため、Play前に見ていた画のまま編集を続けられる
	if(!isPlayMode && !debugTopCamera_->IsEnabled()){
		debugTopCamera_->SetPose({cameraPos, railCamera_->GetRotation()});
		debugTopCamera_->Enable();
	}

	// クリア判定
	// アクティブなレールが最後まで到達したらクリアとする(オフレール中は判定しない)
	// 切り替えは次フレームに行われるため、以降の判定(ゲームオーバー・落下リスタートなど)で上書きされないようここで抜ける
	if(isGameplayActive && player_->HasReachedGoal()){
		// クリア画面で結果を表示できるよう、シーンが作り直される前に書き込んでおく
		StageResult::destroyedTargetCount = targetManager_->GetDestroyedCount();
		StageResult::totalTargetCount = targetManager_->GetTotalCount();
		StageResult::maxCombo = comboCounter_->GetMaxCombo();
		// クリアは ImGui で選んだ演出(初期値は黄色いインクが弾けて画面を塗りつぶす InkBurst)で切り替える
		sceneManager_->ChangeScene("CLEAR",GetTransitionStyle(kClearTransitionKey));
		return;
	}

#ifdef USE_IMGUI
	// レールの状態確認用デバッグ表示(Playモード中も含めて常に表示する)
	ShowStatusWindow();
#endif

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

	// 開始演出中は、プレイ用カメラをプレイヤー中心に回転させた位置・向きでカメラを上書きする
	// (カメラの行列はシーン更新の後にまとめて更新されるため、ここで上書きしてもこのフレームの描画に反映される)
	// タイトルロゴも同じくプレイヤーの位置・向きを基準に配置する
	CameraPose departureCamera;
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
	enemyManager_->Update(playerPos,isGameplayActive?kDeltaTime:0.0f);

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

	// ゲームオーバー判定
	// 体力が0になったらゲームオーバー画面へ遷移する
	// クリア判定と同じく、以降の落下リスタートなどの処理を行わないようここで抜ける
	if(isGameplayActive && player_->IsDead()){
		// ゲームオーバーは暗いインクが上から垂れてきて画面を覆う演出で切り替える
		sceneManager_->ChangeScene("GAMEOVER",SceneTransition::Style::InkDrip);
		return;
	}

	// プレイヤーの自立(ジャンプ+WASD移動)
	// どのレールにも乗れないまま地面の高さまで落ちたら、レール先頭からやり直す
	// 一瞬で始点へ戻ると何が起きたか分かりにくいため、暗転で画面を覆っている間に始点へ戻す
	// 開始演出でスタート台の上を動ける間も、同じ動き方で動かす
	if(isGameplayActive || isPlayerMovable){
		bool needsRestart = player_->UpdateMovement(input_,railEditor_.get(),startPlatform_.get(),cameraForward,railCamera_->GetRight(),ground_->GetHeight(),kDeltaTime);
		if(needsRestart){
			sceneManager_->StartTransition([this](){
				ResetPlayState();
				// チェックポイントを通っていれば、そこから始め直す
				// 通っていなくても、選んだ制御点から始めているときは、落ちても同じ制御点から始め直す
				if(checkpoint_.railIndex >= 0){
					RespawnAtCheckpoint();
				} else if(isPlayingFromPoint_){
					BoardAtPlayStartPoint();
				}
			},GetTransitionStyle(kFallTransitionKey));
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

	// 弾の発射処理(左クリックした瞬間に1発だけ発射する)
	// スタート台の上を動ける間も試し撃ちとして撃てるようにし、台に当たったところを塗れるようにする
	// 試し撃ちの弾は的・敵には当たらないようにして、プレイが始まる前に的を壊したり敵を倒したりしないようにする
	bool shootTriggered = (isGameplayActive || isPlayerMovable) && input_ && input_->TriggerMouseButton(kShootMouseButton);
	if(shootTriggered){
		// プレイヤーの手元から、レティクルの先の点へ向けて撃ち出す
		Vector3 muzzlePos = player_->GetMuzzlePosition();
		Vector3 aimPoint = cameraPos + cameraForward * kAimConvergeDistance;
		Vector3 shootDirection = Normalize(aimPoint - muzzlePos);
		bulletManager_->Fire(muzzlePos,shootDirection,isGameplayActive);

		// 手元から撃つ向きへインクを少し飛び散らせる
		inkEffect_->EmitInkSplash(muzzlePos,shootDirection,kMuzzleSplashCount);
	}

	// 弾の移動更新と生存時間チェック
	bulletManager_->Move(kDeltaTime);

	// 連鎖が途切れるまでの時間を進める(Edit中・開始演出中は止めておく)
	comboCounter_->Update(isGameplayActive?kDeltaTime:0.0f);

	// 的の照準判定と弾との当たり判定
	bool isAimingAtAnyTarget = targetManager_->Update(cameraPos,cameraForward,*bulletManager_);

	// 壊れた的ごとに、破片・しぶき・閃光を出して連鎖数を増やす
	for(const Vector3& destroyedPos : targetManager_->GetDestroyedPositions()){
		inkEffect_->EmitTargetBreak(destroyedPos);
		comboCounter_->AddHit();
	}

	// 鍵の的を壊したら、そのレールを出す(始点から終点へ管を伸ばす)
	ApplyRailLocks(false);

	// 雑魚敵への被弾判定
	enemyManager_->CheckHitByBullets(*bulletManager_);

	// 命中または生存時間切れで消えた弾をリストから削除し、残った弾の描画用の行列を更新する
	bulletManager_->RemoveDeadBullets();
	bulletManager_->UpdateTransforms();

	// 狙えているときはレティクル中心を赤くする
	reticle_->SetAiming(isAimingAtAnyTarget);

	// 演出の更新(このフレームで出した分も含めて行列を求めるため、出す処理がすべて終わってから行う)
	// ヒットストップ中は、飛び散ったインクも空中で止める
	inkEffect_->Update(screenImpact_->IsHitStopping()?0.0f:kDeltaTime);

	// スピード線はレールを進んでいる間だけ出す
	speedLines_->Update(isRidingRail,kDeltaTime);

	// 画面表示の更新
	stageHUD_->Update(player_->GetHp(),player_->GetMaxHp(),targetManager_->GetDestroyedCount(),targetManager_->GetTotalCount(),*comboCounter_);
}

#ifdef USE_IMGUI
// レール・プレイヤー・敵の状態確認用のデバッグウィンドウ
void GameScene::ShowStatusWindow(){
	ImGui::Begin("Rail Branch Debug");
	ImGui::Text("Rail Count: %d",railEditor_->GetRailCount());
	ImGui::Text("Active Rail Index: %d",railEditor_->GetActiveRailIndex());
	ImGui::Text("Active Rail Point Count: %d",railEditor_->GetControlPointCount());
	ImGui::Text("Rail T: %.3f",player_->GetRailT());
	ImGui::Text("Current Point Index: %d",railEditor_->GetControlPointIndexFromT(player_->GetRailT()));

	// オンレール判定・自由移動のデバッグ表示
	ImGui::Text("On Rail: %s",player_->IsOnRail()?"true":"false");
	if(!player_->IsOnRail()){
		ImGui::Text("Free Velocity Y: %.2f",player_->GetFreeVelocityY());
	}
	ImGui::Text("Jump Key: SPACE");

	// 雑魚敵のデバッグ表示
	enemyManager_->ShowDebugInfo();

	// プレイヤーの体力・無敵時間のデバッグ表示
	ImGui::Text("Player HP: %d / %d",player_->GetHp(),player_->GetMaxHp());
	ImGui::Text("Player Invincible: %.2f",player_->GetInvincibleTimer());
	ImGui::End();
}

// Editモード中の編集用パネル
void GameScene::ShowEditorPanels(){
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
	if(!activeCamera){
		return;
	}

	EditorWidgets::Layout L = EditorWidgets::ComputeLayout();
	// デバッグ用のメインウィンドウ(下段・左)
	EditorWidgets::BeginFixedPanel("GameScene Debug",L.bottomLeft);

	// ONにすると、Playを押したときにタイトル・スタート演出を飛ばし、レールの上からすぐプレイを始める(デバッグ用)
	ImGui::Checkbox("Skip Title",&skipTitle_);
	// ONにすると、Playを押したときにRail Editorで選んでいる制御点からプレイを始める(タイトル・スタート台も飛ばす。落ちたときも同じ制御点から始め直す)
	ImGui::Checkbox("Start From Selected Point",&startsFromSelectedPoint_);
	ImGui::TextDisabled("(未選択ならいつも通り)");

	// レール・的・敵の編集のUndo/Redoと、まとめて保存(Ctrl+Z / Ctrl+Y / Ctrl+Sでも操作できる)
	EditorDocumentManager* documentManager = EditorDocumentManager::GetInstance();
	documentManager->HandleShortcuts();
	ImGui::BeginDisabled(!documentManager->CanUndo());
	if(ImGui::Button("Undo")){
		documentManager->Undo();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!documentManager->CanRedo());
	if(ImGui::Button("Redo")){
		documentManager->Redo();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if(ImGui::Button("Save All")){
		documentManager->SaveAll();
	}

	// 保存していない変更があるエディターの名前を並べる(保存し忘れに気づけるようにする)
	bool hasUnsavedDocument = false;
	for(const IEditorDocument* document : documentManager->GetDocuments()){
		if(!document->IsModified()){
			continue;
		}
		if(!hasUnsavedDocument){
			ImGui::TextUnformatted("Unsaved:");
			hasUnsavedDocument = true;
		}
		ImGui::SameLine();
		ImGui::TextUnformatted(document->GetDocumentName());
	}

	// 落下リスタートの演出を切り替える。Preview を押すと落ちなくてもその場で演出だけを確認できる
	// 選んだ演出は調整項目に書き込み、"Global Variables" ウィンドウの Save で JSON に保存する
	GlobalVariables* gv = GlobalVariables::GetInstance();
	const SceneTransition::Style fallTransitionStyle = GetTransitionStyle(kFallTransitionKey);
	int fallStyleIndex = static_cast<int>(fallTransitionStyle);
	if(ImGui::Combo("Fall Transition",&fallStyleIndex,kFallTransitionNames,IM_ARRAYSIZE(kFallTransitionNames))){
		gv->SetValue(kGameSceneGroup,kFallTransitionKey,static_cast<int32_t>(fallStyleIndex));
	}
	if(ImGui::Button("Preview Fall Transition")){
		sceneManager_->StartTransition(nullptr,fallTransitionStyle);
	}

	// クリア画面へ切り替えるときの演出を切り替える。Preview を押すとクリアしなくてもその場で演出だけを確認できる
	const SceneTransition::Style clearTransitionStyle = GetTransitionStyle(kClearTransitionKey);
	int clearStyleIndex = 0;
	for(int i = 0; i < static_cast<int>(std::size(kClearTransitionStyles)); ++i){
		if(kClearTransitionStyles[i] == clearTransitionStyle){
			clearStyleIndex = i;
		}
	}
	if(ImGui::Combo("Clear Transition",&clearStyleIndex,kClearTransitionNames,IM_ARRAYSIZE(kClearTransitionNames))){
		gv->SetValue(kGameSceneGroup,kClearTransitionKey,static_cast<int32_t>(kClearTransitionStyles[clearStyleIndex]));
	}
	if(ImGui::Button("Preview Clear Transition")){
		sceneManager_->StartTransition(nullptr,clearTransitionStyle);
	}

	// スタート台からステージのレールへ移るときの出発の演出を切り替える(次に出発ゲートが開いたときから使われる)
	int departureStyleIndex = static_cast<int>(GetDepartureStyle());
	if(ImGui::Combo("Departure",&departureStyleIndex,DepartureDirector::kStyleNames,IM_ARRAYSIZE(DepartureDirector::kStyleNames))){
		gv->SetValue(kGameSceneGroup,kDepartureStyleKey,static_cast<int32_t>(departureStyleIndex));
	}

	// カメラ設定のUI
	if(ImGui::CollapsingHeader("Camera Settings")){
		Vector3 camPos = activeCamera->GetTranslate();
		if(ImGui::DragFloat3("Camera Pos",&camPos.x,0.1f)){
			activeCamera->SetTranslate(camPos);
		}

		Vector3 camRot = activeCamera->GetRotate();
		if(ImGui::DragFloat3("Camera Rotate",&camRot.x,0.01f)){
			activeCamera->SetRotate(camRot);
		}
	}

	// ライティング設定のUI
	if(ImGui::CollapsingHeader("Lighting")){
		if(PointLight* pData = object3dCommon_->GetPointLightData()){
			ImGui::Text("Point Light");
			ImGui::ColorEdit4("Point Color",&pData->color.x);
			ImGui::DragFloat3("Point Pos",&pData->position.x,0.1f);
			ImGui::DragFloat("Point Intensity",&pData->intensity,0.1f,0.0f,100.0f);
		}
		if(SpotLight* sData = object3dCommon_->GetSpotLightData()){
			ImGui::Text("Spot Light");
			ImGui::ColorEdit4("Spot Color",&sData->color.x);
		}
	}

	ModelManager::GetInstance()->UpdateLightGui();
	ImGui::End();

	Application::GetInstance()->ShowPostProcessUI();

	// シーン階層(Hierarchy)の最小版(左・上段に配置)
	EditorWidgets::BeginFixedPanel("Hierarchy",L.hierarchy);
	targetManager_->ShowHierarchy();
	ImGui::End();

	// Inspectorの最小版(右・上段に配置)
	EditorWidgets::BeginFixedPanel("Inspector",L.inspector);
	targetManager_->ShowInspector();
	ImGui::End();
}
#endif

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
	player_->Draw();

	// タイトルロゴを描画(タイトル中・カメラの回り込み中のみ)
	if(startSequence_){
		startSequence_->Draw3D();
	}

	// 雑魚敵を描画(撃破済みのときはEnemy側で描画をスキップする)
	enemyManager_->Draw();

	// 生存している的だけ描画
	targetManager_->Draw();

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

	// スピード線はレティクル・画面表示より奥に見えるよう先に描く
	speedLines_->Draw();

	if(!isTitleShowing && !isDeparting){
		reticle_->Draw();
	}

	// 体力・壊した的の数・連鎖数はPlay中だけ表示する(タイトル中・カメラの回り込み中は出さない)
	if(EditorContext::GetInstance()->IsPlayMode() && !isTitleShowing){
		stageHUD_->Draw();
	}

	// 開始演出のタイトル表示(PRESS SPACE)と、出発までの表示(操作案内・GO!)を最前面に描画する
	if(startSequence_){
		startSequence_->Draw2D();
	}

	// 操作説明(閉じている間は左下の案内)は、タイトル表示よりさらに手前に描画する
	helpOverlay_->Draw();
}
