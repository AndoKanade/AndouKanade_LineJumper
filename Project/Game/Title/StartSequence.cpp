#include "StartSequence.h"
#include "Title/StartSequenceState.h"
#include "Title/StartSequenceInternal.h"
#include "Title/TitleLogo.h"
#include "Title/TitleUI.h"
#include "Title/StageIntroUI.h"
#include "objects/StartPlatform.h"
#include "Input.h"
#include "SoundManager.h"
#include "GlobalVariables.h"
#include <cmath>
#include <utility>

using namespace StartSequenceInternal;

// 最初は演出を行っていない段階(Edit 中)から始める
StartSequence::StartSequence() : state_(std::make_unique<IdleState>()){}
StartSequence::~StartSequence() = default;

// 初期化
void StartSequence::Initialize(Obj3dCommon* object3dCommon,SpriteCommon* spriteCommon,Input* input,StartPlatform* startPlatform){
	input_ = input;
	startPlatform_ = startPlatform;

	titleLogo_ = std::make_unique<TitleLogo>();
	titleLogo_->Initialize(object3dCommon);

	titleUI_ = std::make_unique<TitleUI>();
	titleUI_->Initialize(spriteCommon);

	introUI_ = std::make_unique<StageIntroUI>();
	introUI_->Initialize(spriteCommon);

	// BGM・SEの読み込み
	SoundManager::GetInstance()->SoundLoadFile(kTitleBgmPath);
	SoundManager::GetInstance()->SoundLoadFile(kDecideSePath);
	SoundManager::GetInstance()->SoundLoadFile(kCountdownBeepSePath);
	SoundManager::GetInstance()->SoundLoadFile(kCountdownGoSePath);

	// 実際に聞きながら音量を詰められるよう、調整項目として登録する
	// (ImGuiの "Global Variables" パネル、または resource/GlobalVariables/Title.json の書き換えで変更できる)
	GlobalVariables* gv = GlobalVariables::GetInstance();
	gv->CreateGroup(kTitleGroup);
	gv->AddItem(kTitleGroup,kBgmVolumeKey,kDefaultBgmVolume);
	gv->AddItem(kTitleGroup,kSeVolumeKey,kDefaultSeVolume);
	// ゲートを映す画は、実際に見ながら構図を詰められるよう調整項目にする
	gv->AddItem(kTitleGroup,kGateCameraPositionKey,kDefaultGateCameraPosition);
	gv->AddItem(kTitleGroup,kGateCameraTargetKey,kDefaultGateCameraTarget);
}

// Start()・StartFromStandby()で共通の、始める前の状態に戻す処理
void StartSequence::ResetForStart(){
	playerHopTimer_ = 0.0f;
	lastCountdownNumber_ = 0;
	cameraDirector_.Reset();
	gateCameraDirector_.Reset();
	if(introUI_){
		introUI_->Reset();
	}
	// 出発ゲートを閉じた状態から始める
	// 前回の出発で台に塗ったインクも消す(落下リスタートでは Start() を呼ばないため、インクは残る)
	if(startPlatform_){
		startPlatform_->ResetGate();
		startPlatform_->ClearInk();
	}
}

// タイトルから始める
void StartSequence::Start(bool skipTitle){
	ResetForStart();

	// タイトル演出を飛ばす場合は、BGMも鳴らさずにそのままプレイ中にする
	// プレイヤーはレールの始点から始めるため、カウントダウンやゲートが開く演出は出さず、ゲートは開ききった状態にしておく
	// (閉じたままだと、レールから落ちたときにゲートより手前へ押し戻され、地面まで落ちずにやり直しにならない)
	if(skipTitle){
		if(startPlatform_){
			startPlatform_->OpenGateImmediately();
		}
		ChangeState(std::make_unique<PlayingState>());
		return;
	}

	// タイトルロゴ・タイトル表示の出し直しとタイトルBGMの再生は、タイトルの段階に入った瞬間に行う
	ChangeState(std::make_unique<TitleState>());
}

// タイトル・カメラの回り込みを飛ばして、スタート台の上を動ける段階から始める
void StartSequence::StartFromStandby(){
	ResetForStart();
	ChangeState(std::make_unique<StandbyState>());
}

// 演出を止める
void StartSequence::Stop(){
	ChangeState(std::make_unique<IdleState>());
	gateCameraDirector_.Reset();
	if(introUI_){
		introUI_->Reset();
	}
	if(startPlatform_){
		startPlatform_->ResetGate();
	}
	SoundManager::GetInstance()->StopAudio(kTitleBgmPath);
}

// 更新処理
void StartSequence::Update(float deltaTime,const Vector3& playerPosition){
	// このフレームで出発ゲートを開け始めたかは、毎フレーム調べ直す
	isGateOpenTriggered_ = false;

	// 出発までの表示(操作案内・カウントダウン・GO!)は、どの段階でも出し入れの動きを進める
	// カウントダウンの数は、ゲートのランプが点くのに合わせてスタート台から受け取る
	if(introUI_ && state_->IsRunning()){
		if(startPlatform_){
			const int countdownNumber = startPlatform_->GetCountdownNumber();
			// 数が 3・2・1 に変わった瞬間に、数字の表示と合わせて合図の音を鳴らす(0 になったときは鳴らさない)
			if(countdownNumber != lastCountdownNumber_ && countdownNumber > 0){
				SoundManager::GetInstance()->PlayAudio(kCountdownBeepSePath,GetVolume(kSeVolumeKey));
			}
			lastCountdownNumber_ = countdownNumber;
			introUI_->SetCountdown(countdownNumber);
		}
		introUI_->Update(deltaTime);
	}
	// ゲートを映すカメラは、レールに乗り込んだ後も戻りきるまで動かす
	gateCameraDirector_.Update(deltaTime);

	// 段階ごとの処理は今の段階に任せ、次の段階が返ってきたら切り替える
	ChangeState(state_->Update(*this,deltaTime,playerPosition));
}

// 段階を切り替え、新しい段階の始まりの処理を呼ぶ
void StartSequence::ChangeState(std::unique_ptr<State> nextState){
	if(!nextState){
		return;
	}
	state_ = std::move(nextState);
	state_->Enter(*this);
}

// プレイヤーがレールに乗り込んだことを伝える(乗り込みを待っている段階・出発の演出中だけが受け付ける)
void StartSequence::NotifyBoarded(){
	ChangeState(state_->OnBoarded(*this));
}

// 「GO!」を出し、合わせて効果音を鳴らす
void StartSequence::BeginPlayWithGo(){
	if(introUI_){
		introUI_->BeginGo();
	}
	// 「GO!」の表示と合わせて鳴らす
	SoundManager::GetInstance()->PlayAudio(kCountdownGoSePath,GetVolume(kSeVolumeKey));
}

// 効果音の音量(調整項目の Title グループの seVolume)
float StartSequence::GetSeVolume(){
	return GetVolume(kSeVolumeKey);
}

// 出発の演出を始めたことを伝える(乗り込みを待っている段階・ゲートを開けている最中だけが受け付ける)
void StartSequence::BeginDeparture(){
	ChangeState(state_->OnDepartureBegan(*this));
}

// 段階ごとの問い合わせへの答えは、今の段階に任せる
bool StartSequence::IsOpeningGate() const{ return state_->IsOpeningGate(); }
bool StartSequence::IsDeparting() const{ return state_->IsDeparting(); }
bool StartSequence::IsPlayable() const{ return state_->IsPlayable(); }
bool StartSequence::IsControllingCamera() const{ return state_->IsControllingCamera(); }
bool StartSequence::IsPlayerMovable() const{ return state_->IsPlayerMovable(); }
bool StartSequence::IsBoarding() const{ return state_->IsBoarding(); }
bool StartSequence::IsIntroRunning() const{ return state_->IsIntroRunning(); }

// タイトルロゴを配置する
void StartSequence::UpdateLogo(const Vector3& pivot,const Vector3& gameplayRotate){
	if(titleLogo_){
		titleLogo_->Update(pivot,gameplayRotate);
	}
}

// 3D描画処理(タイトルロゴはタイトル中と回り込み中の段階だけが描画する)
void StartSequence::Draw3D(){
	state_->Draw3D(*this);
}

// 2D描画処理(タイトル表示はタイトル中と回り込み中の段階だけ、出発までの表示は Edit 中以外の段階で描画する)
void StartSequence::Draw2D(){
	state_->Draw2D(*this);
}

// タイトル中にプレイヤーをその場で跳ねさせる高さ
float StartSequence::GetPlayerHopHeight() const{
	if(!IsControllingCamera()){
		return 0.0f;
	}

	// 周期の中での経過時間を求め、周期の最後の kPlayerHopDuration 秒間だけ跳ねさせる
	float cycleTime = std::fmod(playerHopTimer_,kPlayerHopInterval);
	float hopStartTime = kPlayerHopInterval - kPlayerHopDuration;
	if(cycleTime < hopStartTime){
		return 0.0f;
	}

	// 跳ねている間の進行度(0〜1)をsinの半周期に当てはめ、山なりに上がって下りる高さにする
	float hopProgress = (cycleTime - hopStartTime) / kPlayerHopDuration;
	return kPlayerHopHeight * std::sin(hopProgress * kPi);
}

// 演出中のカメラ座標・向きを求める
void StartSequence::CalculateCamera(const Vector3& pivot,const Vector3& gameplayPosition,const Vector3& gameplayRotate,Vector3& outPosition,Vector3& outRotate) const{
	cameraDirector_.Calculate(pivot,gameplayPosition,gameplayRotate,outPosition,outRotate);
}

// 出発ゲートを開ける様子を映すカメラの座標・向きを求める
// 調整項目のゲートから見た座標を、台の置き方に合わせてワールド座標に直してから使う
CameraPose StartSequence::CalculateGateCamera(const CameraPose& gameplayCamera) const{
	if(!startPlatform_){
		return gameplayCamera;
	}
	GlobalVariables* gv = GlobalVariables::GetInstance();
	Vector3 shotPosition = startPlatform_->GateLocalToWorld(gv->GetVector3Value(kTitleGroup,kGateCameraPositionKey));
	Vector3 shotTarget = startPlatform_->GateLocalToWorld(gv->GetVector3Value(kTitleGroup,kGateCameraTargetKey));
	return gateCameraDirector_.Calculate(shotPosition,shotTarget,gameplayCamera);
}
