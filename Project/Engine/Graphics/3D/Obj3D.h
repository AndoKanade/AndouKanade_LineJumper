#pragma once
#include "MyMath.h"
#include "Model.h"
#include "Skeleton.h"
#include "SkinCluster.h"
#include "Animation.h"
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl.h>
#include <memory>

class Obj3dCommon;
class Model;
class Camera;
class PaintCanvas;

class Obj3D{
public:
	struct TransformationMatrix{
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverseTranspose;
	};

	void Initialize(Obj3dCommon* object3dCommon);
	void Update();
	void Draw();

	// Setter
	void SetParent(const std::weak_ptr<Obj3D>& parent);
	void SetModel(Model* model){ this->model = model; }
	void SetModel(const std::string& filePath);
	void SetCamera(Camera* camera){ this->camera = camera; }
	void SetIsSkinning(bool isSkinning){ isSkinning_ = isSkinning; }
	// 遠くを空の色に溶け込ませる距離に、全オブジェクト共通の値(Obj3dCommon)を使うか
	// falseにすると描画時に書き換えなくなるため、マテリアルに独自の距離を設定して使える
	void SetUseSceneDistanceFade(bool use){ usesSceneDistanceFade_ = use; }
	// インクの塗りテクスチャを設定する(nullptr なら塗られていない見た目で描く)
	// モデルの UV が面どうしで重ならないように展開されていること
	void SetPaintCanvas(const PaintCanvas* paintCanvas){ paintCanvas_ = paintCanvas; }
	// 描画するインデックスの範囲を指定する(1つのモデルの一部だけを描くときに使う。既定はモデル全体)
	void SetDrawIndexRange(uint32_t startIndex,uint32_t indexCount){ drawStartIndex_ = startIndex; drawIndexCount_ = indexCount; }
	void SetTexture(const std::string& filePath){
		if(model){
			model->SetTexture(filePath);
		}
	}

	void SetScale(const Vector3& scale){ transform.scale = scale; }
	void SetRotate(const Vector3& rotate){ transform.rotate = rotate; isUseQuaternion_ = false; }	
	void SetTranslate(const Vector3& translate){ transform.translate = translate; }
	void SetQuaternion(const Quaternion& quat){ quaternion_ = quat; isUseQuaternion_ = true; }

	void LoadAnimation(const std::string& directoryPath,const std::string& filename);

	// Getter
	const Vector3& GetScale() const{ return transform.scale; }
	const Vector3& GetRotate() const{ return transform.rotate; }
	const Vector3& GetTranslate() const{ return transform.translate; }
	const Matrix4x4& GetWorldMatrix() const{ return transformationMatrixData->World; }
	Model* GetModel() const{ return model; }
	Model::Material* GetMaterial() const{ return materialData; }
	Skeleton& GetSkeleton(){ return skeleton_; }

	/// <summary>
	/// モデルの境界球をワールド座標系へ変換して取得する(当たり判定用)
	/// 直前のUpdate()で計算したワールド行列を使うため、位置・回転・拡大率の変更後はUpdate()してから呼ぶこと
	/// </summary>
	Model::BoundingSphere GetWorldBoundingSphere() const;

private:
	void CreateTransformationMatrixData();
	void CreateMaterialData();

	Obj3dCommon* object3dCommon = nullptr;
	Model* model = nullptr;
	Camera* camera = nullptr;
	std::weak_ptr<Obj3D> parent_;

	// 行列用
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	TransformationMatrix* transformationMatrixData = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Model::Material* materialData = nullptr;

	Transform transform;
	Quaternion quaternion_ = {0.0f, 0.0f, 0.0f, 1.0f};
	bool isUseQuaternion_ = false;

	Skeleton skeleton_;
	SkinCluster skinCluster_;
	Animation animation_;
	float animationTime_ = 0.0f;
	bool isSkinning_ = false;

	// 遠くを空の色に溶け込ませる距離に、全オブジェクト共通の値を使うか
	bool usesSceneDistanceFade_ = true;

	// インクの塗りテクスチャ(持ち主は PaintManager)
	const PaintCanvas* paintCanvas_ = nullptr;

	// 描画するインデックスの範囲(既定はモデル全体)
	uint32_t drawStartIndex_ = 0;
	uint32_t drawIndexCount_ = Model::kDrawAllIndices;
};