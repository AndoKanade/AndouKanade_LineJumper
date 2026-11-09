#pragma once
#include "DXCommon.h"
#include "MyMath.h" // Vector3, Vector4 等のため

struct DirectionalLight{
    Vector4 color;
    Vector3 direction;
    float intensity;
    // 光が当たらない面の明るさの下限(光が真正面から当たる面を 1 としたときの割合。0 なら真っ暗、これより明るい面は変えない)
    float shadowBrightness;
    // シェーダーの定数バッファ(16バイト単位)に大きさをそろえるための詰め物
    float padding[3];
};

class ModelCommon{
public:
    void Initialize(DXCommon* dxCommon);
    DXCommon* GetDxCommon() const{ return dxCommon_; }
    ID3D12Resource* GetLightResource() const{ return lightResource.Get(); }
    DirectionalLight* GetLightData() const{ return lightData; }

private:
    DXCommon* dxCommon_;
    // ライト用のリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource;
    // Map用ポインタ
    DirectionalLight* lightData = nullptr;
};