#pragma once

#include <string>

/// <summary>
/// プレイヤーが背負っているインクタンク
/// メインの弾とサブウェポン(Q)は、同じタンクのインクを使う。足りないときはどちらも撃てない。
/// インクはレールに足が着いている間だけ戻り、撃っている間(左クリックを押している間)と、最後にインクを使ってから決めた時間は戻らない。
/// 容量・使う量・戻り方は GlobalVariables の調整項目にして、遊びながら変えられるようにする。
/// </summary>
class InkTank{
public:
	/// <summary>
	/// 調整項目の登録と、満タンの状態への設定
	/// </summary>
	/// <param name="paramGroup">調整項目を登録するGlobalVariablesのグループ名</param>
	void Initialize(const std::string& paramGroup);

	// 満タンに戻す(やり直したときに呼ぶ)
	void Reset();

	/// <summary>
	/// インクの戻りを進める
	/// </summary>
	/// <param name="canRegenerate">インクが戻る状況か(プレイ中で、レールに足が着いているとき true)</param>
	/// <param name="isUsing">撃っている最中か(左クリックを押している間は true。押している間はインクを戻さない)</param>
	/// <param name="deltaTime">経過時間(秒)</param>
	void Update(bool canRegenerate,bool isUsing,float deltaTime);

	// 指定した量のインクが残っているか
	bool CanAfford(float amount) const{ return ink_ >= amount; }

	/// <summary>
	/// 指定した量のインクを使う(足りないときは使わない)
	/// 使ったときは、インクが戻り始めるまでの時間を最初からにする
	/// </summary>
	/// <param name="amount">使う量</param>
	/// <returns>使えたとき true</returns>
	bool TryConsume(float amount);

	// 残りの割合を取得(0: 空 〜 1: 満タン。ゲージの表示に使う)
	float GetRate() const;
	// メインの弾1発で使う量を取得
	float GetMainShotCost() const;
	// サブウェポン1回で使う量を取得
	float GetHomingShotCost() const;

private:
	// 調整項目を登録したGlobalVariablesのグループ名
	std::string paramGroup_;
	// 今のインクの量
	float ink_ = kDefaultCapacity;
	// 最後にインクを使ってから(撃っている間は押し続けている間も含めて)の経過時間(秒)
	float regenTimer_ = 0.0f;

	// 容量の初期値(保存済みJSONがあればそちらが優先される)
	static constexpr float kDefaultCapacity = 100.0f;
	// 容量として設定できる下限(0以下にすると割合を求められないため)
	static constexpr float kMinCapacity = 1.0f;
	// メインの弾1発で使う量の初期値(連射の間隔 0.1 秒で撃ち続けると、満タンから 5 秒で切れる)
	static constexpr float kDefaultMainShotCost = 2.0f;
	// サブウェポン1回で使う量の初期値(満タンから2回撃てる)
	static constexpr float kDefaultHomingShotCost = 35.0f;
	// 最後にインクを使ってから戻り始めるまでの時間の初期値(秒)
	static constexpr float kDefaultRegenDelay = 0.5f;
	// 1秒あたりに戻る量の初期値(空から満タンまで 2 秒)
	static constexpr float kDefaultRegenSpeed = 50.0f;
};
