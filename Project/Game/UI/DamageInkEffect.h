#pragma once

/// <summary>
/// プレイヤーの体力を、画面の縁から迫る敵のインク(ポストプロセスの DamageInk)で表現する
/// 体力が減るほどインクが内側へ迫って景色の色が抜け、被弾した瞬間は画面全体を一瞬インクの色に寄せる。
/// 体力が残り少ないときは、鼓動のようにインクを脈打たせて危ないことを伝える。
/// </summary>
class DamageInkEffect{
public:
	// 体力の表現を消し、すぐにポストプロセスからも外す(シーンの終わり・Editモード用)
	void Reset();

	/// <summary>
	/// 体力からインクの広がり・色づきを求め、ポストプロセスに渡す
	/// </summary>
	/// <param name="hp">プレイヤーの今の体力</param>
	/// <param name="maxHp">プレイヤーの体力の最大値</param>
	/// <param name="isVisible">体力の表現を出すか(Play中だけtrue)</param>
	/// <param name="deltaTime">経過時間(秒)</param>
	void Update(int hp,int maxHp,bool isVisible,float deltaTime);

private:
	// 今表示しているインクの広がり(体力から求めた広がりへ少しずつ寄せ、インクがじわっと迫って見えるようにする)
	float displayedAmount_ = 0.0f;
	// 被弾した瞬間の画面の色づき(被弾で 1 にし、時間とともに 0 へ戻す)
	float flash_ = 0.0f;
	// 体力が残り少ない間の、脈打つ動きの経過時間(秒)
	float pulseTimer_ = 0.0f;
	// 前のフレームの体力(減った瞬間を被弾として検出する。-1: まだ受け取っていない)
	int previousHp_ = -1;

	// インクの広がりを体力から求めた広がりへ寄せる速さ(1秒あたりに残りの差を詰める割合)
	static constexpr float kFollowSpeed = 6.0f;
	// 被弾した瞬間の色づきが消えるまでの時間(秒)
	static constexpr float kFlashDuration = 0.4f;
	// この体力以下になったら、インクを脈打たせる
	static constexpr int kDangerHp = 1;
	// 脈打つときにインクを広げる最大の量と、1秒あたりの脈打つ回数
	static constexpr float kPulseAmplitude = 0.08f;
	static constexpr float kPulseFrequency = 1.2f;
	// これより小さい広がりは 0 とみなし、ポストプロセスから外す
	static constexpr float kMinVisibleAmount = 0.001f;
};
