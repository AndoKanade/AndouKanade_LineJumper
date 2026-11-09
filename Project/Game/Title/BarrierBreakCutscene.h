#pragma once

#include "MyMath.h"
#include "Camera/CameraPose.h"
#include <string>

// 前方宣言
class StartPlatform;

/// <summary>
/// 最後のステージ以外のステージをクリアし、大ジャンプでスタート台に着地した後に、そのステージの通路の壁を壊して見せる演出
/// プレイ用カメラから壁を映す画へ回し(回す)、少し間を置いて壁を壊し(壊す)、プレイ用カメラへ戻す(戻す)。
/// 壁が壊れる瞬間は ConsumeBroken() で GameScene に知らせ、壁を消して破片を飛ばしてもらう。
/// 演出の間はプレイヤーを動かさず、SPACE で飛ばせる(飛ばしたときも、まだ壊していなければ壊れたことを知らせる)。
/// 各段階の長さは調整項目(barrierTurnDuration / barrierBreakDuration / barrierReturnDuration)で変えられる。
/// </summary>
class BarrierBreakCutscene{
public:
	/// <summary>
	/// 初期化処理(各段階の長さを調整項目に登録する)
	/// </summary>
	/// <param name="paramGroup">調整項目を登録するGlobalVariablesのグループ名</param>
	void Initialize(const std::string& paramGroup);

	/// <summary>
	/// 演出を始める
	/// </summary>
	/// <param name="startPlatform">壁を置いているスタート台(壁を映す画の位置を求めるのに使う)</param>
	/// <param name="stageIndex">壊す壁のステージの番号</param>
	void Begin(const StartPlatform& startPlatform,int stageIndex);

	/// <summary>
	/// 演出を進める
	/// </summary>
	/// <param name="deltaTime">経過時間(秒。ヒットストップ中は0)</param>
	/// <param name="isSkipRequested">このフレームで飛ばす操作をしたか</param>
	void Update(float deltaTime,bool isSkipRequested);

	// 途中で止める(Editモードへ戻ったとき・やり直したときに呼ぶ。壊れたことは知らせない)
	void Reset();

	// 演出の最中か(最中はプレイヤーを動かさない)
	bool IsRunning() const{ return isRunning_; }

	// 壊す壁のステージの番号
	int GetStageIndex() const{ return stageIndex_; }

	/// <summary>
	/// 演出のカメラの位置と向きを取得する
	/// </summary>
	/// <param name="gameplayCamera">このフレームのプレイ用カメラの位置と向き(回し始めと戻しきったときはこれと一致する)</param>
	/// <param name="outPose">演出のカメラの位置と向き</param>
	/// <returns>演出の最中なら true(false ならプレイ用カメラのまま)</returns>
	bool GetCameraPose(const CameraPose& gameplayCamera,CameraPose& outPose) const;

	// このフレームで壁が壊れたかを受け取る(受け取ったら false に戻る)
	bool ConsumeBroken();

private:
	// 壁を映す画を混ぜる割合(0: プレイ用カメラ、1: 壁を映す画)
	float GetShotWeight() const;

	/// <summary>
	/// 経過時間を、長さ duration の段階の進み具合(0〜1)に直す(長さが0以下のときは、すぐ終わったものとして1を返す)
	/// </summary>
	static float StageRate(float elapsed,float duration);

	// 調整項目を登録したGlobalVariablesのグループ名
	std::string paramGroup_;

	// 演出の最中か
	bool isRunning_ = false;
	// 演出を始めてからの経過時間(秒)
	float timer_ = 0.0f;
	// 壊す壁のステージの番号
	int stageIndex_ = -1;
	// 壁をもう壊したか
	bool hasBroken_ = false;
	// 壁が壊れた知らせをまだ受け取られていないか
	bool isBrokenPending_ = false;
	// 壁を映す画(始めたときに求めておく)
	CameraPose shotPose_;

	// 各段階の長さを決める調整項目名
	static constexpr const char* kTurnDurationKey = "barrierTurnDuration";
	static constexpr const char* kBreakDurationKey = "barrierBreakDuration";
	static constexpr const char* kReturnDurationKey = "barrierReturnDuration";
	// 各段階の長さの初期値(秒。回す 0.5 秒 → 壊す 1 秒 → 戻す 0.5 秒の、全体で 2 秒)
	static constexpr float kDefaultTurnDuration = 0.5f;
	static constexpr float kDefaultBreakDuration = 1.0f;
	static constexpr float kDefaultReturnDuration = 0.5f;
	// 回しきってから壁を壊すまでの間(秒。壁が画面に収まったのを見せてから壊す。壊す段階より長いときは、壊す段階の終わりに壊す)
	static constexpr float kBreakDelay = 0.2f;
	// 壁を映す画のカメラの位置(台の上面の中央を原点にした、台の向きでの座標。x: 右、y: 上、z: 前)
	// 台の真ん中に着地したプレイヤーより少し後ろ(通路側)の右上から、通路の壁と、その先の最後のステージのゲートを見る
	static constexpr Vector3 kShotLocalPosition = {1.2f, 1.8f, -2.5f};
};
