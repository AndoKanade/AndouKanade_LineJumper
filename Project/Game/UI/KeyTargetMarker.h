#pragma once
#include "MyMath.h"
#include <memory>

// 前方宣言
class Sprite;
class SpriteCommon;

/// <summary>
/// 鍵の的に気づいてもらうために、鍵の的の上に出す「▼」の目印
/// 画面に映っているときは的の少し上で跳ねさせ、画面の外にあるときは画面の端に、的のある向きへ回して出す。
/// カメラの後ろにあるときは出さない。出し入れは少しずつ濃く・薄くする。
/// ImGui を使わずスプライトで描くため、Release 構成でも表示される。
/// </summary>
class KeyTargetMarker{
public:
	KeyTargetMarker();
	~KeyTargetMarker();

	// テクスチャの読み込みとスプライトの生成
	void Initialize(SpriteCommon* spriteCommon);
	// 何も表示していない状態に戻す
	void Reset();

	/// <summary>
	/// 表示の動きを進める
	/// </summary>
	/// <param name="isShown">出すかどうか</param>
	/// <param name="targetPosition">目印を付ける鍵の的の中心</param>
	/// <param name="cameraPosition">カメラの位置(的がカメラの後ろにあるかを調べる)</param>
	/// <param name="cameraForward">カメラの前方ベクトル</param>
	/// <param name="viewProjection">カメラのビュー・プロジェクション行列(的の画面上の位置を求める)</param>
	/// <param name="deltaTime">経過時間(秒)</param>
	void Update(bool isShown,const Vector3& targetPosition,const Vector3& cameraPosition,const Vector3& cameraForward,const Matrix4x4& viewProjection,float deltaTime);

	// 描画処理(スプライト共通の描画前処理も含む。表示していないときは何もしない)
	void Draw();

private:
	SpriteCommon* spriteCommon_ = nullptr;
	// ▼
	std::unique_ptr<Sprite> sprite_;
	// 今の濃さ(0〜1)
	float alpha_ = 0.0f;
	// 跳ねさせるための経過時間(秒)
	float bobTimer_ = 0.0f;

	// 画像の大きさ(ピクセル)。生成した画像(128 × 128)を縮めて表示する
	static constexpr Vector2 kSize = {56.0f, 56.0f};
	// 的の中心から上へずらす量(ワールド座標の高さ。的の大きさより上に出す)
	static constexpr float kWorldHeightOffset = 1.2f;
	// 跳ねる高さ(ピクセル)と速さ(1秒あたりの位相の進み、ラジアン)
	static constexpr float kBobHeight = 10.0f;
	static constexpr float kBobSpeed = 8.0f;
	// 画面の端に出すときの、端からの余白(ピクセル。目印の大きさの分は内側へ入れる)
	static constexpr float kEdgeMargin = 48.0f;
	// 濃くなる時間・薄くなって消える時間(秒)
	static constexpr float kFadeInDuration = 0.15f;
	static constexpr float kFadeOutDuration = 0.25f;
	// 的がカメラの前にあるとみなす、カメラの前方ベクトルに沿った距離の下限
	static constexpr float kMinForwardDistance = 0.1f;
	// sin波(-1〜1)を0〜1に直す倍率
	static constexpr float kWaveToRateScale = 0.5f;
	// 画像の中心を基準点にする
	static constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
};
