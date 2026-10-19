#include "DepartureDirector.h"
#include "Title/Departure/BaseDeparture.h"
#include "Title/Departure/SuperJumpDeparture.h"
#include "Title/Departure/InkDiveDeparture.h"
#include "Title/Departure/CatapultDeparture.h"
#include "Title/Departure/CountdownFlipDeparture.h"

DepartureDirector::DepartureDirector() = default;
DepartureDirector::~DepartureDirector() = default;

// 初期化
// 演出を種類ごとに1つずつ生成しておき、切り替えるたびに作り直さないようにする
void DepartureDirector::Initialize(){
	departures_[static_cast<size_t>(DepartureStyle::SuperJump)] = std::make_unique<SuperJumpDeparture>();
	departures_[static_cast<size_t>(DepartureStyle::InkDive)] = std::make_unique<InkDiveDeparture>();
	departures_[static_cast<size_t>(DepartureStyle::Catapult)] = std::make_unique<CatapultDeparture>();
	departures_[static_cast<size_t>(DepartureStyle::CountdownFlip)] = std::make_unique<CountdownFlipDeparture>();
}

// 使う演出を決め、出発台の見た目を整える
void DepartureDirector::Prepare(DepartureStyle style,const DepartureContext& context){
	// 範囲外の番号(保存済みの JSON を書き換えた場合など)は、演出なしにする
	const size_t index = static_cast<size_t>(style);
	preparedDeparture_ = index < departures_.size()?departures_[index].get():nullptr;
	if(preparedDeparture_){
		preparedDeparture_->Prepare(context);
	}
}

// 出発台を使う演出か
bool DepartureDirector::UsesDeparturePad() const{
	return preparedDeparture_ && !preparedDeparture_->StartsWithGate();
}

// 出発ゲートを開け始めた瞬間に始める演出か(範囲外の番号と Walk は false)
bool DepartureDirector::StartsWithGate(DepartureStyle style) const{
	const size_t index = static_cast<size_t>(style);
	return index < departures_.size() && departures_[index] && departures_[index]->StartsWithGate();
}

// Prepare() で決めた演出を始める
void DepartureDirector::Begin(const DepartureContext& context,const CameraPose& startCamera){
	if(!preparedDeparture_){
		return;
	}
	runningDeparture_ = preparedDeparture_;
	isBoardedPending_ = false;
	runningDeparture_->Begin(context,startCamera);
}

// 演出を進める
void DepartureDirector::Update(float deltaTime,const CameraPose& gameplayCamera){
	if(!runningDeparture_){
		return;
	}
	runningDeparture_->Update(deltaTime,gameplayCamera);

	// 乗せ終えた知らせは、演出が同じフレームで終わっても受け取れるよう、ここで預かっておく
	Vector3 boardedPosition;
	if(runningDeparture_->ConsumeBoarded(boardedPosition)){
		isBoardedPending_ = true;
		boardedPosition_ = boardedPosition;
	}
	if(runningDeparture_->IsFinished()){
		runningDeparture_ = nullptr;
	}
}

// このフレームでレールに乗せ終えたかを受け取る
bool DepartureDirector::ConsumeBoarded(Vector3& outPosition){
	if(!isBoardedPending_){
		return false;
	}
	isBoardedPending_ = false;
	outPosition = boardedPosition_;
	return true;
}

// 演出のカメラの位置と向きを取得する
bool DepartureDirector::GetCameraPose(CameraPose& outPose) const{
	return runningDeparture_ && runningDeparture_->GetCameraPose(outPose);
}

// 進めている演出を止める
void DepartureDirector::Reset(){
	for(auto& departure : departures_){
		if(departure){
			departure->Reset();
		}
	}
	runningDeparture_ = nullptr;
	isBoardedPending_ = false;
}
