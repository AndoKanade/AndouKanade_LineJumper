#include "InkTank.h"
#include "GlobalVariables.h"
#include <algorithm>

namespace{
	// 調整項目の名前
	constexpr const char* kCapacityKey = "inkCapacity";
	constexpr const char* kMainShotCostKey = "inkMainShotCost";
	constexpr const char* kHomingShotCostKey = "inkHomingShotCost";
	constexpr const char* kRegenDelayKey = "inkRegenDelay";
	constexpr const char* kRegenSpeedKey = "inkRegenSpeed";
}

// 調整項目の登録と、満タンの状態への設定
void InkTank::Initialize(const std::string& paramGroup){
	paramGroup_ = paramGroup;

	// 調整項目の登録(第3引数はデフォルト値。保存済みJSONがあればそちらが優先される)
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	globalVariables->AddItem(paramGroup_,kCapacityKey,kDefaultCapacity);
	globalVariables->AddItem(paramGroup_,kMainShotCostKey,kDefaultMainShotCost);
	globalVariables->AddItem(paramGroup_,kHomingShotCostKey,kDefaultHomingShotCost);
	globalVariables->AddItem(paramGroup_,kRegenDelayKey,kDefaultRegenDelay);
	globalVariables->AddItem(paramGroup_,kRegenSpeedKey,kDefaultRegenSpeed);

	Reset();
}

// 満タンに戻す
void InkTank::Reset(){
	ink_ = (std::max)(GlobalVariables::GetInstance()->GetFloatValue(paramGroup_,kCapacityKey),kMinCapacity);
	regenTimer_ = 0.0f;
}

// インクの戻りを進める
void InkTank::Update(bool canRegenerate,bool isUsing,float deltaTime){
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	const float capacity = (std::max)(globalVariables->GetFloatValue(paramGroup_,kCapacityKey),kMinCapacity);

	// 撃っている間は、インクが足りなくて撃てていなくても戻さない(指を離してから決めた時間がたつと戻り始める)
	if(isUsing){
		regenTimer_ = 0.0f;
	} else{
		regenTimer_ += deltaTime;
	}

	// レールに足が着いていて、最後に使ってから決めた時間がたっていれば、決めた速さで戻す
	if(canRegenerate && regenTimer_ >= globalVariables->GetFloatValue(paramGroup_,kRegenDelayKey)){
		ink_ += globalVariables->GetFloatValue(paramGroup_,kRegenSpeedKey) * deltaTime;
	}

	// 実行中に容量を小さくしたときも、容量を超えないようにする
	ink_ = std::clamp(ink_,0.0f,capacity);
}

// 指定した量のインクを使う(足りないときは使わない)
bool InkTank::TryConsume(float amount){
	if(!CanAfford(amount)){
		return false;
	}
	ink_ -= amount;
	regenTimer_ = 0.0f;
	return true;
}

// 残りの割合を取得
float InkTank::GetRate() const{
	const float capacity = (std::max)(GlobalVariables::GetInstance()->GetFloatValue(paramGroup_,kCapacityKey),kMinCapacity);
	return std::clamp(ink_ / capacity,0.0f,1.0f);
}

// メインの弾1発で使う量を取得
float InkTank::GetMainShotCost() const{
	return GlobalVariables::GetInstance()->GetFloatValue(paramGroup_,kMainShotCostKey);
}

// サブウェポン1回で使う量を取得
float InkTank::GetHomingShotCost() const{
	return GlobalVariables::GetInstance()->GetFloatValue(paramGroup_,kHomingShotCostKey);
}
