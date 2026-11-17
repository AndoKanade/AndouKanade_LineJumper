#pragma once

#include "MyMath.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

// 前方宣言
class Sprite;
class SpriteCommon;

/// <summary>
/// クリア画面で結果を順に見せる表示
/// 「GAME CLEAR!」を出した後、結果の行ごとにインクのしみを塗り、塗った跡の上に結果を出す。
/// 壊した的の数 → 最大連鎖数の順に 0 から数字を増やしていき(止まった瞬間に弾ませる)、
/// 最後に結果から決めたランク(S / A / B / C)をハンコのように押す。
/// ImGui を使わずスプライトで描くため、Release 構成でも表示される。
/// </summary>
class ClearResultUI{
public:
	// ランク(良い順)
	enum class Rank{
		S,
		A,
		B,
		C,
		Count, // ランクの数
	};

	ClearResultUI();
	~ClearResultUI();

	/// <summary>
	/// テクスチャの読み込みとスプライトの生成
	/// </summary>
	/// <param name="spriteCommon">スプライト共通設定</param>
	/// <param name="destroyedCount">壊した的の数</param>
	/// <param name="totalCount">配置されていた的の総数</param>
	/// <param name="maxCombo">一番長く続いた連鎖数</param>
	void Initialize(SpriteCommon* spriteCommon,int destroyedCount,int totalCount,int maxCombo);

	// 表示の動きを進める
	void Update(float deltaTime);

	// 途中を飛ばして、すべての結果を出し終えた状態にする
	void Skip();

	// すべての結果を出し終えたかどうか
	bool IsFinished() const;

	// 描画処理(スプライト共通の描画前処理も含む)
	void Draw();

	/// <summary>
	/// 結果からランクを決める
	/// 的を壊した割合と、最大連鎖数の満点の連鎖数に対する割合を重みを付けて足し、その値でランクを決める。
	/// </summary>
	static Rank CalculateRank(int destroyedCount,int totalCount,int maxCombo);

private:
	// 出し始めの時間から、指定した時間をかけて進む割合(0〜1)を求める
	float GetProgress(float startTime,float duration) const;

	// インクのしみを、出し始めの時間から弾けるように広げて塗る
	void UpdateSplat(Sprite* sprite,const Vector2& center,const Vector2& size,float angle,const Vector4& color,float startTime);

	// 0 から目標の数まで、出し始めの時間からだんだん増やした今の数を求める
	int GetCountUpValue(int targetValue,float startTime) const;

	// 数え終わった瞬間に大きくして、だんだん元の大きさに戻す倍率を求める
	float GetCountEndPopScale(float startTime) const;

	/// <summary>
	/// 数字の文字列を、数字用スプライトを並べて配置する(StageHUD と同じ数字の画像を使う)
	/// </summary>
	/// <param name="text">表示する文字列(0〜9, +, / のみ)</param>
	/// <param name="center">文字列全体の中心の画面座標</param>
	/// <param name="charHeight">1文字の高さ(ピクセル)</param>
	/// <param name="color">文字の色</param>
	void LayoutText(const std::string& text,const Vector2& center,float charHeight,const Vector4& color);

	SpriteCommon* spriteCommon_ = nullptr;

	// 表示する結果と、そこから決めたランク
	int destroyedCount_ = 0;
	int totalCount_ = 0;
	int maxCombo_ = 0;
	Rank rank_ = Rank::C;

	// 表示を始めてからの経過時間(秒)
	float timer_ = 0.0f;

	// 画面全体を塗る暗い下地(インクのしみを目立たせる)
	std::unique_ptr<Sprite> backgroundSprite_;
	// 「GAME CLEAR!」(クリア画面の画像の上側だけを切り出す)
	std::unique_ptr<Sprite> titleSprite_;
	// 「SPACE でタイトルへ」(クリア画面の画像の下側だけを切り出す)
	std::unique_ptr<Sprite> hintSprite_;
	// 結果の行ごとに塗るインクのしみ(壊した的の数・最大連鎖数・ランクの順)
	static constexpr size_t kTargetSplatIndex = 0;
	static constexpr size_t kComboSplatIndex = 1;
	static constexpr size_t kRankSplatIndex = 2;
	static constexpr size_t kSplatCount = 3;
	std::array<std::unique_ptr<Sprite>,kSplatCount> splatSprites_;
	// 壊した的の数の横に置く標的のアイコン
	std::unique_ptr<Sprite> targetIcon_;
	// 最大連鎖数を入れる吹き出し
	std::unique_ptr<Sprite> comboBubble_;
	// ランクの上に出す「RANK」
	std::unique_ptr<Sprite> rankLabelSprite_;
	// ランクの文字
	std::unique_ptr<Sprite> rankSprite_;
	// 数字用のスプライト(使い回す。表示する文字数ぶんだけ使う)
	std::vector<std::unique_ptr<Sprite>> digitSprites_;
	// このフレームで使った数字用スプライトの数
	size_t usedDigitCount_ = 0;

	// --- 出す順番と時間(秒) ---
	// 「GAME CLEAR!」が弾けるように出てくる時間
	static constexpr float kTitleStartTime = 0.0f;
	static constexpr float kTitleDuration = 0.45f;
	// インクのしみが広がる時間と、しみを塗り始めてから中身を出し始めるまでの時間
	static constexpr float kSplatDuration = 0.3f;
	static constexpr float kContentDelay = 0.2f;
	// 中身が濃くなる時間
	static constexpr float kContentFadeDuration = 0.15f;
	// 数字を 0 から増やしきるまでの時間
	static constexpr float kCountUpDuration = 1.0f;
	// 数え終わってから次の行のしみを塗り始めるまでの間
	static constexpr float kRowInterval = 0.3f;
	// 「GAME CLEAR!」が出てから最初の行のしみを塗り始めるまでの間
	static constexpr float kTitleToRowInterval = 0.05f;
	// 壊した的の数の行
	static constexpr float kTargetSplatStartTime = kTitleStartTime + kTitleDuration + kTitleToRowInterval;
	static constexpr float kTargetCountStartTime = kTargetSplatStartTime + kContentDelay;
	// 最大連鎖数の行
	static constexpr float kComboSplatStartTime = kTargetCountStartTime + kCountUpDuration + kRowInterval;
	static constexpr float kComboCountStartTime = kComboSplatStartTime + kContentDelay;
	// ランク(数え終わってから少し溜めて押す)
	static constexpr float kRankPauseTime = 0.4f;
	static constexpr float kRankSplatStartTime = kComboCountStartTime + kCountUpDuration + kRankPauseTime;
	static constexpr float kRankStampStartTime = kRankSplatStartTime + kContentDelay;
	// ランクを押しつけてくる時間と、押しつけた後に揺れる時間
	static constexpr float kStampDuration = 0.25f;
	static constexpr float kWobbleDuration = 0.45f;
	// すべてを出し終えて「SPACE でタイトルへ」を出し始める時間
	static constexpr float kFinishTime = kRankStampStartTime + kStampDuration + kWobbleDuration;

	// --- 動き ---
	// 数え終わった瞬間に大きくする割合(1 + この値の倍率まで弾む)と、元の大きさに戻るまでの時間
	static constexpr float kCountEndPopScale = 0.3f;
	static constexpr float kCountEndPopDuration = 0.2f;
	// ランクを押しつけ始めるときの大きさの倍率
	static constexpr float kStampStartScale = 3.0f;
	// 押しつけた後の揺れの角度の幅(ラジアン)と速さ(1秒あたりの位相の進み、ラジアン)
	static constexpr float kWobbleAngle = 0.12f;
	static constexpr float kWobbleSpeed = 30.0f;
	// 「SPACE でタイトルへ」を点滅させる速さ(1秒あたりの位相の進み、ラジアン)と、一番薄いときの濃さ
	static constexpr float kHintBlinkSpeed = 4.0f;
	static constexpr float kHintMinAlpha = 0.35f;

	// --- ランクの決め方 ---
	// 的を壊した割合と、最大連鎖数の割合の重み(足すと 1 になるようにする)
	static constexpr float kTargetRateWeight = 0.7f;
	static constexpr float kComboRateWeight = 0.3f;
	// 最大連鎖数の割合を満点にする連鎖数
	// 最大連鎖数は1つのステージの中で続いた数なので、全ステージの的の総数では割らない(的を足すほど満点が遠くなるため)
	static constexpr int kFullComboCount = 10;
	// ランクごとに必要な値(S・A・B の順。どれにも届かなければ C)
	static constexpr std::array<float,static_cast<size_t>(Rank::C)> kRankThresholds = {0.9f, 0.7f, 0.5f};
	// ランクごとのインクの色(S・A・B・C の順)
	static constexpr std::array<Vector4,static_cast<size_t>(Rank::Count)> kRankColors = {{
		{1.0f, 0.85f, 0.15f, 1.0f},  // S: 自分のインクの黄色
		{1.0f, 0.36f, 0.66f, 1.0f},  // A: ピンク
		{0.3f, 0.78f, 1.0f, 1.0f},   // B: 水色
		{0.55f, 0.58f, 0.66f, 1.0f}, // C: 灰色
	}};

	// --- 配置(画面の幅・高さに対する割合と、ピクセル) ---
	// 画面全体を塗る下地の色(暗い紺)
	static constexpr Vector4 kBackgroundColor = {0.07f, 0.09f, 0.15f, 1.0f};
	// クリア画面の画像(1024 × 320)のうち、「GAME CLEAR!」と「SPACE でタイトルへ」を切り出す範囲
	static constexpr Vector2 kTitleTextureLeftTop = {0.0f, 0.0f};
	static constexpr Vector2 kTitleTextureSize = {1024.0f, 210.0f};
	static constexpr Vector2 kHintTextureLeftTop = {0.0f, 220.0f};
	static constexpr Vector2 kHintTextureSize = {1024.0f, 100.0f};
	// 「GAME CLEAR!」と「SPACE でタイトルへ」の中心
	static constexpr Vector2 kTitleCenterRate = {0.5f, 0.15f};
	static constexpr Vector2 kHintCenterRate = {0.5f, 0.93f};
	// 結果の行のしみの中心・大きさ・傾き
	static constexpr Vector2 kTargetRowCenterRate = {0.32f, 0.42f};
	static constexpr Vector2 kComboRowCenterRate = {0.32f, 0.66f};
	static constexpr Vector2 kRowSplatSize = {620.0f, 260.0f};
	static constexpr float kTargetSplatAngle = -0.08f;
	static constexpr float kComboSplatAngle = 0.06f;
	// 壊した的の数の行の、標的のアイコンと数字の中心の、行の中心からの横の位置と大きさ
	static constexpr float kTargetIconOffsetX = -170.0f;
	static constexpr float kTargetIconSize = 110.0f;
	static constexpr float kTargetTextOffsetX = 60.0f;
	// 最大連鎖数の吹き出しの大きさ
	static constexpr Vector2 kComboBubbleSize = {340.0f, 170.0f};
	// 結果の数字の高さ
	static constexpr float kResultCharHeight = 100.0f;
	// ランクのしみの中心・大きさ・傾き
	static constexpr Vector2 kRankCenterRate = {0.72f, 0.56f};
	static constexpr Vector2 kRankSplatSize = {480.0f, 480.0f};
	static constexpr float kRankSplatAngle = 0.4f;
	// 「RANK」の中心(ランクの中心からの縦の位置)と大きさ
	static constexpr float kRankLabelOffsetY = -200.0f;
	static constexpr Vector2 kRankLabelSize = {320.0f, 96.0f};
	// ランクの文字の大きさ
	static constexpr Vector2 kRankSize = {320.0f, 320.0f};

	// --- 数字 ---
	// 数字のテクスチャの1文字分の大きさ(ピクセル)
	static constexpr Vector2 kGlyphSize = {64.0f, 80.0f};
	// 数字のテクスチャで、0〜9の後に並んでいる記号の番号
	static constexpr int kGlyphPlusIndex = 10;
	static constexpr int kGlyphSlashIndex = 11;
	// 文字の間隔(1文字の幅に対する割合。縁取りの分だけ詰めて並べる)
	static constexpr float kCharSpacingRate = 0.78f;
	// 数字用スプライトの数(壊した数の「000/000」と連鎖数の「+000」が入る数)
	static constexpr size_t kMaxDigitCount = 12;

	// 白(画像の色をそのまま出す)と、中心・左上を基準点にするためのアンカー
	static constexpr Vector4 kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
	static constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
	static constexpr Vector2 kTopLeftAnchor = {0.0f, 0.0f};
};
