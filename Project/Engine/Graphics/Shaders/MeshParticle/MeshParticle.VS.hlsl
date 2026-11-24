#include "MeshParticle.hlsli"

// 同じモデルをまとめて描くときの頂点シェーダー
// 1つずつの行列と色は、コンピュートシェーダーで求めた配列から SV_InstanceID で読む

ConstantBuffer<DrawConstants> gDraw : register(b0);
ConstantBuffer<ViewConstants> gView : register(b1);
StructuredBuffer<MeshParticleInstance> gInstances : register(t0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

MeshParticleVertexOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    // SV_InstanceID は描画の開始位置に関係なく 0 から数えるため、この種類の入れ物の先頭の番号を足す
    MeshParticleInstance instance = gInstances[gDraw.instanceOffset + instanceId];

    MeshParticleVertexOutput output;
    float32_t4 worldPosition = mul(input.position, instance.world);
    output.position = mul(worldPosition, gView.viewProjection);
    output.worldPosition = worldPosition.xyz;
    output.normal = normalize(mul(input.normal, (float32_t3x3) instance.worldInverseTranspose));
    output.texcoord = input.texcoord;
    output.color = instance.color;
    return output;
}
