#include "StartSequenceState.h"
#include "Title/StartSequenceInternal.h"
#include "Title/TitleLogo.h"
#include "Title/TitleUI.h"
#include "Title/StageIntroUI.h"
#include "objects/StartPlatform.h"
#include "Input.h"
#include "SoundManager.h"

using namespace StartSequenceInternal;

// -------------------------------------------------
// すべての段階で共通
// -------------------------------------------------

// 出発までの表示(操作案内・カウントダウン・GO!)を描く
void StartSequence::State::Draw2D(StartSequence& owner){
	if(owner.introUI_){
		owner.introUI_->Draw();
	}
}

// -------------------------------------------------
// 演出を行っていない段階
// -------------------------------------------------

// Edit 中は何も進めない
std::unique_ptr<StartSequence::State> StartSequence::IdleState::Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition){
	(void)owner;
	(void)deltaTime;
	(void)playerPosition;
	return nullptr;
}

// Edit 中は出発までの表示も出さない
void StartSequence::IdleState::Draw2D(StartSequence& owner){
	(void)owner;
}

// -------------------------------------------------
// タイトル表示中
// -------------------------------------------------

// タイトルロゴ・タイトル表示を初めから出し直し、タイトルBGMを最初からループ再生する
void StartSequence::TitleState::Enter(StartSequence& owner){
	if(owner.titleLogo_){
		owner.titleLogo_->Reset();
	}
	if(owner.titleUI_){
		owner.titleUI_->Reset();
	}
	SoundManager::GetInstance()->PlayAudio(kTitleBgmPath,GetVolume(kBgmVolumeKey),true);
}

// カメラをゆっくり揺らしながら、SPACE の入力を待つ
std::unique_ptr<StartSequence::State> StartSequence::TitleState::Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition){
	(void)playerPosition;
	owner.cameraDirector_.UpdateIdle(deltaTime);
	// ImGuiで変更した音量をすぐ反映させる
	SoundManager::GetInstance()->SetVolume(kTitleBgmPath,GetVolume(kBgmVolumeKey));
	if(owner.titleUI_){
		owner.titleUI_->Update(deltaTime,kTitleVisible);
	}
	// タイトルロゴの登場演出と、着地後の揺れを進める
	if(owner.titleLogo_){
		owner.titleLogo_->UpdateAppear(deltaTime);
		owner.titleLogo_->UpdateIdle(deltaTime);
	}
	// プレイヤーが跳ねる周期を進める
	owner.playerHopTimer_ += deltaTime;

	// 登場演出の途中で消える演出と重ならないよう、ロゴが着地してから入力を受け付ける
	if((!owner.titleLogo_ || owner.titleLogo_->IsAppearFinished()) && owner.input_ && owner.input_->TriggerKey(DIK_SPACE)){
		return std::make_unique<CameraTurnState>();
	}
	return nullptr;
}

// タイトルロゴを描く
void StartSequence::TitleState::Draw3D(StartSequence& owner){
	if(owner.titleLogo_){
		owner.titleLogo_->Draw();
	}
}

// タイトル表示(PRESS SPACE)を描いてから、出発までの表示を重ねる
void StartSequence::TitleState::Draw2D(StartSequence& owner){
	if(owner.titleUI_){
		owner.titleUI_->Draw();
	}
	State::Draw2D(owner);
}

// -------------------------------------------------
// カメラの回り込み中
// -------------------------------------------------

// プレイヤーの背後へ回り込み始め、同時にタイトルロゴを縮めて消し始めて、回り込むカメラにかぶらないようにする
void StartSequence::CameraTurnState::Enter(StartSequence& owner){
	owner.cameraDirector_.BeginTurn();
	if(owner.titleLogo_){
		owner.titleLogo_->BeginHide();
	}
	// 決定音を鳴らす
	SoundManager::GetInstance()->PlayAudio(kDecideSePath,GetVolume(kSeVolumeKey));
}

// 回り込みの進行に合わせてタイトル表示とBGMを薄くしていき、回り込みが終わったらスタート台の上を動けるようにする
std::unique_ptr<StartSequence::State> StartSequence::CameraTurnState::Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition){
	(void)playerPosition;
	owner.cameraDirector_.UpdateTurn(deltaTime);
	// スタート時に跳ねている途中なら、着地するまでは跳ねを進める(着地後は高さ0のまま止まる)
	if(owner.GetPlayerHopHeight() > 0.0f){
		owner.playerHopTimer_ += deltaTime;
	}
	if(owner.titleLogo_){
		owner.titleLogo_->UpdateHide(deltaTime);
	}
	if(owner.titleUI_){
		owner.titleUI_->Update(deltaTime,kTitleVisible - owner.cameraDirector_.GetTurnProgress());
	}
	// タイトル表示と同じく、回り込みの進行に合わせてBGMを小さくしていく
	SoundManager::GetInstance()->SetVolume(kTitleBgmPath,GetVolume(kBgmVolumeKey) * (kTitleVisible - owner.cameraDirector_.GetTurnProgress()));

	if(owner.cameraDirector_.IsTurnFinished()){
		// 回り込みが終わったらBGMを止め、スタート台の上を動けるようにする
		SoundManager::GetInstance()->StopAudio(kTitleBgmPath);
		return std::make_unique<StandbyState>();
	}
	return nullptr;
}

// 消えていく途中のタイトルロゴを描く
void StartSequence::CameraTurnState::Draw3D(StartSequence& owner){
	if(owner.titleLogo_){
		owner.titleLogo_->Draw();
	}
}

// 薄くなっていくタイトル表示を描いてから、出発までの表示を重ねる
void StartSequence::CameraTurnState::Draw2D(StartSequence& owner){
	if(owner.titleUI_){
		owner.titleUI_->Draw();
	}
	State::Draw2D(owner);
}

// -------------------------------------------------
// スタート台の上で準備中
// -------------------------------------------------

// どれかの出発ゲートの前の目印に立っている間だけ操作案内を出し、SPACE でそのゲートから出発する
// (この段階ではジャンプより先に判定し、押した瞬間に GateOpenState へ移るため、同じ SPACE でジャンプはしない)
std::unique_ptr<StartSequence::State> StartSequence::StandbyState::Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition){
	(void)deltaTime;
	const int gateIndex = owner.startPlatform_?owner.startPlatform_->FindGateArea(playerPosition):-1;
	const bool isAtGate = gateIndex >= 0;
	if(owner.introUI_){
		owner.introUI_->SetPromptVisible(isAtGate);
	}
	if(isAtGate && owner.input_ && owner.input_->TriggerKey(DIK_SPACE)){
		return std::make_unique<GateOpenState>(gateIndex);
	}
	return nullptr;
}

// -------------------------------------------------
// 出発ゲートを開けている最中
// -------------------------------------------------

// 目印から柱へインクを流し込んでゲートを開け始め、操作案内は消す
void StartSequence::GateOpenState::Enter(StartSequence& owner){
	owner.startPlatform_->BeginOpenGate(gateIndex_);
	if(owner.introUI_){
		owner.introUI_->SetPromptVisible(false);
	}
	SoundManager::GetInstance()->PlayAudio(kDecideSePath,GetVolume(kSeVolumeKey));
	// カメラをゲートの斜め前へ寄せ、インクが塗り上がる様子を大きく見せる
	owner.gateCameraDirector_.BeginShot();
	// このフレームで開け始めたことを GameScene へ知らせる
	owner.isGateOpenTriggered_ = true;
}

// ゲートが開ききったら、レールへ歩いて乗れる段階へ進む
std::unique_ptr<StartSequence::State> StartSequence::GateOpenState::Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition){
	(void)deltaTime;
	(void)playerPosition;
	if(!owner.startPlatform_ || owner.startPlatform_->IsGateOpened()){
		return std::make_unique<BoardingState>();
	}
	return nullptr;
}

// 開けている最中に出発の演出が始まったときは、演出のカメラに任せるため、ゲートを映すカメラを止める
std::unique_ptr<StartSequence::State> StartSequence::GateOpenState::OnDepartureBegan(StartSequence& owner){
	owner.gateCameraDirector_.Reset();
	return std::make_unique<DepartureState>();
}

// -------------------------------------------------
// レールへの乗り込み待ち
// -------------------------------------------------

// ゲートを映すカメラをプレイ用カメラへ戻し始める
void StartSequence::BoardingState::Enter(StartSequence& owner){
	owner.gateCameraDirector_.BeginReturn();
}

// プレイヤーがレールに乗ったら、GameScene から NotifyBoarded() で知らせてもらう
std::unique_ptr<StartSequence::State> StartSequence::BoardingState::Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition){
	(void)owner;
	(void)deltaTime;
	(void)playerPosition;
	return nullptr;
}

// 自分で歩いてレールへ乗り込んだら「GO!」を出してプレイを始める
std::unique_ptr<StartSequence::State> StartSequence::BoardingState::OnBoarded(StartSequence& owner){
	owner.BeginPlayWithGo();
	return std::make_unique<PlayingState>();
}

// 出発台から出発の演出が始まったら、演出が終わるまでプレイヤーを動けなくする
std::unique_ptr<StartSequence::State> StartSequence::BoardingState::OnDepartureBegan(StartSequence& owner){
	(void)owner;
	return std::make_unique<DepartureState>();
}

// -------------------------------------------------
// 出発の演出中
// -------------------------------------------------

// 出発の演出がレールへ乗せ終えたら、GameScene から NotifyBoarded() で知らせてもらう
std::unique_ptr<StartSequence::State> StartSequence::DepartureState::Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition){
	(void)owner;
	(void)deltaTime;
	(void)playerPosition;
	return nullptr;
}

// 演出がレールへ乗せ終えたら「GO!」を出してプレイを始める
std::unique_ptr<StartSequence::State> StartSequence::DepartureState::OnBoarded(StartSequence& owner){
	owner.BeginPlayWithGo();
	return std::make_unique<PlayingState>();
}

// -------------------------------------------------
// プレイ中
// -------------------------------------------------

// 開始演出は終わっているため何も進めない
std::unique_ptr<StartSequence::State> StartSequence::PlayingState::Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition){
	(void)owner;
	(void)deltaTime;
	(void)playerPosition;
	return nullptr;
}
