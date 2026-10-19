#pragma once

#include "MyMath.h"
#include <cmath>
#include <numbers>

/// <summary>
/// カメラの位置と向き(演出のカメラワークで、複数の画を混ぜるのに使う)
/// 向きはカメラの Euler 角(X: 下向きが正、Y: +Z から +X へ回る向きが正)
/// </summary>
struct CameraPose{
	Vector3 position = {0.0f, 0.0f, 0.0f};
	Vector3 rotate = {0.0f, 0.0f, 0.0f};
};

namespace CameraPoseMath{
	// 1周分の角度(ラジアン)
	inline constexpr float kFullTurn = 2.0f * std::numbers::pi_v<float>;
	// 画面の中央の UV 座標(カメラの後ろにある点を変換しようとしたときに代わりに使う)
	inline constexpr Vector2 kScreenCenterUv = {0.5f, 0.5f};
	// 描画先の座標(-1〜1、上向きが+Y)を、画面の UV 座標(0〜1、左上が原点で下向きが+V)に直す倍率
	inline constexpr float kClipToUvScale = 0.5f;

	// 指定した位置から注目する位置を向くカメラの向きを求める
	inline Vector3 LookRotation(const Vector3& from,const Vector3& to){
		Vector3 direction = Normalize(to - from);
		float horizontalLength = std::sqrt(direction.x * direction.x + direction.z * direction.z);
		return {std::atan2(-direction.y,horizontalLength), std::atan2(direction.x,direction.z), 0.0f};
	}

	// 指定した位置から注目する位置を見るカメラの位置と向き
	inline CameraPose LookAt(const Vector3& position,const Vector3& target){
		return {position, LookRotation(position,target)};
	}

	// 角度を、遠回りしない向きで補間する(差を -π〜π に収めてから補間し、1周回ってしまわないようにする)
	inline float LerpAngle(float from,float to,float t){
		return from + std::remainder(to - from,kFullTurn) * t;
	}

	// 2つのカメラの位置と向きを補間する(t=0 で from、t=1 で to と完全に一致する)
	inline CameraPose Lerp(const CameraPose& from,const CameraPose& to,float t){
		return {
			::Lerp(from.position,to.position,t),
			{LerpAngle(from.rotate.x,to.rotate.x,t), LerpAngle(from.rotate.y,to.rotate.y,t), LerpAngle(from.rotate.z,to.rotate.z,t)}
		};
	}

	/// <summary>
	/// ワールド座標を、画面の UV 座標に変換する(放射ブラーの中心を決めるのに使う)
	/// </summary>
	/// <param name="position">ワールド座標</param>
	/// <param name="viewProjection">カメラのビュー・プロジェクション行列</param>
	inline Vector2 WorldToScreenUv(const Vector3& position,const Matrix4x4& viewProjection){
		// 行ベクトル × 行列の順で掛け、同次座標(x, y, w)を求める
		const Matrix4x4& m = viewProjection;
		float clipX = position.x * m.m[0][0] + position.y * m.m[1][0] + position.z * m.m[2][0] + m.m[3][0];
		float clipY = position.x * m.m[0][1] + position.y * m.m[1][1] + position.z * m.m[2][1] + m.m[3][1];
		float clipW = position.x * m.m[0][3] + position.y * m.m[1][3] + position.z * m.m[2][3] + m.m[3][3];
		if(clipW <= 0.0f){
			return kScreenCenterUv;
		}
		return {kClipToUvScale + clipX / clipW * kClipToUvScale, kClipToUvScale - clipY / clipW * kClipToUvScale};
	}
}
