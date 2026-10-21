#pragma once
#include "DXCommon.h"
#include <d3d12.h>
#include <wrl.h>
#include "Camera.h"
#include "MyMath.h"
#include "ModelCommon.h"

struct PointLight{
	Vector4 color;
	Vector3 position;
	float intensity;
	float radius;
	float decay;
	float padding[2];
};

struct SpotLight{
	Vector4 color;
	Vector3 position;
	float intensity;
	Vector3 direction;
	float distance;
	float cosAngle;
	float decay;
	float cosFalloffStart;
	float padding[2];
};

// --- Obj3dCommonクラス ---

class Obj3dCommon{
public: // 定数

	// インクの塗りテクスチャ(t2)を渡すルートパラメータの番号(通常用・スキニング用)
	static constexpr UINT kPaintTextureRootIndex = 8;
	static constexpr UINT kSkinningPaintTextureRootIndex = 9;

public: // 外部から呼び出すもの

	// 初期化
	void Initialize(DXCommon* dxCommon);

	// 描画設定
	void Draw();

	// セッター
	void SetDefaultCamera(Camera* camera){ this->defaultCamera_ = camera; }
	// 遠くの3Dオブジェクトを空の色に溶け込ませる距離(全オブジェクト共通。End <= Startのときは溶け込ませない)
	// 独自の距離を使うオブジェクト(Obj3D::SetUseSceneDistanceFade(false))には効かない
	void SetSceneDistanceFade(float start,float end){ sceneDistanceFadeStart_ = start; sceneDistanceFadeEnd_ = end; }

	// ゲッター
	ID3D12RootSignature* GetRootSignature() const{ return rootSignature.Get(); }
	ID3D12PipelineState* GetGraphicsPipelineState() const{ return graphicsPipelineState.Get(); }
	DXCommon* GetDxCommon() const{ return dxCommon_; }
	Camera* GetDefaultCamera() const{ return defaultCamera_; }
	ID3D12PipelineState* GetSkinningGraphicsPipelineState() const{ return skinningGraphicsPipelineState.Get(); }
	ID3D12RootSignature* GetSkinningRootSignature() const{ return skinningRootSignature.Get(); }
	// 追加
	D3D12_GPU_VIRTUAL_ADDRESS GetPointLightDataGPU() const{ return pointLightResource_->GetGPUVirtualAddress(); }
	D3D12_GPU_VIRTUAL_ADDRESS GetSpotLightDataGPU() const{ return spotLightResource_->GetGPUVirtualAddress(); }

	ID3D12RootSignature* GetComputeRootSignature() const{ return computeRootSignature.Get(); }
	ID3D12PipelineState* GetComputePipelineState() const{ return computePipelineState.Get(); }

	// 全オブジェクト共通の、遠くを空の色に溶け込ませる距離
	float GetSceneDistanceFadeStart() const{ return sceneDistanceFadeStart_; }
	float GetSceneDistanceFadeEnd() const{ return sceneDistanceFadeEnd_; }

	// ライトデータ取得
	DirectionalLight* GetDirectionalLightData(){ return directionalLightData_; }
	PointLight* GetPointLightData(){ return pointLightData_; }
	SpotLight* GetSpotLightData(){ return spotLightData_; }

private: // 内部関数
	void CreateRootSignature();
	void CreateSkinningRootSignature();
	void CreateGraphicsPipelineState();
	void CreateComputePipelineState();

	void CreateUAV(ID3D12Device* device,ID3D12Resource* resource,UINT numElements,UINT stride,D3D12_CPU_DESCRIPTOR_HANDLE handle);

private: // メンバ変数

	DXCommon* dxCommon_ = nullptr;
	Camera* defaultCamera_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;         // 通常用
	Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningRootSignature; // スキニング用

	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningGraphicsPipelineState;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature;

	// カメラリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	CameraForGPU* cameraData_ = nullptr;

	// 平行光源リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	DirectionalLight* directionalLightData_ = nullptr;

	// 点光源リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
	PointLight* pointLightData_ = nullptr;

	// スポットライトリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
	SpotLight* spotLightData_ = nullptr;

	// 全オブジェクト共通の、遠くを空の色に溶け込ませる距離(初期値は溶け込ませない)
	float sceneDistanceFadeStart_ = 0.0f;
	float sceneDistanceFadeEnd_ = 0.0f;
};