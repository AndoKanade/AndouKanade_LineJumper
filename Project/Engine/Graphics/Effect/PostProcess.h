#pragma once
#include "DXCommon.h"
#include "Mymath.h"
#include <d3d12.h>
#include <wrl.h>
#include <map>
#include <string>

class PostProcess{
public:
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    // ポストプロセスの種類
    enum class Type{
        PostProcess,
        BoxFilter,
        Grayscale,
        Vignette,
        GaussianBlur,
        LuminanceOutline,
        DepthOutline,
		RadialBlur,
		Dissolve,
        Random,
        Glitch,
        DamageInk   // 体力が減るほど画面の縁から敵のインクが迫ってくる(プレイヤーの体力の表現)
    };

    // 定数バッファ構造体
    struct PostProcessData{
        int32_t kernelSize;
        float vignetteIntensity;
        float vignetteScale;
        float padding;
        Vector2 radialBlurCenter; 
        float radialBlurWidth;    
		float dissolveThreshold;
        float dissolveEdgeWidth;
        Vector3 dissolveEdgeColor;
        float randomIntensity;
        float randomTime;
        float damageAmount; // 体力の減り具合(0: 満タン 〜 1: 尽きた)。画面の縁のインクの広がりに使う
        float damageFlash;  // 被弾した瞬間の画面の色づき(0〜1)
    };

public:
    void Initialize(DXCommon* dxCommon);
    void Draw(ID3D12GraphicsCommandList* commandList,D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,D3D12_GPU_DESCRIPTOR_HANDLE depthTextureHandle,Type type);

    // パラメータ設定
    void SetKernelSize(int32_t k){ if(constMap_) constMap_->kernelSize = k; }
    void SetVignetteIntensity(float intensity){ if(constMap_) constMap_->vignetteIntensity = intensity; }
    void SetVignetteScale(float scale){ if(constMap_) constMap_->vignetteScale = scale; }
    void SetRadialBlurCenter(const Vector2& center){ if(constMap_) constMap_->radialBlurCenter = center; }
    void SetRadialBlurWidth(float width){ if(constMap_) constMap_->radialBlurWidth = width; }
    void SetDissolveThreshold(float threshold){ if(constMap_) constMap_->dissolveThreshold = threshold; }
    void SetDissolveEdgeWidth(float width){ if(constMap_) constMap_->dissolveEdgeWidth = width; }
    void SetDissolveEdgeColor(const Vector3& color){ if(constMap_) constMap_->dissolveEdgeColor = color; }
	void SetRandomIntensity(float intensity){ if(constMap_) constMap_->randomIntensity = intensity; }   
    void SetRandomTime(float time){ if(constMap_) constMap_->randomTime = time; }
    void SetDamageAmount(float amount){ if(constMap_) constMap_->damageAmount = amount; }
    void SetDamageFlash(float flash){ if(constMap_) constMap_->damageFlash = flash; }

private:
    void CreateRootSignature(ID3D12Device* device);
    void CreatePipelineState(ID3D12Device* device,DXCommon* dxCommon,Type type,const std::wstring& filename);

private:
    ComPtr<ID3D12RootSignature> rootSignature_;
    std::map<Type,ComPtr<ID3D12PipelineState>> pipelineStates_;
    ComPtr<ID3D12Resource> constBuff_;
    PostProcessData* constMap_ = nullptr;
};