#include "ModelCommon.h"

void ModelCommon::Initialize(DXCommon* dxCommon){
    dxCommon_ = dxCommon;

    // --- ライトリソースの生成 ---
    // 256バイトアライメントでサイズ計算
    size_t sizeInBytes = (sizeof(DirectionalLight) + 0xff) & ~0xff;
    lightResource = dxCommon_->CreateBufferResource(sizeInBytes);

    // 書き込み用ポインタを保持 (Map)
    lightResource->Map(0,nullptr,reinterpret_cast<void**>(&lightData));

    // デフォルトのライト設定 (これがないと真っ暗になります)
    lightData->color = {1.0f, 1.0f, 1.0f, 1.0f};     // 白光
    lightData->direction = {0.5f, -0.7f, 0.5f};   // 斜め下
    lightData->intensity = 1.0f;                      // 明るさ1.0
    lightData->shadowBrightness = 0.0f;               // 光が当たらない面は今までどおり暗くする
}