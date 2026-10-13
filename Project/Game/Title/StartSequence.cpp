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
	const std::string kDecideSePath = "resource/music/se/decide.mp3";	// GlobalVariablesのグループ名(タイトル演出の調整項目)
	const char* kTitleGroup = "Title";
	// BGM・SEの音量の調整項目名
	const char* kBgmVolumeKey = "bgmVolume";
	const char* kSeVolumeKey = "seVolume";
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

	// 実際に聞きながら音量を詰められるよう、調整項目として登録する
	// (ImGuiの "Global Variables" パネル、または resource/GlobalVariables/Title.json の書き換えで変更できる)
	GlobalVariables* gv = GlobalVariables::GetInstance();
	gv->CreateGroup(kTitleGroup);
	gv->AddItem(kTitleGroup,kBgmVolumeKey,kDefaultBgmVolume);
	gv->AddItem(kTitleGroup,kSeVolumeKey,kDefaultSeVolume);
}

// タイトルから始める
void StartSequence::Start(bool skipTitle){
	playerHopTimer_ = 0.0f;
	cameraDirector_.Reset();
	if(introUI_){
		introUI_->Reset();
	}
	// 出発ゲートを閉じた状態から始める
	if(startPlatform_){
		startPlatform_->ResetGate();
	}

	// タイトル演出を飛ばす場合は、BGMも鳴らさずにそのままプレイ中にする
	// 出発ゲートも開けておき、台の上からすぐレールへ歩いて乗れるようにする
	if(skipTitle){
		if(startPlatform_){
			startPlatform_->BeginOpenGate();
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
	// 出発までの表示(操作案内・ステージ情報・GO!)は、どの段階でも出し入れの動きを進める
	if(introUI_ && phase_ != Phase::None){
		introUI_->Update(deltaTime);
	}

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
			// ゲートのランプを順に点けている間、これから遊ぶステージの情報を見せる
			startPlatform_->BeginOpenGate();
			if(introUI_){
				introUI_->SetPromptVisible(false);
				introUI_->ShowStageCard();
			}
			SoundManager::GetInstance()->PlayAudio(kDecideSePath,GetVolume(kSeVolumeKey));
			phase_ = Phase::GateOpen;
		}
		break;
	}

	case Phase::GateOpen:
		// ゲートが開ききったらステージ情報を引っ込め、レールへ歩いて乗れるようにする
		if(!startPlatform_ || startPlatform_->IsGateOpened()){
			if(introUI_){
				introUI_->HideStageCard();
			}
			phase_ = Phase::Boarding;
		}
		break;

	case Phase::Boarding:
		// プレイヤーがレールに乗ったら、GameScene から NotifyBoarded() で知らせてもらう
		break;

	case Phase::Playing:
	case Phase::None:
	default:
		break;
	}
}

// プレイヤーがレールに乗り込んだことを伝える
void StartSequence::NotifyBoarded(){
	if(phase_ != Phase::Boarding){
		return;
	}
	if(introUI_){
		introUI_->BeginGo();
	}
	phase_ = Phase::Playing;
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
