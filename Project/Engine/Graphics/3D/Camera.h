#pragma once
#include "MyMath.h"
#include <wrl.h>
#include <d3d12.h>

struct CameraForGPU{
	Vector3 worldPosition;
};

class Camera{
public:
	// コンストラクタ
	Camera();

	// 更新処理
	void Update();
	void Initialize(ID3D12Device* device);

	// --- Getter ---
	const Matrix4x4& GetWorldMatrix() const{ return worldMatrix; }
	const Matrix4x4& GetViewMatrix() const{ return viewMatrix; }
	const Matrix4x4& GetProjectionMatrix() const{ return projectionMatrix; }
	const Matrix4x4& GetViewProjectionMatrix() const{ return viewProjectionMatrix; }
	const Vector3& GetRotate() const{ return transform.rotate; }
	const Vector3& GetTranslate() const{ return transform.translate; }
	float GetFov() const{ return fov; }
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const{
		return resource->GetGPUVirtualAddress();
	}

	// --- Setter ---
	void SetRotate(const Vector3& rotate){ transform.rotate = rotate; }
	void SetTranslate(const Vector3& translate){ transform.translate = translate; }
	void SetFov(float fov){ this->fov = fov; }
	void SetAspectRatio(float aspectRatio){ this->aspectRatio = aspectRatio; }
	void SetNearClip(float nearClip){ this->nearClip = nearClip; }
	void SetFarClip(float farClip){ this->farClip = farClip; }

private:
	// トランスフォーム（位置・回転）
	Transform transform;

	// 行列データ
	Matrix4x4 worldMatrix;
	Matrix4x4 viewMatrix;
	Matrix4x4 projectionMatrix;
	Matrix4x4 viewProjectionMatrix;

	// 射影行列の計算用パラメータ
	float fov;          // 視野角
	float aspectRatio;  // アスペクト比
	float nearClip;     // 近平面
	float farClip;      // 遠平面

	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	CameraForGPU* data = nullptr;
};