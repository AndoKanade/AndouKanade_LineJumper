#pragma once
#include "Title/StartSequence.h"
#include <memory>

/// <summary>
/// ステージ開始までの流れ(StartSequence)の段階(状態)の基底クラス
/// StartSequence は今の段階を1つだけ持ち、段階ごとの更新・描画・外からの知らせ(レールに乗った・出発の演出を始めた)の受け付けを今の段階に任せる。
/// 次の段階へ進むかどうかは段階自身が判定し、進むときは次の段階を戻り値で返す(StartSequence が返ってきた段階へ切り替える)。
/// GameScene へ答える問い合わせ(プレイ可能か・動けるかなど)も段階ごとに答えを持つ。
/// </summary>
class StartSequence::State{
public:
	virtual ~State() = default;

	// この段階に入った瞬間に1回だけ呼ばれる(段階の始まりに行う演出の開始・効果音など)
	virtual void Enter(StartSequence& owner){ (void)owner; }

	/// <summary>
	/// 段階ごとの更新処理(入力の受付・演出の進行・次の段階へ進むかの判定)
	/// </summary>
	/// <param name="owner">この段階を持つ StartSequence</param>
	/// <param name="deltaTime">経過時間(秒)</param>
	/// <param name="playerPosition">プレイヤーの座標(出発ゲートの前にいるかの判定に使う)</param>
	/// <returns>次の段階へ進むときはその段階、この段階を続けるときは nullptr</returns>
	virtual std::unique_ptr<State> Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition) = 0;

	// プレイヤーがレールに乗り込んだと知らされたときの処理(進む段階を返す。受け付けない段階では nullptr)
	virtual std::unique_ptr<State> OnBoarded(StartSequence& owner){ (void)owner; return nullptr; }

	// 出発の演出を始めたと知らされたときの処理(進む段階を返す。受け付けない段階では nullptr)
	virtual std::unique_ptr<State> OnDepartureBegan(StartSequence& owner){ (void)owner; return nullptr; }

	// 3D描画処理(タイトルロゴを出す段階だけが描く)
	virtual void Draw3D(StartSequence& owner){ (void)owner; }

	// 2D描画処理(既定では出発までの表示だけを描く。タイトルを出す段階はタイトル表示も描く)
	virtual void Draw2D(StartSequence& owner);

	// 演出を行っているか(false の段階では、出発までの表示の更新と描画をしない)
	virtual bool IsRunning() const{ return true; }
	// プレイが始まる前の演出中か(タイトルからレールに乗り込むまで)
	virtual bool IsIntroRunning() const{ return true; }
	// レール進行・操作・射撃などのゲーム処理を動かしてよいか
	virtual bool IsPlayable() const{ return false; }
	// カメラを演出側で動かしている最中か
	virtual bool IsControllingCamera() const{ return false; }
	// スタート台の上を自由に動ける段階か
	virtual bool IsPlayerMovable() const{ return false; }
	// 出発ゲートが開き、レールへ乗り込むのを待っている段階か
	virtual bool IsBoarding() const{ return false; }
	// 出発ゲートを開けている最中か
	virtual bool IsOpeningGate() const{ return false; }
	// 出発の演出でレールへ向かっている最中か
	virtual bool IsDeparting() const{ return false; }
};

/// <summary>
/// 演出を行っていない段階(Editモード中)
/// </summary>
class StartSequence::IdleState : public StartSequence::State{
public:
	std::unique_ptr<State> Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition) override;
	void Draw2D(StartSequence& owner) override;
	bool IsRunning() const override{ return false; }
	bool IsIntroRunning() const override{ return false; }
};

/// <summary>
/// タイトル表示中(カメラをゆっくり揺らしながらスタート入力を待つ)。SPACE で CameraTurnState へ進む
/// </summary>
class StartSequence::TitleState : public StartSequence::State{
public:
	void Enter(StartSequence& owner) override;
	std::unique_ptr<State> Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition) override;
	void Draw3D(StartSequence& owner) override;
	void Draw2D(StartSequence& owner) override;
	bool IsControllingCamera() const override{ return true; }
};

/// <summary>
/// カメラがプレイヤーの背後へ回り込み中。回り込みが終わったら StandbyState へ進む
/// </summary>
class StartSequence::CameraTurnState : public StartSequence::State{
public:
	void Enter(StartSequence& owner) override;
	std::unique_ptr<State> Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition) override;
	void Draw3D(StartSequence& owner) override;
	void Draw2D(StartSequence& owner) override;
	bool IsControllingCamera() const override{ return true; }
};

/// <summary>
/// スタート台の上を自由に動ける(出発ゲートは閉じている)。ゲートの前の目印で SPACE を押すと GateOpenState へ進む
/// </summary>
class StartSequence::StandbyState : public StartSequence::State{
public:
	std::unique_ptr<State> Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition) override;
	bool IsPlayerMovable() const override{ return true; }
};

/// <summary>
/// 出発ゲートを開けている最中(プレイヤーは動けない)。ゲートが開ききったら BoardingState へ進む
/// ゲートのカウントダウンに合わせて飛び乗る演出では、開けている途中で出発の演出が始まり DepartureState へ進む
/// </summary>
class StartSequence::GateOpenState : public StartSequence::State{
public:
	/// <param name="gateIndex">開けるゲートの番号(StartPlatform のゲートの並び順)</param>
	explicit GateOpenState(int gateIndex) : gateIndex_(gateIndex){}

	void Enter(StartSequence& owner) override;
	std::unique_ptr<State> Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition) override;
	std::unique_ptr<State> OnDepartureBegan(StartSequence& owner) override;
	bool IsOpeningGate() const override{ return true; }

private:
	// 開けるゲートの番号
	int gateIndex_;
};

/// <summary>
/// 出発ゲートが開き、自分で歩いてレールへ乗り込む(または出発台へ向かう)のを待っている
/// レールに乗ったら PlayingState、出発の演出が始まったら DepartureState へ進む
/// </summary>
class StartSequence::BoardingState : public StartSequence::State{
public:
	void Enter(StartSequence& owner) override;
	std::unique_ptr<State> Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition) override;
	std::unique_ptr<State> OnBoarded(StartSequence& owner) override;
	std::unique_ptr<State> OnDepartureBegan(StartSequence& owner) override;
	bool IsPlayerMovable() const override{ return true; }
	bool IsBoarding() const override{ return true; }
};

/// <summary>
/// 出発の演出でレールへ向かっている最中(プレイヤーは動けない)。演出がレールへ乗せ終えたら PlayingState へ進む
/// </summary>
class StartSequence::DepartureState : public StartSequence::State{
public:
	std::unique_ptr<State> Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition) override;
	std::unique_ptr<State> OnBoarded(StartSequence& owner) override;
	bool IsDeparting() const override{ return true; }
};

/// <summary>
/// プレイ中(開始演出は終わっている)
/// </summary>
class StartSequence::PlayingState : public StartSequence::State{
public:
	std::unique_ptr<State> Update(StartSequence& owner,float deltaTime,const Vector3& playerPosition) override;
	bool IsIntroRunning() const override{ return false; }
	bool IsPlayable() const override{ return true; }
};
