#pragma once
#include "MyMath.h"
#include <memory>

// 前方宣言
class Input;
class Sprite;
class SpriteCommon;

/// <summary>
/// ESC キーで出す終了確認メニュー(「ゲームをやめる？」→ はい / いいえ)
/// 開くと画面を暗くし、インクの板とボタンを弾むように出す。開いている間はゲームを止める。
/// 「はい」を選ぶとインクが画面いっぱいに広がってから終了し、「いいえ」(または ESC)で閉じてゲームに戻る。
/// ImGui を使わずスプライトで描くため、Release 構成でも表示される。
/// </summary>
class QuitConfirmMenu{
public:
	QuitConfirmMenu();
	~QuitConfirmMenu();

	// テクスチャ・効果音の読み込みとスプライトの生成
	void Initialize(SpriteCommon* spriteCommon);

	/// <summary>
	/// 開閉と選択の更新(毎フレーム、シーンの更新より前に呼ぶ)
	/// </summary>
	/// <param name="input">入力(ESC で開き、A / D で選び、SPACE で決める)</param>
	void Update(Input* input);

	// 描画処理(スプライト共通の描画前処理も含む。閉じているときは何もしない)
	void Draw();

	// ゲームを止めておくか(開いている間と、開閉の動きの間は true)
	bool IsPausingGame() const{ return state_ != State::Closed; }
	// 「はい」を選んでインクが広がり終わり、ゲームを終了してよいか
	bool IsQuitDecided() const{ return state_ == State::Quitted; }

private:
	// メニューの状態
	enum class State{
		Closed,   // 閉じている(ゲームを動かす)
		Opening,  // 開いている途中
		Open,     // 開いていて選べる
		Closing,  // 「いいえ」を選んで閉じている途中
		Quitting, // 「はい」を選んでインクが広がっている途中
		Quitted,  // インクが広がり終わった(終了してよい)
	};
	// 選べる項目(ボタンの左から順)
	enum Option{
		kOptionYes,
		kOptionNo,
		kOptionCount,
	};

	// 開き始める(初期の選択は誤操作で終了しないよう「いいえ」にする)
	void Open();
	// 開いている間の選択と決定
	void UpdateSelection(Input* input);
	// 今の状態と経過時間から、スプライトの位置・大きさ・色を決める
	void ApplyLook();
	// 効果音を鳴らす
	void PlaySe(const char* path) const;

	SpriteCommon* spriteCommon_ = nullptr;
	// 後ろのゲーム画面を暗くする幕
	std::unique_ptr<Sprite> dimSprite_;
	// 見出しと操作の案内を描いたインクの板
	std::unique_ptr<Sprite> panelSprite_;
	// 「はい」「いいえ」のボタン
	std::unique_ptr<Sprite> optionSprites_[kOptionCount];
	// 「はい」を選んだときに画面いっぱいに広がるインク
	std::unique_ptr<Sprite> quitSplatSprite_;

	State state_ = State::Closed;
	// 今の状態になってからの経過時間(秒)
	float stateTimer_ = 0.0f;
	// 開いてからの経過時間(秒。選んでいるボタンを脈打たせるのに使う)
	float openTimer_ = 0.0f;
	// 選び直してからの経過時間(秒。選んだボタンを弾ませるのに使う)
	float selectTimer_ = 0.0f;
	// 選んでいる項目
	int selectedOption_ = kOptionNo;

	// 1フレームの時間(秒。エンジンは60fps固定。ゲームを止めている間も動かすため、シーンの時間とは別に進める)
	static constexpr float kFrameDeltaTime = 1.0f / 60.0f;

	// 開く・閉じる・インクが広がるのにかける時間(秒)
	static constexpr float kOpenDuration = 0.3f;
	static constexpr float kCloseDuration = 0.18f;
	static constexpr float kQuitSplatDuration = 0.45f;
	// 開くときの大きさの倍率(ここから弾むように 1 倍へ戻す)
	static constexpr float kOpenStartScale = 0.5f;
	// 閉じるときの大きさの倍率(1 倍からここまで縮めながら薄くする)
	static constexpr float kCloseEndScale = 0.8f;
	// ボタンを板より少し遅れて出す時間(秒)
	static constexpr float kOptionPopDelay = 0.08f;

	// 幕の色(黒)と、開ききったときの濃さ
	static constexpr Vector4 kDimColor = {0.0f, 0.0f, 0.0f, 1.0f};
	static constexpr float kDimAlpha = 0.6f;

	// 板の大きさ(ピクセル。生成した画像 1100 × 680 を縮めて表示する)と、中心の高さ(画面の高さに対する割合)
	static constexpr Vector2 kPanelSize = {880.0f, 544.0f};
	static constexpr float kPanelCenterYRate = 0.5f;

	// ボタンの大きさ(ピクセル。生成した画像 420 × 200 を縮めて表示する)
	static constexpr Vector2 kOptionSize = {273.0f, 130.0f};
	// ボタンの中心の高さ(画面の高さに対する割合)と、画面の中央から左右へずらす距離(ピクセル)
	static constexpr float kOptionCenterYRate = 0.53f;
	static constexpr float kOptionOffsetX = 180.0f;
	// 選んでいるボタンの色(はい: 敵のインクのピンク、いいえ: 自分のインクの黄色)
	static constexpr Vector4 kSelectedOptionColors[kOptionCount] = {
		{1.0f, 0.4f, 0.75f, 1.0f},
		{1.0f, 0.85f, 0.15f, 1.0f},
	};
	// 選んでいないボタンの色(暗くして、選んでいる方を目立たせる)
	static constexpr Vector4 kUnselectedOptionColor = {0.55f, 0.55f, 0.6f, 1.0f};
	// 選んでいるボタン・選んでいないボタンの大きさの倍率
	static constexpr float kSelectedOptionScale = 1.15f;
	static constexpr float kUnselectedOptionScale = 0.9f;
	// 選んでいるボタンの脈打つ大きさの幅と速さ(1秒あたりのラジアン)
	static constexpr float kPulseAmplitude = 0.04f;
	static constexpr float kPulseSpeed = 6.0f;
	// 選んでいるボタンの傾き(ラジアン。左右に揺らして、インクが跳ねているように見せる)と揺れの速さ
	static constexpr float kWobbleAngle = 0.05f;
	static constexpr float kWobbleSpeed = 4.0f;
	// 選び直したときに弾ませる大きさの倍率と時間(秒)
	static constexpr float kSelectPopScale = 1.3f;
	static constexpr float kSelectPopDuration = 0.2f;

	// インクの広がり始め・広がり終わりの大きさ(ピクセル。終わりは画面を覆いきる大きさにする)
	static constexpr float kQuitSplatStartSize = 120.0f;
	static constexpr float kQuitSplatEndSize = 4200.0f;
	// インクの回転量(ラジアン。広がりながら少し回して勢いを出す)
	static constexpr float kQuitSplatSpin = 0.6f;

	// 画像の中心を基準点にする
	static constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
	// 左上を基準点にする(画面全体を覆う幕に使う)
	static constexpr Vector2 kTopLeftAnchor = {0.0f, 0.0f};
};
