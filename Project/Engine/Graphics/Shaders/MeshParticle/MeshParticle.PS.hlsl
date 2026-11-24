#include "MeshParticle.hlsli"

// 同じモデルをまとめて描くときのピクセルシェーダー
// Obj3D(Object3d.PS.hlsl)と同じ見た目にするため、ライティングと遠くを空に溶け込ませる計算は Object3d.PS.hlsl と同じ式にしている
// (パーティクルには使わない、インクの塗り・現れる演出・環境マップの映り込みは入れていない)
// Object3d.PS.hlsl のライティングを変えたときは、こちらも合わせて変える

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float32_t intensity;
    // 光が当たらない面の明るさの下限
    float32_t shadowBrightness;
};

struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t radius;
    float32_t decay;
    float32_t padding[2];
};

struct SpotLight
{
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t3 direction;
    float32_t distance;
    float32_t cosAngle;
    float32_t decay;
    float32_t cosFalloffStart;
    float32_t padding[2];
};

struct Camera
{
    float32_t3 worldPosition;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

ConstantBuffer<DrawConstants> gDraw : register(b0);
ConstantBuffer<ViewConstants> gView : register(b1);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);
ConstantBuffer<Camera> gCamera : register(b3);
ConstantBuffer<PointLight> gPointLight : register(b4);
ConstantBuffer<SpotLight> gSpotLight : register(b5);
Texture2D<float32_t4> gTexture : register(t0);
TextureCube<float32_t4> gEnvironmentTexture : register(t1);
SamplerState gSampler : register(s0);

PixelShaderOutput main(MeshParticleVertexOutput input)
{
    PixelShaderOutput output;

    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    float32_t3 baseColor = input.color.rgb * textureColor.rgb;
    float32_t3 finalColor = baseColor;

    if (gDraw.enableLighting != 0)
    {
        float32_t3 normal = normalize(input.normal);
        float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        // --- 1. 平行光源 ---
        float32_t3 dirFactor = normalize(-gDirectionalLight.direction);
        float32_t dirCos = pow(dot(normal, dirFactor) * 0.5f + 0.5f, 2.0f);
        // 光が当たらない面も真っ暗にせず、shadowBrightness の明るさまで持ち上げる
        dirCos = max(dirCos, gDirectionalLight.shadowBrightness);
        float32_t3 dirDiffuse = baseColor * gDirectionalLight.color.rgb * dirCos * gDirectionalLight.intensity;
        float32_t dirSpecularPow = pow(saturate(dot(reflect(-dirFactor, normal), toEye)), gView.shininess);
        float32_t3 dirSpecular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * dirSpecularPow;

        // --- 2. 点光源 ---
        float32_t3 pointVec = input.worldPosition - gPointLight.position;
        float32_t pDistance = length(pointVec);
        float32_t3 pFactor = normalize(-pointVec);
        float32_t pAttenuation = pow(saturate(1.0f - (pDistance / gPointLight.radius)), gPointLight.decay);
        float32_t pCos = pow(dot(normal, pFactor) * 0.5f + 0.5f, 2.0f);
        float32_t3 pDiffuse = baseColor * gPointLight.color.rgb * pCos * gPointLight.intensity * pAttenuation;
        float32_t pSpecularPow = pow(saturate(dot(reflect(-pFactor, normal), toEye)), gView.shininess);
        float32_t3 pSpecular = gPointLight.color.rgb * gPointLight.intensity * pSpecularPow * pAttenuation;

        // --- 3. スポットライト ---
        float32_t3 spotVec = input.worldPosition - gSpotLight.position;
        float32_t sDistance = length(spotVec);
        float32_t3 spotDirP = normalize(spotVec);
        float32_t sCosAngle = dot(spotDirP, normalize(gSpotLight.direction));
        float32_t sFalloff = saturate((sCosAngle - gSpotLight.cosAngle) / (gSpotLight.cosFalloffStart - gSpotLight.cosAngle));
        float32_t sAttenuation = pow(saturate(1.0f - (sDistance / gSpotLight.distance)), gSpotLight.decay);
        float32_t3 sFactor = -spotDirP;
        float32_t sCos = pow(dot(normal, sFactor) * 0.5f + 0.5f, 2.0f);
        float32_t3 sDiffuse = baseColor * gSpotLight.color.rgb * sCos * gSpotLight.intensity * sAttenuation * sFalloff;
        float32_t sSpecularPow = pow(saturate(dot(reflect(-sFactor, normal), toEye)), gView.shininess);
        float32_t3 sSpecular = gSpotLight.color.rgb * gSpotLight.intensity * sSpecularPow * sAttenuation * sFalloff;

        finalColor = dirDiffuse + pDiffuse + sDiffuse + dirSpecular + pSpecular + sSpecular;
    }

    // --- 遠くを空に溶け込ませる ---
    // 遠くほど色を「その方向に見える空(スカイボックス)の色」へ近づける(Object3d.PS.hlsl と同じ)
    if (gView.distanceFadeEnd > gView.distanceFadeStart)
    {
        float32_t3 fromCamera = input.worldPosition - gCamera.worldPosition;
        float32_t fadeRate = smoothstep(gView.distanceFadeStart, gView.distanceFadeEnd, length(fromCamera));
        float32_t3 skyColor = gEnvironmentTexture.Sample(gSampler, normalize(fromCamera)).rgb;
        finalColor = lerp(finalColor, skyColor, fadeRate);
    }

    output.color.rgb = finalColor;
    output.color.a = input.color.a * textureColor.a;

    // アルファテスト(テクスチャの透明な部分と、すっかり透明になった粒は描かない)
    if (textureColor.a <= gView.alphaCutoff || output.color.a == 0.0f)
    {
        discard;
    }

    return output;
}
