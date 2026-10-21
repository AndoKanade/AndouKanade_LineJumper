#pragma once
#include "MyMath.h"
#include "CameraManager.h"
#include "Camera.h"
#include "EditorContext.h"
#include "WinAPI.h"
#include <cmath>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

/// <summary>
/// EditモードのSceneビュー上で、マウスで物を選んで動かすための共通処理(ヘッダーオンリー)
/// マウス位置から画面奥へ伸びるレイを求め、レイと平面の交点で動かす先を決める。
/// </summary>
namespace EditorPicking{

	// マウス位置から画面奥へ伸びるレイ
	struct PickRay{
		Vector3 origin;    // レイの始点(ニアクリップ面上の点)
		Vector3 direction; // レイの正規化された方向
	};

	// レイと平面が平行かどうかを判定するしきい値(内積の絶対値がこの値未満なら平行とみなす)
	inline constexpr float kRayPlaneEpsilon = 1e-6f;

	// 座標に行列を掛け、同次座標のwで割る(NDCからワールド座標への逆変換に使用)
	inline Vector3 TransformWithDivide(const Vector3& v,const Matrix4x4& m){
		float x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0];
		float y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1];
		float z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2];
		float w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];

		// wが0に近い異常値のときは割らずにそのまま返す
		if(std::fabs(w) < kRayPlaneEpsilon){
			return {x, y, z};
		}
		return {x / w, y / w, z / w};
	}

	// レイと平面の交点を求める(平行、または平面がレイの後方にある場合はfalse)
	inline bool IntersectRayPlane(const PickRay& ray,const Vector3& planePoint,const Vector3& planeNormal,Vector3& outPos){
		float denom = Dot(ray.direction,planeNormal);
		if(std::fabs(denom) < kRayPlaneEpsilon){
			return false;
		}

		float t = Dot(planePoint - ray.origin,planeNormal) / denom;
		if(t < 0.0f){
			return false;
		}

		outPos = ray.origin + ray.direction * t;
		return true;
	}

	// Editモード中にゲーム画面が実際に描かれている矩形(レターボックス適用後)を求める
	// Application.cppのビューポート計算と同じ手順で求めることで、マウス座標と描画結果を一致させる
	inline bool ComputeGameViewportRect(float& outX,float& outY,float& outW,float& outH){
		const EditorRect& sceneRect = EditorContext::GetInstance()->GetSceneViewRect();
		if(sceneRect.w <= 0.0f || sceneRect.h <= 0.0f){
			return false;
		}

		const float targetAspect = static_cast<float>(WinAPI::kClientWidth) / static_cast<float>(WinAPI::kClientHeight);
		const float rectAspect = sceneRect.w / sceneRect.h;

		float viewWidth = 0.0f;
		float viewHeight = 0.0f;
		if(rectAspect > targetAspect){
			// 領域が横長 → 高さを合わせて左右をレターボックス
			viewHeight = sceneRect.h;
			viewWidth = viewHeight * targetAspect;
		} else{
			// 領域が縦長 → 幅を合わせて上下をレターボックス
			viewWidth = sceneRect.w;
			viewHeight = viewWidth / targetAspect;
		}

		outX = sceneRect.x + (sceneRect.w - viewWidth) * 0.5f;
		outY = sceneRect.y + (sceneRect.h - viewHeight) * 0.5f;
		outW = viewWidth;
		outH = viewHeight;
		return true;
	}

	// 現在のマウス位置から伸びるレイを求める(ゲーム画面の外・カメラ未取得のときはfalse)
	inline bool ComputeMouseRay(PickRay& outRay){
#ifdef USE_IMGUI
		float viewX = 0.0f, viewY = 0.0f, viewW = 0.0f, viewH = 0.0f;
		if(!ComputeGameViewportRect(viewX,viewY,viewW,viewH)){
			return false;
		}

		// ゲーム画面の外にマウスがあるときは対象外
		ImVec2 mousePos = ImGui::GetMousePos();
		if(mousePos.x < viewX || mousePos.x > viewX + viewW || mousePos.y < viewY || mousePos.y > viewY + viewH){
			return false;
		}

		Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
		if(!camera){
			return false;
		}

		// ビューポート内のマウス位置を正規化デバイス座標(NDC)へ変換する
		float ndcX = (mousePos.x - viewX) / viewW * 2.0f - 1.0f;
		float ndcY = 1.0f - (mousePos.y - viewY) / viewH * 2.0f;

		// ビュープロジェクション行列の逆行列で、NDCの near/far 2点をワールド座標へ戻す
		// DirectXの深度範囲は 0(ニアクリップ面)〜1(ファークリップ面)
		Matrix4x4 invViewProjection = Inverse(camera->GetViewProjectionMatrix());
		Vector3 nearPos = TransformWithDivide({ndcX, ndcY, 0.0f},invViewProjection);
		Vector3 farPos = TransformWithDivide({ndcX, ndcY, 1.0f},invViewProjection);

		outRay.origin = nearPos;
		outRay.direction = Normalize(farPos - nearPos);
		return true;
#else
		(void)outRay;
		return false;
#endif
	}

} // namespace EditorPicking
