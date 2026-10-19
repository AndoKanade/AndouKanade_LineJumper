#pragma once

#include "MyMath.h"
#include "Camera/CameraPose.h"
#include "Title/Departure/DepartureContext.h"

/// <summary>
/// 出発の演出の基底クラス(スタート台からステージのレールへ移るときの演出)
/// 出発台の手前の線を越えたら(StartsWithGate() が true の演出は、出発ゲートを開け始めた瞬間に) Begin() が呼ばれ、
/// 毎フレーム Update() で、プレイヤーの表示とカメラを動かす。
/// レールに乗せて「GO!」を出す瞬間に NotifyBoarded() を呼ぶと、GameScene が開始演出を進めてプレイを始める。
///
/// 新しい演出を足すときは、このクラスを継承して Begin() / Update() / IsFinished() / GetCameraPose() を作り、
/// DepartureDirector の DepartureStyle と演出の一覧に登録する。
/// </summary>
class BaseDeparture{
public:
	virtual ~BaseDeparture() = default;

	/// <summary>
	/// 出発ゲートが開いた瞬間の準備(出発台の見た目を整えるなど。何もしない演出もある)
	/// </summary>
	/// <param name="context">演出が動かす・参照するもの</param>
	virtual void Prepare(const DepartureContext& context){ (void)context; }

	// 出発ゲートを開け始めた瞬間に始める演出か(false なら、ゲートが開いた後に出発台の手前の線を越えたら始める)
	virtual bool StartsWithGate() const{ return false; }

	/// <summary>
	/// 出発の演出を始める
	/// </summary>
	/// <param name="context">演出が動かす・参照するもの</param>
	/// <param name="startCamera">始めた瞬間に映っているカメラの位置と向き(ここから演出のカメラへ動かす)</param>
	virtual void Begin(const DepartureContext& context,const CameraPose& startCamera) = 0;

	/// <summary>
	/// 演出を進める(プレイヤーの表示を上書きし、カメラの位置と向きを決める)
	/// </summary>
	/// <param name="deltaTime">経過時間(秒。ヒットストップ中は0)</param>
	/// <param name="gameplayCamera">このフレームのプレイ用カメラの位置と向き(最後はここへ戻す)</param>
	virtual void Update(float deltaTime,const CameraPose& gameplayCamera) = 0;

	// 演出がすべて終わったか(終わったら DepartureDirector が止める)
	virtual bool IsFinished() const = 0;

	/// <summary>
	/// 演出のカメラの位置と向きを取得する
	/// </summary>
	/// <param name="outPose">演出のカメラの位置と向き</param>
	/// <returns>カメラを演出側で動かしている最中なら true(false ならプレイ用カメラのまま)</returns>
	virtual bool GetCameraPose(CameraPose& outPose) const = 0;

	// 途中で止める(視野角など、演出で変えたものを元に戻す)
	virtual void Reset(){}

	/// <summary>
	/// このフレームでレールに乗せ終え、「GO!」を出す瞬間を迎えたかを受け取る(受け取ったら false に戻る)
	/// </summary>
	/// <param name="outPosition">乗せた位置</param>
	bool ConsumeBoarded(Vector3& outPosition){
		if(!isBoardedPending_){
			return false;
		}
		isBoardedPending_ = false;
		outPosition = boardedPosition_;
		return true;
	}

protected:
	/// <summary>
	/// 値が start から end へ進む間を 0〜1 に直す(範囲の外は 0 か 1 に収める。演出の段階ごとの進み具合に使う)
	/// </summary>
	static float StageRate(float value,float start,float end){
		float rate = (value - start) / (end - start);
		return rate < 0.0f?0.0f:(rate > 1.0f?1.0f:rate);
	}

	// レールに乗せ終えたことを知らせる(次の ConsumeBoarded() で受け取られる)
	void NotifyBoarded(const Vector3& position){
		isBoardedPending_ = true;
		boardedPosition_ = position;
	}

	// 乗せ終えた知らせをまだ受け取られていないか
	bool isBoardedPending_ = false;
	// 乗せた位置
	Vector3 boardedPosition_ = {0.0f, 0.0f, 0.0f};
};
