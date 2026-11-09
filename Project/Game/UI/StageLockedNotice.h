#pragma once
#include "MyMath.h"
#include <memory>

// 前方宣言
class Sprite;
class SpriteCommon;

/// <summary>
/// プレイ会用の設定のときに、遊べないステージのゲートの前に来たら出す「いまは遊べないよ!!」の表示
/// ゲートの前に来た瞬間に、少し大きい状態から弾むように縮んで出し、離れたら薄くなって消える。
/// ImGui を使わずスプライトで描くため、Release 構成でも表示される。
/// </summary>
class StageLockedNotice{
public:
	StageLockedNotice();
	~StageLockedNotice();

	// テクスチャの読み込みとスプライトの生成
	void Initialize(SpriteCommon* spriteCommon);
	// 何も表示していない状態に戻す
	void Reset();

	/// <summary>
	/// 表示の動きを進める
	/// </summary>
	/// <param name="isShown">出すかどうか(遊べないステージのゲートの前にいる間 true)</param>
	/// <param name="deltaTime">経過時間(秒)</param>
	void Update(bool isShown,float deltaTime);

	// 描画処理(スプライト共通の描画前処理も含む。表示していないときは何もしない)
	void Draw();

private:
	SpriteCommon* spriteCommon_ = nullptr;
	// いまは遊べないよ!!
	std::unique_ptr<Sprite> sprite_;
	// 出し始めてからの経過時間(秒。弾む動きに使う)
	float popTimer_ = 0.0f;
	// 今の濃さ(0〜1。出している間は上がり、消すときは下がる)
	float alpha_ = 0.0f;
	// 前のフレームで出していたか(出し始めた瞬間に弾む動きを最初からにする)
	bool wasShown_ = false;

	// 画像の大きさ(ピクセル)。生成した画像(1280 × 400)を縮めて表示する
	static constexpr Vector2 kSize = {832.0f, 260.0f};
	// 画面上の位置(画面の幅・高さに対する割合)
	static constexpr float kCenterXRate = 0.5f;
	static constexpr float kHeightRate = 0.3f;
	// 出し始めの大きさの倍率と、弾みながら元の大きさに収まるまでの時間(秒)
	static constexpr float kPopStartScale = 1.4f;
	static constexpr float kPopDuration = 0.25f;
	// 濃くなる時間・薄くなって消える時間(秒)
	static constexpr float kFadeInDuration = 0.1f;
	static constexpr float kFadeOutDuration = 0.25f;
	// 画像の中心を基準点にする
	static constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
};
