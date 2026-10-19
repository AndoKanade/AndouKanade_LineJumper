#pragma once

#include "Title/Departure/BaseDeparture.h"
#include "SceneTransition.h"

/// <summary>
/// 出発の演出: インクに潜って移動する
/// 出発ゲートが開くと出発台にインク溜まりを塗っておき、線を越えるとインク溜まりへ頭から飛び込んで沈む。
/// 画面をインクで覆う切り替えの演出を流し、覆っている間にレールの始点へ移して、覆いが晴れたらインクから飛び出して「GO!」を出す。
/// 試し撃ち場とステージの間を、画面の切り替えではっきり区切る。
/// </summary>
class InkDiveDeparture : public BaseDeparture{
public:
	void Prepare(const DepartureContext& context) override;
	void Begin(const DepartureContext& context,const CameraPose& startCamera) override;
	void Update(float deltaTime,const CameraPose& gameplayCamera) override;
	bool IsFinished() const override{ return state_ == State::Finished; }
	bool GetCameraPose(CameraPose& outPose) const override;
	void Reset() override;

private:
	// 演出の段階
	enum class State{
		Dive,     // インク溜まりへ飛び込んで沈む
		Covering, // 画面がインクで覆われるのを待つ
		Hidden,   // レールの始点へ移り、覆いが晴れるのを待つ(プレイヤーは隠しておく)
		PopOut,   // インクから飛び出す
		Finished, // 終わった
	};

	// 画面が覆われた瞬間の処理(レールの始点へ乗せる)
	void OnCovered();

	DepartureContext context_;
	State state_ = State::Finished;
	// 今の段階に入ってからの経過時間(秒)
	float timer_ = 0.0f;

	// 線を越えた位置・インク溜まりの中央・飛び出すレールの始点
	Vector3 startPosition_ = {0.0f, 0.0f, 0.0f};
	Vector3 poolPosition_ = {0.0f, 0.0f, 0.0f};
	Vector3 landPosition_ = {0.0f, 0.0f, 0.0f};
	// レールの始点での水平な前方向と、モデルを前へ向ける Y 回転
	Vector3 forward_ = {0.0f, 0.0f, 1.0f};
	float yaw_ = 0.0f;

	// 始めた瞬間に映っていたカメラと、このフレームの演出のカメラ
	CameraPose startCamera_;
	CameraPose cameraPose_;
	bool isControllingCamera_ = false;

	// 画面を覆う切り替えの演出(自分の黄色いインクが弾けて塗りつぶす)
	static constexpr SceneTransition::Style kTransitionStyle = SceneTransition::Style::InkBurst;
	// 出発台に塗るインク溜まりの半径
	static constexpr float kPoolRadius = 0.9f;

	// --- 飛び込む ---
	// 飛び込みにかける時間(秒)と、跳ねる高さ
	static constexpr float kDiveDuration = 0.55f;
	static constexpr float kDiveHopHeight = 0.8f;
	// 進み具合0〜1の放物線 s*(1-s) の頂点(s=0.5で0.25)を1にそろえるための係数
	static constexpr float kParabolaPeakScale = 4.0f;
	// 頭から飛び込むように前へ倒す最大の角度(ラジアン、約80度)
	static constexpr float kDivePitch = 1.4f;
	// 飛び込みの進み具合のうち、インクへ沈み始める割合と、沈む深さ
	static constexpr float kSinkStartRate = 0.6f;
	static constexpr float kSinkDepth = 0.3f;
	// 沈むときに横へ広げる割合(インクへ溶けるように見せる)
	static constexpr float kSinkWidenRate = 0.3f;
	// 沈んだ瞬間に跳ねるしぶきの数と、画面の揺れの幅・収まるまでの時間(秒)
	static constexpr int kDiveSplashCount = 14;
	static constexpr float kDiveShakeAmplitude = 0.06f;
	static constexpr float kDiveShakeDuration = 0.2f;
	// 飛び込む様子を映すカメラの、インク溜まりからの位置(後ろ・上)
	static constexpr float kDiveCameraBack = 2.5f;
	static constexpr float kDiveCameraHeight = 2.2f;

	// --- 飛び出す ---
	// 飛び出しにかける時間(秒)と、跳ねる高さ
	static constexpr float kPopDuration = 0.45f;
	static constexpr float kPopHeight = 0.6f;
};
