#pragma once

#include "MyMath.h"
#include "Sprite.h"
#include "WinAPI.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

/// <summary>
/// 画面を覆う演出(systems/transition フォルダの各クラス)と SceneTransition で共通して使う定数と関数
/// ほかのクラスからは読み込まない。
/// </summary>
namespace TransitionEffectUtil{
	// 画面を覆う白一色のテクスチャ(色を掛けて黒にする)
	// 読み込み時にミップマップを生成するため、1x1 では失敗する。縮小できる大きさにしている
	inline const std::string kFadeTexture = "resource/white16x16.png";
	// InkBurst で弾けるインクのしみ(白で描いた画像に色を掛けて使う。tools/generate_transition_assets.py で生成する)
	inline const std::string kInkSplatTextures[] = {
		"resource/UI/transition/inkSplat0.png",
		"resource/UI/transition/inkSplat1.png",
		"resource/UI/transition/inkSplat2.png",
	};
	// 垂れるインクの先・塗り進むインクの先・雲のかたまりに使う白い円(同じスクリプトで生成する)
	inline const std::string kCircleTexture = "resource/UI/transition/circle.png";

	// InkBurst のしみ1つ分の置き方
	struct InkSplatLayout{
		Vector2 position; // 中心の位置(画面の幅・高さに対する割合)
		float sizeScale;  // 大きさ(画面の高さに対する倍率)
		float angle;      // 傾き(ラジアン)
		int textureIndex; // 使うしみの画像の番号
	};
	// しみの色を交互に変えるための割る数
	inline constexpr int kInkColorAlternateCount = 2;

	// スプライトを左上基準で配置するためのアンカー
	inline constexpr Vector2 kTopLeftAnchor = {0.0f, 0.0f};
	// 帯を傾けても中心がずれないよう、中心基準で配置するためのアンカー
	inline constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
	// 画面を覆う色(黒)
	inline constexpr float kFadeColor = 0.0f;

	// 画面の大きさと中心
	inline constexpr float kScreenWidth = static_cast<float>(WinAPI::kClientWidth);
	inline constexpr float kScreenHeight = static_cast<float>(WinAPI::kClientHeight);
	inline constexpr float kHalf = 0.5f;
	inline constexpr Vector2 kScreenCenter = {kScreenWidth * kHalf, kScreenHeight * kHalf};
	// 画面の対角線の長さ。帯の長さをこれにすると、どの傾きでも画面の端から端まで届く
	inline const float kScreenDiagonal = std::sqrt(kScreenWidth * kScreenWidth + kScreenHeight * kScreenHeight);

	// 進み具合の下限と上限
	inline constexpr float kProgressMin = 0.0f;
	inline constexpr float kProgressMax = 1.0f;
	// 帯が左右どちらから走り込むか(長さの向きに対する符号)
	inline constexpr float kFromLeft = -1.0f;
	inline constexpr float kFromRight = 1.0f;

	// 覆い具合の上限と下限
	inline constexpr float kFullyCovered = 1.0f;
	inline constexpr float kNotCovered = 0.0f;

	// 演出の色(色数を黒と差し色の2色に絞り、何が起きたかを一目で分かるようにする)
	inline constexpr Vector4 kBaseColor = {0.03f, 0.03f, 0.05f, 1.0f};
	inline constexpr Vector4 kAccentColor = {1.0f, 0.2f, 0.55f, 1.0f};
	// 自分のインクの色(黄色)と、重なりを見分けやすくするための少し濃い色(しみごとに交互に使う)
	inline constexpr Vector4 kInkColor = {1.0f, 0.85f, 0.15f, 1.0f};
	inline constexpr Vector4 kInkShadeColor = {0.95f, 0.62f, 0.1f, 1.0f};

	// 帯は傾けて置くため、中心基準のスプライトとして作る
	inline std::unique_ptr<Sprite> CreateBandSprite(SpriteCommon* spriteCommon,const std::string& texture){
		std::unique_ptr<Sprite> sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon,texture);
		sprite->SetAnchorPoint(kCenterAnchor);
		return sprite;
	}

	// スプライトを、中心の位置・傾き・大きさ・色を指定して置く
	inline void PlaceSprite(Sprite* sprite,const Vector2& center,float angle,const Vector2& size,const Vector4& color){
		sprite->SetPosition(center);
		sprite->SetRotation(angle);
		sprite->SetSize(size);
		sprite->SetColor(color);
		sprite->Update();
	}

	// 傾けた帯の長さの向き(スプライトの横方向)。Sprite の回転と同じ向きに回す
	inline Vector2 GetLengthAxis(float angle){
		return {std::cos(angle), std::sin(angle)};
	}

	// 傾けた帯の太さの向き(スプライトの縦方向)
	inline Vector2 GetThicknessAxis(float angle){
		return {-std::sin(angle), std::cos(angle)};
	}

	// 位置を指定した向きに指定した距離だけ動かす
	inline Vector2 MoveAlong(const Vector2& position,const Vector2& axis,float distance){
		return {position.x + axis.x * distance, position.y + axis.y * distance};
	}

	// 帯ごとに走り出すタイミングをずらした進み具合(0〜1)を求める
	// order 番目の帯は stagger * order だけ遅れて動き出し、最後の帯が動き終わるときに全体の進み具合が1になる
	inline float GetStaggeredProgress(float rate,int order,int count,float stagger){
		float duration = kProgressMax - stagger * static_cast<float>(count - 1);
		return std::clamp((rate - stagger * static_cast<float>(order)) / duration,kProgressMin,kProgressMax);
	}

	// 全体の進み具合のうち、start〜end の区間がどこまで進んだか(0〜1)を求める
	inline float GetRangeProgress(float rate,float start,float end){
		return std::clamp((rate - start) / (end - start),kProgressMin,kProgressMax);
	}
}
