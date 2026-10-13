#pragma once

#include "MyMath.h"
#include <memory>

// 前方宣言
class Obj3D;
class Obj3dCommon;

/// <summary>
/// スタート台
/// 最初のレールの始点の手前に置く箱。プレイヤーは台の上から始まり、自由に動いてからレールに乗る。
/// 台の前側をレールの始点と少し重ねて置き、歩いて前へ進むだけでレールに乗れるようにする。
/// </summary>
class StartPlatform{
public:
	StartPlatform();
	~StartPlatform();

	// 初期化処理(モデルの生成)
	void Initialize(Obj3dCommon* objCommon);

	/// <summary>
	/// レールの始点に合わせて台を置く(Edit中にレールを動かしても反映されるよう、Playに入るたびに呼ぶ)
	/// </summary>
	/// <param name="railStartPosition">レールの始点の座標(台の上面の高さもこの高さにそろえる)</param>
	/// <param name="railStartForward">レールの始点での進行方向(台はこの向きの手前側に置く)</param>
	void Place(const Vector3& railStartPosition,const Vector3& railStartForward);

	// 描画処理
	void Draw();

	// 水平方向(XZ平面)で見て、指定した座標が台の上面の範囲に入っているか
	bool IsInsideTop(const Vector3& position) const;

	// 台の上面の高さを取得
	float GetTopHeight() const{ return spawnPosition_.y; }
	// プレイヤーを立たせる開始位置(台の上面の中央)を取得
	const Vector3& GetSpawnPosition() const{ return spawnPosition_; }

private:
	// 描画用の3Dオブジェクト
	std::unique_ptr<Obj3D> obj_;

	// プレイヤーを立たせる開始位置(台の上面の中央)
	Vector3 spawnPosition_ = {0.0f, 0.0f, 0.0f};
	// 台の向き(XZ平面の正規化ベクトル。前方向と右方向)
	Vector3 forwardXZ_ = {0.0f, 0.0f, 1.0f};
	Vector3 rightXZ_ = {1.0f, 0.0f, 0.0f};

	// 台の左右の幅の半分
	static constexpr float kHalfWidth = 2.0f;
	// 台の前後の長さの半分
	static constexpr float kHalfLength = 4.0f;
	// 台の厚みの半分
	static constexpr float kHalfHeight = 0.5f;
	// 台の前の端を、レールの始点からどれだけ先まで重ねるか(重ねた範囲に入ればレールに乗れる)
	static constexpr float kRailOverlapLength = 1.5f;
};
