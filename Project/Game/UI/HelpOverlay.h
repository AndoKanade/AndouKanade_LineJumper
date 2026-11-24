#pragma once

#include "MyMath.h"
#include <memory>

// 前方宣言
class Input;
class Sprite;
class SpriteCommon;

/// <summary>
/// 操作説明の画面
/// H キーで開閉し、操作方法と画面の流れ(どうすれば各画面へ進むか)を1枚の画像で表示する。
/// 閉じている間は、画面の左下に「H あそびかた」の案内を出して、説明を開けることを伝える。
/// ImGui を使わずスプライトで描くため、Release 構成でも表示される。
/// 画像は tools/generate_help_texture.py で生成する。
/// </summary>
class HelpOverlay{
public:
	HelpOverlay();
	~HelpOverlay();

	// テクスチャの読み込みとスプライトの生成
	void Initialize(SpriteCommon* spriteCommon);

	/// <summary>
	/// H キーによる開閉と、表示の更新
	/// </summary>
	/// <param name="input">入力</param>
	/// <param name="isEnabled">説明を開けるかどうか。false の間は閉じて、案内も出さない</param>
	void Update(Input* input,bool isEnabled);

	// 描画処理(スプライト共通の描画前処理も含む)
	void Draw();

	// 説明を開いているかどうか(開いている間、シーン側はゲームの進行と入力を止める)
	bool IsOpen() const{ return isOpen_; }

private:
	SpriteCommon* spriteCommon_ = nullptr;

	// 説明の後ろのゲーム画面を暗くする幕
	std::unique_ptr<Sprite> dimSprite_;
	// 説明の本体(操作方法と画面の流れ)
	std::unique_ptr<Sprite> helpSprite_;
	// 閉じている間に画面の隅に出す案内
	std::unique_ptr<Sprite> hintSprite_;

	// 説明を開いているかどうか
	bool isOpen_ = false;
	// 説明を開ける状態かどうか(案内を出すかどうかにも使う)
	bool isEnabled_ = false;

	// 説明の画像の大きさ(ピクセル。画像と同じ大きさで等倍表示する)
	static constexpr Vector2 kHelpSize = {1440.0f, 800.0f};
	// 案内の画像の大きさ(ピクセル)
	static constexpr Vector2 kHintSize = {300.0f, 64.0f};
	// 画面の端から案内までの余白(ピクセル)
	static constexpr float kScreenMargin = 32.0f;
	// 説明の後ろの幕の色(黒の半透明)
	static constexpr Vector4 kDimColor = {0.0f, 0.0f, 0.0f, 0.55f};
	// 案内の色(ゲーム画面の邪魔にならないよう少し透かす)
	static constexpr Vector4 kHintColor = {1.0f, 1.0f, 1.0f, 0.85f};
	// 中心を基準点にするためのアンカー
	static constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
	// 左下を基準点にするためのアンカー
	static constexpr Vector2 kBottomLeftAnchor = {0.0f, 1.0f};
};
