#pragma once

#include "MyMath.h"
#include <functional>
#include <memory>
#include <vector>

class Sprite;
class SpriteCommon;

/// <summary>
/// 画面のつながりが切れる瞬間(シーンの切り替え・落下リスタートなど)を隠すための暗転
/// 画面を覆う → 覆い終わったら指定された処理を呼ぶ → 画面を戻す、の順に進める。
/// シーンを切り替えても消えないよう SceneManager が持ち、すべてのシーンの上に描画する。
/// 進め方(Update)と見た目(Draw)を分けているため、後から見た目だけを別の演出に差し替えられる。
/// </summary>
class SceneTransition{
public:
	// 画面を覆う演出の種類
	enum class Style{
		Fade,        // 黒で徐々に覆う(シーンの切り替えで使う基本の暗転)
		RailStripes, // レールを模した斜めの帯が左右から走って覆い、始点の方向(左)へ抜けていく
		FallAndLift, // 帯が上から落ちてきて覆い、下に沈んでから上へ持ち上げるように抜けていく
		RetryCutIn,  // 画面を暗くして斜めの帯と「RETRY」の文字を差し込み、黒の帯を広げて覆う
	};

	SceneTransition();
	~SceneTransition();

	// 画面全体を覆うスプライトを生成する
	void Initialize(SpriteCommon* spriteCommon);

	// スプライトを解放する(SceneManager はプログラム終了まで残るため、DirectX の終了前に呼ぶ)
	void Finalize();

	// 暗転を始める。画面を覆い終わったら onCovered を1回だけ呼ぶ
	// すでに暗転中なら何もせず false を返す(同じ遷移が二重に始まらないようにするため)
	bool Start(std::function<void()> onCovered,Style style = Style::Fade);

	// 暗転を進める
	void Update(float deltaTime);

	// 画面を覆うスプライトを描画する(すべてのシーンの描画の後に呼ぶ)
	void Draw();

	// 暗転中かどうか(覆っている途中・覆い終わり・戻している途中のいずれか)
	bool IsActive() const{ return phase_ != Phase::None; }

private:
	enum class Phase{
		None,      // 何もしていない
		Covering,  // 画面を覆っている途中
		Covered,   // 覆い終わった(真っ暗な画面を1フレーム描画してから onCovered を呼ぶ)
		Revealing, // 画面を戻している途中
	};

	// 演出ごとの画面を覆いきるまでの時間(秒)
	float GetCoverDuration() const;
	// 演出ごとの画面を戻しきるまでの時間(秒)
	float GetRevealDuration() const;

	// 演出ごとに、今の覆い具合に合わせてスプライトの位置・大きさ・色を決める
	void UpdateFade();
	void UpdateRailStripes();
	void UpdateFallAndLift();
	void UpdateRetryCutIn();

	// 今の段階
	Phase phase_ = Phase::None;
	// 今の演出の種類
	Style style_ = Style::Fade;
	// 画面の覆い具合(0: 何も覆っていない、1: 完全に覆っている)
	float coverRate_ = 0.0f;
	// 画面を覆い終わったときに呼ぶ処理
	std::function<void()> onCovered_;

	SpriteCommon* spriteCommon_ = nullptr;
	// 画面全体を覆う黒のスプライト(Fade の暗転と、RetryCutIn で画面を暗くするのに使う)
	std::unique_ptr<Sprite> fadeSprite_;
	// RailStripes の帯の本体と、帯の縁に引くレールの線
	std::vector<std::unique_ptr<Sprite>> stripeSprites_;
	std::vector<std::unique_ptr<Sprite>> stripeLineSprites_;
	// FallAndLift の帯の本体と、帯の下の縁(落ちてくる先頭)に引く線
	std::vector<std::unique_ptr<Sprite>> columnSprites_;
	std::vector<std::unique_ptr<Sprite>> columnLineSprites_;
	// RetryCutIn の、広がって画面を覆う黒の帯・差し込む色付きの帯・「RETRY」の文字
	std::unique_ptr<Sprite> cutInCoverSprite_;
	std::unique_ptr<Sprite> cutInBandSprite_;
	std::unique_ptr<Sprite> cutInTextSprite_;

	// 演出の色(色数を黒と差し色の2色に絞り、何が起きたかを一目で分かるようにする)
	static constexpr Vector4 kBaseColor = {0.03f, 0.03f, 0.05f, 1.0f};
	static constexpr Vector4 kAccentColor = {1.0f, 0.2f, 0.55f, 1.0f};

	// Fade の画面を覆いきる・戻しきるまでの時間(秒)
	static constexpr float kFadeCoverDuration = 0.4f;
	static constexpr float kFadeRevealDuration = 0.4f;
	// 覆い具合の上限と下限
	static constexpr float kFullyCovered = 1.0f;
	static constexpr float kNotCovered = 0.0f;

	// RailStripes の調整項目
	// 画面を覆いきる・戻しきるまでの時間(秒)
	static constexpr float kStripeCoverDuration = 0.5f;
	static constexpr float kStripeRevealDuration = 0.45f;
	// 帯の本数
	static constexpr int kStripeCount = 7;
	// 帯の傾き(ラジアン。マイナスで右上がり)
	static constexpr float kStripeAngle = -0.35f;
	// 帯どうしが走り出すタイミングのずれ(覆い具合に対する割合)
	static constexpr float kStripeStagger = 0.07f;
	// 隣の帯と隙間ができないよう、帯を少し太くする倍率
	static constexpr float kStripeOverlapScale = 1.05f;
	// レールの線の太さ(ピクセル)
	static constexpr float kStripeLineThickness = 6.0f;

	// FallAndLift の調整項目
	// 画面を覆いきる・戻しきるまでの時間(秒)
	static constexpr float kColumnCoverDuration = 0.55f;
	static constexpr float kColumnRevealDuration = 0.5f;
	// 帯の本数(傾けたときに画面の左右の端が空かないよう、画面外に1本ずつ余分に置く)
	static constexpr int kColumnCount = 10;
	// 画面の外に置く帯の本数(左右それぞれ1本)
	static constexpr int kColumnMarginCount = 2;
	// 帯の傾き(ラジアン。落ちる勢いを出すため少しだけ傾ける)
	static constexpr float kColumnAngle = 0.17f;
	// 帯どうしが落ち始めるタイミングのずれ(覆い具合に対する割合)
	static constexpr float kColumnStagger = 0.05f;
	// 落ちる順番をばらけさせるため、帯の番号にこの数を掛けて本数で割った余りを順番にする(本数と互いに素な数)
	static constexpr int kColumnOrderStep = 3;
	// 傾けても画面の上下の端が空かないよう、帯を画面より長くする倍率
	static constexpr float kColumnHeightScale = 1.4f;
	// 隣の帯と隙間ができないよう、帯を少し太くする倍率
	static constexpr float kColumnOverlapScale = 1.1f;
	// 画面の外から落ちてくる距離(画面の高さに対する倍率)
	static constexpr float kColumnTravelScale = 1.6f;
	// 帯の下の縁に引く線の太さ(ピクセル)
	static constexpr float kColumnLineThickness = 10.0f;

	// RetryCutIn の調整項目
	// 画面を覆いきる・戻しきるまでの時間(秒。文字を読ませるため覆う側を長めにする)
	static constexpr float kCutInCoverDuration = 1.0f;
	static constexpr float kCutInRevealDuration = 0.45f;
	// 帯と文字の傾き(ラジアン。マイナスで右上がり)
	static constexpr float kCutInAngle = -0.2f;
	// 色付きの帯の太さ(ピクセル)
	static constexpr float kCutInBandThickness = 220.0f;
	// 画面を暗くする濃さ
	static constexpr float kCutInTintAlpha = 0.5f;
	// 帯と文字が差し込み終わる覆い具合
	static constexpr float kCutInSlideEnd = 0.25f;
	// 黒の帯が広がり始める覆い具合
	static constexpr float kCutInCoverStart = 0.6f;
	// 帯が止まっている間に文字をゆっくり流す距離(ピクセル)
	static constexpr float kCutInTextDrift = 40.0f;
	// 文字の画像を拡大する倍率
	static constexpr float kCutInTextScale = 1.5f;
	// 画面を戻すときに、帯と文字が抜けきる進み具合
	static constexpr float kCutInExitEnd = 0.5f;
};
