#pragma once
#include "PostProcess.h"
#include <memory>

/// <summary>
/// ポストエフェクトの動作モード(状態)の基底クラス
/// 画面全体にかけるポストプロセスの種類と、時間とともに変わる演出(グリッチ・放射ブラー・ディゾルブ)の進め方を、状態ごとのクラスに分ける。
/// 持ち主(Application)は今の状態を1つだけ持ち、毎フレーム Update() を呼んで、描く種類を GetType() で受け取るだけにする。
/// 演出の終わりは状態自身が判定し、次の状態を Update() の戻り値で返す(持ち主は返ってきた状態へ切り替える)。
/// </summary>
class PostEffectState{
public:
	virtual ~PostEffectState() = default;

	/// <summary>
	/// この状態に入った瞬間に1回だけ呼ばれる(演出の始まりの値をポストプロセスの定数に書き込む)
	/// </summary>
	/// <param name="postProcess">値を書き込むポストプロセス</param>
	virtual void Enter(PostProcess* postProcess){ (void)postProcess; }

	/// <summary>
	/// 状態を1フレーム進める
	/// </summary>
	/// <param name="postProcess">値を書き込むポストプロセス</param>
	/// <param name="deltaTime">経過時間(秒)</param>
	/// <returns>次の状態へ移るときはその状態、この状態を続けるときは nullptr</returns>
	virtual std::unique_ptr<PostEffectState> Update(PostProcess* postProcess,float deltaTime) = 0;

	// この状態で描くポストプロセスの種類
	virtual PostProcess::Type GetType() const = 0;
};

/// <summary>
/// 指定した種類のポストプロセスをかけ続ける状態(通常の表示と、デバッグUIで選んだ種類)
/// 時間では終わらず、ほかの状態へ切り替えられるまで続ける。
/// </summary>
class SelectedPostEffectState : public PostEffectState{
public:
	/// <param name="type">かけ続けるポストプロセスの種類</param>
	explicit SelectedPostEffectState(PostProcess::Type type) : type_(type){}

	std::unique_ptr<PostEffectState> Update(PostProcess* postProcess,float deltaTime) override;
	PostProcess::Type GetType() const override{ return type_; }

private:
	// かけ続けるポストプロセスの種類
	PostProcess::Type type_;
};

/// <summary>
/// グリッチを一瞬だけかけ、決めた時間がたったら通常の表示へ戻る状態
/// </summary>
class GlitchPostEffectState : public PostEffectState{
public:
	std::unique_ptr<PostEffectState> Update(PostProcess* postProcess,float deltaTime) override;
	PostProcess::Type GetType() const override{ return PostProcess::Type::Glitch; }

private:
	// グリッチをかける時間(秒)
	static constexpr float kDuration = 0.15f;
	// 通常の表示へ戻るまでの残り時間(秒)
	float remainTime_ = kDuration;
};

/// <summary>
/// 放射ブラーを一瞬かけ、時間とともに弱めてから通常の表示へ戻る状態(衝撃の瞬間の演出用)
/// </summary>
class RadialBlurPostEffectState : public PostEffectState{
public:
	/// <param name="center">ブラーの中心(画面の UV 座標。左上が 0,0、右下が 1,1)</param>
	/// <param name="width">かけ始めのブラーの強さ(1サンプルごとに中心からずらす割合)</param>
	/// <param name="duration">ブラーが消えるまでの時間(秒)</param>
	RadialBlurPostEffectState(const Vector2& center,float width,float duration)
		: center_(center),startWidth_(width),duration_(duration){}

	void Enter(PostProcess* postProcess) override;
	std::unique_ptr<PostEffectState> Update(PostProcess* postProcess,float deltaTime) override;
	PostProcess::Type GetType() const override{ return PostProcess::Type::RadialBlur; }

private:
	// ブラーの中心(画面の UV 座標)
	Vector2 center_;
	// かけ始めのブラーの強さ
	float startWidth_;
	// ブラーが消えるまでの時間(秒)
	float duration_;
	// かけ始めてからの経過時間(秒)
	float elapsedTime_ = 0.0f;
};

/// <summary>
/// ディゾルブの閾値を 0 から 1 まで時間をかけて上げ、画面を消していく状態
/// 消しきったら、消えた画面のままディゾルブをかけ続ける状態(SelectedPostEffectState)へ移る。
/// </summary>
class DissolvePostEffectState : public PostEffectState{
public:
	std::unique_ptr<PostEffectState> Update(PostProcess* postProcess,float deltaTime) override;
	PostProcess::Type GetType() const override{ return PostProcess::Type::Dissolve; }

	// 今の閾値(0: 消えていない 〜 1: 消えきった。デバッグUIのスライダーに出すのに使う)
	float GetThreshold() const{ return threshold_; }

private:
	// 消しきるまでの時間(秒)
	static constexpr float kDuration = 2.0f;
	// 閾値の上限(消えきった状態)
	static constexpr float kMaxThreshold = 1.0f;
	// 始めてからの経過時間(秒)
	float elapsedTime_ = 0.0f;
	// 今の閾値
	float threshold_ = 0.0f;
};
