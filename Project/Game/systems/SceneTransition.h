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
		InkBurst,    // 自分の黄色いインクが画面の中心から外へ次々に弾けて塗りつぶし、下へ垂れ落ちて抜けていく(クリア)
		InkDrip,     // 暗いインクが画面の上から不揃いに垂れてきて覆い、そのまま下へ沈んで抜けていく(ゲームオーバー)
		RailPaint,   // 斜めのレールの線が走り、そのあとを黄色いインクが塗り進んで覆い、塗った帯が右へ抜けていく(クリアの案)
		GoalStamp,   // 「GOAL!」が上から押されて揺れ、しみが飛んだ後に中央から黄色いインクが広がって覆う(クリアの案)
		CloudRise,   // 下から雲が湧き上がって画面を包み、左右に割れて抜けていく(クリアの案)
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
	void UpdateInkBurst();
	void UpdateInkDrip();
	void UpdateRailPaint();
	void UpdateGoalStamp();
	void UpdateCloudRise();

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
	// InkBurst の、画面に弾けるインクのしみ(全面を塗る色は fadeSprite_ に色を掛けて使う)
	std::vector<std::unique_ptr<Sprite>> inkSplatSprites_;
	// InkDrip の、垂れるインクの本体と先の丸み(差し色の縁取り用と、本体の色用の2層)
	std::vector<std::unique_ptr<Sprite>> dripRimSprites_;
	std::vector<std::unique_ptr<Sprite>> dripRimTipSprites_;
	std::vector<std::unique_ptr<Sprite>> dripSprites_;
	std::vector<std::unique_ptr<Sprite>> dripTipSprites_;
	// RailPaint の、レールの線・塗り進む帯の本体・帯の先(塗り進む側)と後ろ(抜けていく側)の丸み
	std::vector<std::unique_ptr<Sprite>> railPaintLineSprites_;
	std::vector<std::unique_ptr<Sprite>> railPaintBodySprites_;
	std::vector<std::unique_ptr<Sprite>> railPaintHeadSprites_;
	std::vector<std::unique_ptr<Sprite>> railPaintTailSprites_;
	// GoalStamp の、画面を暗くする全面・押した衝撃で飛ぶしみ・中央から広がるしみ・「GOAL!」の文字
	// (最後に全面を塗る色は fadeSprite_ に色を掛けて使う)
	std::unique_ptr<Sprite> stampDimSprite_;
	std::vector<std::unique_ptr<Sprite>> stampSplashSprites_;
	std::unique_ptr<Sprite> stampSpreadSprite_;
	std::unique_ptr<Sprite> stampTextSprite_;
	// 「GOAL!」の文字の、押しきったときの大きさ(画像の大きさに kStampTextScale を掛けたもの)
	Vector2 stampTextBaseSize_ = {0.0f, 0.0f};
	// CloudRise の雲のかたまり(全面を塗る白は fadeSprite_ に色を掛けて使う)
	std::vector<std::unique_ptr<Sprite>> cloudSprites_;

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

	// InkBurst の調整項目(しみの位置・大きさ・傾きの並びは SceneTransition.cpp の kInkSplatLayouts)
	// 画面を覆いきる・戻しきるまでの時間(秒)
	static constexpr float kInkBurstCoverDuration = 0.9f;
	static constexpr float kInkBurstRevealDuration = 0.7f;
	// しみどうしが弾け始める・落ち始めるタイミングのずれ(覆い具合に対する割合)
	static constexpr float kInkSplatCoverStagger = 0.05f;
	static constexpr float kInkSplatRevealStagger = 0.04f;
	// しみが落ちていく距離(画面の高さに対する倍率)
	static constexpr float kInkSplatFallScale = 1.8f;
	// しみの隙間を埋めるため、画面全体をインクの色で塗り始める覆い具合
	static constexpr float kInkFillCoverStart = 0.8f;
	// 画面を戻すときに、全体の塗りが消えきる進み具合
	static constexpr float kInkFillRevealEnd = 0.3f;
	// 自分のインクの色(黄色)と、重なりを見分けやすくするための少し濃い色(しみごとに交互に使う)
	static constexpr Vector4 kInkColor = {1.0f, 0.85f, 0.15f, 1.0f};
	static constexpr Vector4 kInkShadeColor = {0.95f, 0.62f, 0.1f, 1.0f};

	// InkDrip の調整項目
	// 画面を覆いきる・戻しきるまでの時間(秒。重さを出すため覆う側を長めにする)
	static constexpr float kDripCoverDuration = 1.1f;
	static constexpr float kDripRevealDuration = 0.7f;
	// 垂れるインクの本数
	static constexpr int kDripCount = 16;
	// 垂れ始める順番をばらけさせるため、番号にこの数を掛けて本数で割った余りを順番にする(本数と互いに素な数)
	static constexpr int kDripOrderStep = 5;
	// 垂れるインクどうしが垂れ始めるタイミングのずれ(覆い具合に対する割合)
	static constexpr float kDripStagger = 0.03f;
	// 最初に画面の上の縁を埋める帯の長さ(画面の高さに対する割合)と、埋め終わる覆い具合
	static constexpr float kDripBandRate = 0.12f;
	static constexpr float kDripBandEnd = 0.25f;
	// 本数で分けた幅に対する、垂れるインクの太さの倍率(不揃いに見せるため順に使う)
	// 隣り合う2本の倍率の和が2以上になるようにして、垂れきったときに隙間ができないようにする
	static constexpr float kDripWidthScales[] = {1.3f, 0.9f, 1.5f, 1.0f, 1.2f, 0.85f, 1.4f, 1.1f};
	// 差し色の縁取りを本体より先に出す長さ(ピクセル)
	static constexpr float kDripRimLead = 8.0f;
	// 垂れている間に画面を暗くする濃さ
	static constexpr float kDripTintAlpha = 0.45f;
	// 隙間が残っても画面を覆いきれるよう、暗くする濃さを1まで上げ始める覆い具合
	static constexpr float kDripFillStart = 0.92f;
	// 画面を戻すときに沈んでいく距離(画面の高さに対する倍率)
	static constexpr float kDripSinkScale = 1.6f;

	// RailPaint の調整項目
	// 画面を覆いきる・戻しきるまでの時間(秒)
	static constexpr float kRailPaintCoverDuration = 0.9f;
	static constexpr float kRailPaintRevealDuration = 0.6f;
	// 塗り進む帯の本数と傾き(ラジアン。マイナスで右上がり)
	static constexpr int kRailPaintCount = 8;
	static constexpr float kRailPaintAngle = -0.12f;
	// 帯どうしが塗り始める・抜け始めるタイミングのずれ(覆い具合に対する割合)
	static constexpr float kRailPaintCoverStagger = 0.06f;
	static constexpr float kRailPaintRevealStagger = 0.03f;
	// レールの線が端から端まで走りきる覆い具合
	static constexpr float kRailPaintLineEnd = 0.35f;
	// 帯を塗り進める長さ(帯の長さに対する倍率。先の丸みが画面の外へ出きるよう少し長くする)
	static constexpr float kRailPaintTravelScale = 1.1f;
	// 隣の帯と隙間ができないよう、帯を少し太くする倍率
	static constexpr float kRailPaintOverlapScale = 1.08f;
	// レールの線の太さ(ピクセル)
	static constexpr float kRailPaintLineThickness = 6.0f;

	// GoalStamp の調整項目(押した衝撃で飛ぶしみの位置・大きさは SceneTransition.cpp の kStampSplashLayouts)
	// 画面を覆いきる・戻しきるまでの時間(秒。文字を読ませるため覆う側を長めにする)
	static constexpr float kStampCoverDuration = 1.4f;
	static constexpr float kStampRevealDuration = 0.6f;
	// 画面を暗くする濃さと、暗くしきる覆い具合
	static constexpr float kStampDimAlpha = 0.35f;
	static constexpr float kStampDimEnd = 0.3f;
	// 文字が押されきる(大きい状態から縮んで止まる)覆い具合と、押し始めの大きさの倍率
	static constexpr float kStampSlamEnd = 0.3f;
	static constexpr float kStampStartScale = 3.0f;
	// 文字がはっきり見えるようになる覆い具合
	static constexpr float kStampTextAppearEnd = 0.1f;
	// 文字の画像を表示する大きさの倍率
	static constexpr float kStampTextScale = 0.9f;
	// 文字の中心の高さ(画面の高さに対する割合)
	static constexpr float kStampHeightRate = 0.45f;
	// 押した後に文字を左右に揺らす幅(ピクセル)・速さ(覆い具合1あたりの位相の進み、ラジアン)・揺れが収まる覆い具合
	static constexpr float kStampShakeWidth = 12.0f;
	static constexpr float kStampShakeSpeed = 120.0f;
	static constexpr float kStampShakeEnd = 0.45f;
	// 押した衝撃で飛ぶしみが広がりきる覆い具合
	static constexpr float kStampSplashEnd = 0.5f;
	// 中央からしみが広がり始める覆い具合と、広がりきったときの大きさ(画面の高さに対する倍率)と傾き
	static constexpr float kStampSpreadStart = 0.55f;
	static constexpr float kStampSpreadScale = 4.5f;
	static constexpr float kStampSpreadAngle = 0.3f;
	// 隙間を埋めるため、画面全体をインクの色で塗り始める覆い具合
	static constexpr float kStampFillStart = 0.85f;
	// 画面を戻すときに、全体の塗りが消えきる進み具合と、広がったしみが落ちていく距離(画面の高さに対する倍率)
	static constexpr float kStampFillRevealEnd = 0.25f;
	static constexpr float kStampSpreadFallScale = 1.5f;

	// CloudRise の調整項目
	// 画面を覆いきる・戻しきるまでの時間(秒)
	static constexpr float kCloudCoverDuration = 1.0f;
	static constexpr float kCloudRevealDuration = 0.7f;
	// 雲を並べる段の数と、1段あたりの数
	static constexpr int kCloudRowCount = 7;
	static constexpr int kCloudColumnCount = 9;
	// 一番下の段と一番上の段の中心の高さ(画面の高さに対する割合。画面の上下の外まで並べる)
	static constexpr float kCloudBottomRowHeight = 1.15f;
	static constexpr float kCloudTopRowHeight = -0.1f;
	// 段どうしが湧き上がり始めるタイミングのずれ(覆い具合に対する割合)
	static constexpr float kCloudStagger = 0.08f;
	// 湧き上がり始めの大きさの割合(ここから元の大きさまでふくらむ)
	static constexpr float kCloudStartSizeRate = 0.6f;
	// 隙間を埋めるため、画面全体を白で塗り始める覆い具合
	static constexpr float kCloudFillStart = 0.85f;
	// 画面を戻すときに左右へ割れて動く距離(画面の幅に対する割合)
	static constexpr float kCloudSplitDistance = 0.8f;
	// 雲の色(白と、影になったところの薄い水色。数個に1つを影の色にする)
	static constexpr Vector4 kCloudColor = {1.0f, 1.0f, 1.0f, 1.0f};
	static constexpr Vector4 kCloudShadeColor = {0.82f, 0.9f, 0.95f, 1.0f};
	static constexpr int kCloudShadeInterval = 3;
};
