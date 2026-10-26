#include "StartSequence.h"
#include "Title/TitleLogo.h"
#include "Title/TitleUI.h"
#include "Title/StageIntroUI.h"
#include "objects/StartPlatform.h"
#include "Input.h"
#include "SoundManager.h"
#include "GlobalVariables.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace{
	// タイトル中に流すBGM
	const std::string kTitleBgmPath = "resource/music/bgm/nemui.mp3";
	// SPACE を押してスタートしたとき・出発ゲートを開けたときの決定音
	const std::string kDecideSePath = "resource/music/se/decide.mp3";
	// ゲートのカウントダウンの 3・2・1 で鳴らす音と、「GO!」を出すときに鳴らす音
	const std::string kCountdownBeepSePath = "resource/music/se/countdown_beep.mp3";
	const std::string kCountdownGoSePath = "resource/music/se/countdown_go.mp3";
	// GlobalVariablesのグループ名(タイトル演出の調整項目)
	const char* kTitleGroup = "Title";
	// BGM・SEの音量の調整項目名
	const char* kBgmVolumeKey = "bgmVolume";
	const char* kSeVolumeKey = "seVolume";
	// ゲートを映す画のカメラ座標・注目する座標の調整項目名
	const char* kGateCameraPositionKey = "gateCameraPosition";
	const char* kGateCameraTargetKey = "gateCameraTarget";
	// 音量の範囲(0=無音, 1=元の音量)
	constexpr float kMinVolume = 0.0f;
	constexpr float kMaxVolume = 1.0f;

	// 調整項目から音量を取得する(ImGuiで範囲外の値を入れられても音が割れないよう範囲内に収める)
	float GetVolume(const char* key){
		return std::clamp(GlobalVariables::GetInstance()->GetFloatValue(kTitleGroup,key),kMinVolume,kMaxVolume);
	}
}

StartSequence::StartSequence() = default;
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

// タイトルから始める
void StartSequence::Start(bool skipTitle){
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

	// タイトル演出を飛ばす場合は、BGMも鳴らさずにそのままプレイ中にする
	// プレイヤーはレールの始点から始めるため、カウントダウンやゲートが開く演出は出さず、ゲートは開ききった状態にしておく
	// (閉じたままだと、レールから落ちたときにゲートより手前へ押し戻され、地面まで落ちずにやり直しにならない)
	if(skipTitle){
		if(startPlatform_){
			startPlatform_->OpenGateImmediately();
		}
		phase_ = Phase::Playing;
		return;
	}

	phase_ = Phase::Title;
	if(titleLogo_){
		titleLogo_->Reset();
	}
	if(titleUI_){
		titleUI_->Reset();
	}
	// タイトルBGMを最初からループ再生する
	SoundManager::GetInstance()->PlayAudio(kTitleBgmPath,GetVolume(kBgmVolumeKey),true);
}

// 演出を止める
void StartSequence::Stop(){
	phase_ = Phase::None;
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
	if(introUI_ && phase_ != Phase::None){
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

	switch(phase_){
	case Phase::Title:
		// カメラをゆっくり揺らしながら、SPACE の入力を待つ
		cameraDirector_.UpdateIdle(deltaTime);
		// ImGuiで変更した音量をすぐ反映させる
		SoundManager::GetInstance()->SetVolume(kTitleBgmPath,GetVolume(kBgmVolumeKey));
		if(titleUI_){
			titleUI_->Update(deltaTime,kTitleVisible);
		}
		// タイトルロゴの登場演出と、着地後の揺れを進める
		if(titleLogo_){
			titleLogo_->UpdateAppear(deltaTime);
			titleLogo_->UpdateIdle(deltaTime);
		}
		// プレイヤーが跳ねる周期を進める
		playerHopTimer_ += deltaTime;
		// 登場演出の途中で消える演出と重ならないよう、ロゴが着地してから入力を受け付ける
		// スタートと同時にタイトルロゴを縮めて消し始め、回り込むカメラにかぶらないようにする
		if((!titleLogo_ || titleLogo_->IsAppearFinished()) && input_ && input_->TriggerKey(DIK_SPACE)){
			cameraDirector_.BeginTurn();
			if(titleLogo_){
				titleLogo_->BeginHide();
			}
			// 決定音を鳴らす
			SoundManager::GetInstance()->PlayAudio(kDecideSePath,GetVolume(kSeVolumeKey));
			phase_ = Phase::CameraTurn;
		}
		break;

	case Phase::CameraTurn:
		// 回り込みの進行に合わせてタイトル表示を薄くしていく
		cameraDirector_.UpdateTurn(deltaTime);
		// スタート時に跳ねている途中なら、着地するまでは跳ねを進める(着地後は高さ0のまま止まる)
		if(GetPlayerHopHeight() > 0.0f){
			playerHopTimer_ += deltaTime;
		}
		if(titleLogo_){
			titleLogo_->UpdateHide(deltaTime);
		}
		if(titleUI_){
			titleUI_->Update(deltaTime,kTitleVisible - cameraDirector_.GetTurnProgress());
		}
		// タイトル表示と同じく、回り込みの進行に合わせてBGMを小さくしていく
		SoundManager::GetInstance()->SetVolume(kTitleBgmPath,GetVolume(kBgmVolumeKey) * (kTitleVisible - cameraDirector_.GetTurnProgress()));
		if(cameraDirector_.IsTurnFinished()){
			// 回り込みが終わったらBGMを止め、スタート台の上を動けるようにする
			SoundManager::GetInstance()->StopAudio(kTitleBgmPath);
			phase_ = Phase::Standby;
		}
		break;

	case Phase::Standby:
	{
		// 出発ゲートの前の目印に立っている間だけ操作案内を出し、SPACE で出発する
		// (この段階ではジャンプより先に判定し、押した瞬間に GateOpen へ移るため、同じ SPACE でジャンプはしない)
		const bool isAtGate = startPlatform_ && startPlatform_->IsInGateArea(playerPosition);
		if(introUI_){
			introUI_->SetPromptVisible(isAtGate);
		}
		if(isAtGate && input_ && input_->TriggerKey(DIK_SPACE)){
			// 目印から柱へインクを流し込んでゲートを開け始め、操作案内は消す
			startPlatform_->BeginOpenGate();
			if(introUI_){
				introUI_->SetPromptVisible(false);
			}
			SoundManager::GetInstance()->PlayAudio(kDecideSePath,GetVolume(kSeVolumeKey));
			// カメラをゲートの斜め前へ寄せ、インクが塗り上がる様子を大きく見せる
			gateCameraDirector_.BeginShot();
			phase_ = Phase::GateOpen;
			isGateOpenTriggered_ = true;
		}
		break;
	}

	case Phase::GateOpen:
		// ゲートが開ききったら、カメラをプレイ用カメラへ戻し始め、レールへ歩いて乗れるようにする
		if(!startPlatform_ || startPlatform_->IsGateOpened()){
			gateCameraDirector_.BeginReturn();
			phase_ = Phase::Boarding;
		}
		break;

	case Phase::Boarding:
		// プレイヤーがレールに乗ったら、GameScene から NotifyBoarded() で知らせてもらう
		break;

	case Phase::Departure:
		// 出発の演出がレールへ乗せ終えたら、GameScene から NotifyBoarded() で知らせてもらう
		break;

	case Phase::Playing:
	case Phase::None:
	default:
		break;
	}
}

// プレイヤーがレールに乗り込んだことを伝える
void StartSequence::NotifyBoarded(){
	if(phase_ != Phase::Boarding && phase_ != Phase::Departure){
		return;
	}
	if(introUI_){
		introUI_->BeginGo();
	}
	// 「GO!」の表示と合わせて鳴らす
	SoundManager::GetInstance()->PlayAudio(kCountdownGoSePath,GetVolume(kSeVolumeKey));
	phase_ = Phase::Playing;
}

// 効果音の音量(調整項目の Title グループの seVolume)
float StartSequence::GetSeVolume(){
	return GetVolume(kSeVolumeKey);
}

// 出発の演出を始めたことを伝える
// ゲートを開けている最中に始めたときは、演出のカメラに任せるため、ゲートを映すカメラを止める
void StartSequence::BeginDeparture(){
	if(phase_ != Phase::Boarding && phase_ != Phase::GateOpen){
		return;
	}
	if(phase_ == Phase::GateOpen){
		gateCameraDirector_.Reset();
	}
	phase_ = Phase::Departure;
}

// タイトルロゴを配置する
void StartSequence::UpdateLogo(const Vector3& pivot,const Vector3& gameplayRotate){
	if(titleLogo_){
		titleLogo_->Update(pivot,gameplayRotate);
	}
}

// 3D描画処理(タイトルロゴはタイトル中と回り込み中だけ描画する)
void StartSequence::Draw3D(){
	if(titleLogo_ && IsControllingCamera()){
		titleLogo_->Draw();
	}
}

// 2D描画処理(タイトル表示はタイトル中と回り込み中だけ、出発までの表示は Edit 中以外で描画する)
void StartSequence::Draw2D(){
	if(titleUI_ && IsControllingCamera()){
		titleUI_->Draw();
	}
	if(introUI_ && phase_ != Phase::None){
		introUI_->Draw();
	}
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
