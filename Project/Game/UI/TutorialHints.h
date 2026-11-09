#pragma once
#include "MyMath.h"
#include <array>
#include <memory>

// 前方宣言
class Sprite;
class SpriteCommon;

/// <summary>
/// 場面ごとに画面の上に1つずつ出す操作の案内(チュートリアル)
/// 「移動・見回す → 試し撃ち → ゲートで出発 → レールで的を撃つ」の順に出し、その操作をしたら消して次の案内を出す。
/// 一度消した案内は、落ちてやり直しても出し直さない(タイトルから始め直すと Reset() で最初から)。
/// 先の操作を先にしてしまったとき(案内の前にゲートを開けたなど)は、そこまでの案内を飛ばす。
/// ImGui を使わずスプライトで描くため、Release 構成でも表示される。
/// </summary>
class TutorialHints{
public:
	// 1フレーム分の、案内を進めるのに使う状況
	struct Situation{
		bool isOnPlatform = false;       // スタート台の上を動ける段階か
		bool isRiding = false;           // レールを進んでプレイしている最中か
		Vector3 playerPosition = {0.0f, 0.0f, 0.0f}; // プレイヤーの座標
		bool hasFired = false;           // このフレームで弾を撃ったか
		bool hasOpenedGate = false;      // このフレームで出発ゲートを開け始めたか
		bool hasDestroyedTarget = false; // このフレームで的を壊したか
	};

	TutorialHints();
	~TutorialHints();

	// テクスチャの読み込みとスプライトの生成
	void Initialize(SpriteCommon* spriteCommon);
	// 最初の案内から出し直す(タイトルから始め直すときに呼ぶ)
	void Reset();

	/// <summary>
	/// 案内を進め、表示の動きを進める
	/// </summary>
	/// <param name="situation">このフレームの状況</param>
	/// <param name="deltaTime">経過時間(秒)</param>
	void Update(const Situation& situation,float deltaTime);

	// 描画処理(スプライト共通の描画前処理も含む。表示していないときは何もしない)
	void Draw();

private:
	// 案内の段階(並びは出す順番。Done はすべて出し終えた状態)
	enum class Step{
		Move,
		Shoot,
		Gate,
		RailShoot,
		Done,
	};
	static constexpr int kHintCount = static_cast<int>(Step::Done);

	// 今の段階の案内を出す場面か
	bool IsShowingScene(const Situation& situation) const;
	// 今の段階の操作をしたか(このフレームで案内を消してよいか)
	bool IsStepCompleted(const Situation& situation) const;
	// 指定した段階へ進める(出していた案内は薄くして消す)
	void AdvanceTo(Step step);

	SpriteCommon* spriteCommon_ = nullptr;
	// 案内ごとのスプライト(並びは Step と同じ)
	std::array<std::unique_ptr<Sprite>,kHintCount> sprites_;

	// 今の段階
	Step step_ = Step::Move;
	// 今の段階の案内を出し始めてからの経過時間(秒。出す場面にいる間だけ進める)
	float shownTimer_ = 0.0f;
	// 移動の案内を出し始めたときのプレイヤーの座標(どれだけ歩いたかを測る)
	Vector3 moveStartPosition_ = {0.0f, 0.0f, 0.0f};
	// 移動の案内を出し始めたか(出し始めた瞬間の座標を覚える)
	bool hasMoveStarted_ = false;
	// 試し撃ちの案内を出している間に撃った回数(試し撃ちの案内を消すのに使う)
	int shotCount_ = 0;
	// 今の案内の濃さ(0〜1)
	float alpha_ = 0.0f;
	// 消えていく案内(前の段階の案内。薄くしている間だけ描く)と、その濃さ
	int fadingHintIndex_ = kNoHint;
	float fadingAlpha_ = 0.0f;
	// 前の案内が消えてから次の案内を出すまで待つ時間(秒)
	float nextDelayTimer_ = 0.0f;

	// 案内が無いことを表す番号
	static constexpr int kNoHint = -1;
	// 移動の案内を消す、歩いた距離(水平方向)
	static constexpr float kMoveDoneDistance = 2.0f;
	// 試し撃ちの案内を消す、撃った回数(台が塗れるのを見てもらえるよう、何発か撃ってから消す)
	static constexpr int kShootDoneCount = 5;
	// レールで撃つ案内を、的を壊さなくても消すまでの時間(秒)
	static constexpr float kRailShootMaxDuration = 6.0f;
	// 前の案内が消えてから次の案内を出すまでの時間(秒)
	static constexpr float kNextHintDelay = 0.4f;
	// 濃くなる時間・薄くなって消える時間(秒)
	static constexpr float kFadeInDuration = 0.2f;
	static constexpr float kFadeOutDuration = 0.3f;
	// 出し始めに上から下りてくる距離(ピクセル)と、下りきるまでの時間(秒)
	static constexpr float kDropDistance = 24.0f;
	static constexpr float kDropDuration = 0.3f;
	// 画像の大きさ(ピクセル)。生成した画像(1200 × 120)を縮めて表示する
	static constexpr Vector2 kSize = {720.0f, 72.0f};
	// 画面上の位置(画面の幅・高さに対する割合)
	static constexpr float kCenterXRate = 0.5f;
	static constexpr float kHeightRate = 0.13f;
	// 画像の中心を基準点にする
	static constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
};
