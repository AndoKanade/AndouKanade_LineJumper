#include "Object3d.hlsli"

// --- 構造体定義 ---
struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t alphaCutoff;
    float32_t4x4 uvTransform;
    float32_t shininess;
    float32_t environmentCoefficient;
    int32_t useBlinnPhong;
    // カメラからの距離がこの間で、色を背景の空の色へ少しずつ近づける(End <= Start のときは近づけない)
    float32_t distanceFadeStart;
    float32_t distanceFadeEnd;
    // 現れる演出の見せ方(0: ふつうに描く 1: 横じまが太くなって埋まる 2: 光る境界線が下から上へ通る。Model::RevealStyle と同じ番号)
    int32_t revealStyle;
    // 現れる演出の進み具合(0: まったく見えない 〜 1: すべて見える)
    float32_t revealProgress;
    // 現れる演出をかける範囲の、下端のワールドの高さと、上端までの高さ
    float32_t revealBottom;
    float32_t revealHeight;
};

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float32_t intensity;
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

// --- 定数バッファ・テクスチャ ---
ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);
ConstantBuffer<Camera> gCamera : register(b3);
ConstantBuffer<PointLight> gPointLight : register(b4);
ConstantBuffer<SpotLight> gSpotLight : register(b5);
Texture2D<float32_t4> gTexture : register(t0);
TextureCube<float32_t4> gEnvironmentTexture : register(t1);
// インクの塗りテクスチャ(PaintCanvas)。塗れないオブジェクトには何も塗られていない透明なテクスチャが渡される
Texture2D<float32_t4> gPaintTexture : register(t2);
SamplerState gSampler : register(s0);

// --- インクの塗りの見た目 ---
// 塗りテクスチャのαは、インクの輪郭でちょうどこの値になるなめらかな値で塗られている
static const float32_t kInkEdgeValue = 0.5f;
// 輪郭をぼかす幅の最小値(画面上でαがほとんど変わらないほど拡大したときも、輪郭がギザギザにならないようにする)
static const float32_t kInkMinEdgeWidth = 0.01f;
// インクの盛り上がり: 輪郭から内側へαがこの割合だけ増えるまでの間を斜面にし、内側は平らにする
static const float32_t kInkBevelScale = 2.5f;
// 盛り上がりの斜面で法線を傾ける強さ
static const float32_t kInkBumpStrength = 0.6f;
// UV の変化が無い(UV が1点に集まった)モデルで、接線の長さが0になって割り算が壊れないようにする最小値
static const float32_t kTangentEpsilon = 1e-12f;
// インクの表面はつやがあるので、鏡面反射の鋭さと強さを上げる
static const float32_t kInkShininess = 64.0f;
static const float32_t kInkSpecularScale = 1.6f;

// --- 現れる演出 ---
// 見せ方の番号(Model::RevealStyle と同じ)
static const int32_t kRevealStripes = 1;
static const int32_t kRevealSweep = 2;
// 横じまの本数(モデルの下端から上端までに並べる数)
static const float32_t kRevealStripeCount = 9.0f;
// 境界線の太さ(モデルの高さに対する割合)と、線の色(光って見えるよう白にする)
static const float32_t kRevealEdgeWidth = 0.06f;
static const float32_t3 kRevealEdgeColor = float32_t3(1.0f, 1.0f, 1.0f);
// 範囲の高さが0のときに割り算が壊れないようにする最小値
static const float32_t kRevealMinHeight = 1e-4f;

// インクの盛り上がりの高さ(輪郭の外は0、輪郭から少し内側で1になり、そこから内側は平ら)
float32_t InkHeight(float32_t field)
{
    return saturate((field - kInkEdgeValue) * kInkBevelScale);
}

// --- メイン関数 ---
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV Transform適用 & テクスチャサンプリング
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // --- インクの塗り ---
    // 塗りテクスチャはモデルの UV そのままの位置に塗られているため、UV Transform は掛けずに読む
    // 補間して読んだαを輪郭の値でしきい値処理し、画面上の1ピクセル分の幅でぼかして、テクスチャの解像度より細かくなめらかな輪郭にする
    float32_t4 paint = gPaintTexture.Sample(gSampler, input.texcoord);
    float32_t inkEdgeWidth = max(fwidth(paint.a), kInkMinEdgeWidth);
    float32_t inkMask = smoothstep(kInkEdgeValue - inkEdgeWidth, kInkEdgeValue + inkEdgeWidth, paint.a);

    // インクの輪郭が盛り上がって見えるよう、隣のピクセルとの高さの差で法線を傾ける
    // 接線は、画面上で隣のピクセルとのワールド座標・UV の差から求める(頂点に接線を持たないモデルでも使える)
    float32_t3 surfaceNormal = normalize(input.normal);
    {
        float32_t3 dpdx = ddx(input.worldPosition);
        float32_t3 dpdy = ddy(input.worldPosition);
        float32_t2 duvdx = ddx(input.texcoord);
        float32_t2 duvdy = ddy(input.texcoord);
        float32_t3 dpdyPerp = cross(dpdy, surfaceNormal);
        float32_t3 dpdxPerp = cross(surfaceNormal, dpdx);
        float32_t3 tangent = dpdyPerp * duvdx.x + dpdxPerp * duvdy.x;
        float32_t3 bitangent = dpdyPerp * duvdx.y + dpdxPerp * duvdy.y;
        float32_t tangentScale = rsqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), kTangentEpsilon));

        // 塗りテクスチャの左右・上下の1ピクセル隣の高さの差(U・V 方向の傾き)
        float32_t2 paintSize;
        gPaintTexture.GetDimensions(paintSize.x, paintSize.y);
        float32_t2 texel = 1.0f / paintSize;
        float32_t slopeU = InkHeight(gPaintTexture.Sample(gSampler, input.texcoord + float32_t2(texel.x, 0.0f)).a) -
                           InkHeight(gPaintTexture.Sample(gSampler, input.texcoord - float32_t2(texel.x, 0.0f)).a);
        float32_t slopeV = InkHeight(gPaintTexture.Sample(gSampler, input.texcoord + float32_t2(0.0f, texel.y)).a) -
                           InkHeight(gPaintTexture.Sample(gSampler, input.texcoord - float32_t2(0.0f, texel.y)).a);
        surfaceNormal = normalize(surfaceNormal - (tangent * slopeU + bitangent * slopeV) * (tangentScale * kInkBumpStrength));
    }
    float32_t shininess = lerp(gMaterial.shininess, kInkShininess, inkMask);
    float32_t specularScale = lerp(1.0f, kInkSpecularScale, inkMask);

    // 基本色の決定
    float32_t3 finalColor = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t3 baseColor = lerp(gMaterial.color.rgb * textureColor.rgb, paint.rgb, inkMask);

    if (gMaterial.enableLighting != 0)
    {
        float32_t3 normal = surfaceNormal;
        float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        // --- 1. Directional Light ---
        float32_t3 dirFactor = normalize(-gDirectionalLight.direction);
        float32_t dirCos = pow(dot(normal, dirFactor) * 0.5f + 0.5f, 2.0f);
        float32_t3 dirDiffuse = baseColor * gDirectionalLight.color.rgb * dirCos * gDirectionalLight.intensity;
        
        // [修正] 鏡面反射計算の切り替え
        float32_t dirSpecularPow;
        if (gMaterial.useBlinnPhong != 0)
        {
            float32_t3 halfVector = normalize(dirFactor + toEye);
            dirSpecularPow = pow(saturate(dot(normal, halfVector)), shininess);
        }
        else
        {
            dirSpecularPow = pow(saturate(dot(reflect(-dirFactor, normal), toEye)), shininess);
        }
        float32_t3 dirSpecular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * dirSpecularPow;

        // --- 2. Point Light ---
        float32_t3 pointVec = input.worldPosition - gPointLight.position;
        float32_t pDistance = length(pointVec);
        float32_t3 pFactor = normalize(-pointVec);
        float32_t pAttenuation = pow(saturate(1.0f - (pDistance / gPointLight.radius)), gPointLight.decay);
        
        float32_t pCos = pow(dot(normal, pFactor) * 0.5f + 0.5f, 2.0f);
        float32_t3 pDiffuse = baseColor * gPointLight.color.rgb * pCos * gPointLight.intensity * pAttenuation;
        
        // [修正] 鏡面反射計算の切り替え
        float32_t pSpecularPow;
        if (gMaterial.useBlinnPhong != 0)
        {
            float32_t3 halfVector = normalize(pFactor + toEye);
            pSpecularPow = pow(saturate(dot(normal, halfVector)), shininess);
        }
        else
        {
            pSpecularPow = pow(saturate(dot(reflect(-pFactor, normal), toEye)), shininess);
        }
        float32_t3 pSpecular = gPointLight.color.rgb * gPointLight.intensity * pSpecularPow * pAttenuation;

        // --- 3. Spot Light ---
        float32_t3 spotVec = input.worldPosition - gSpotLight.position;
        float32_t sDistance = length(spotVec);
        float32_t3 spotDirP = normalize(spotVec);
        
        float32_t sCosAngle = dot(spotDirP, normalize(gSpotLight.direction));
        float32_t sFalloff = saturate((sCosAngle - gSpotLight.cosAngle) / (gSpotLight.cosFalloffStart - gSpotLight.cosAngle));
        float32_t sAttenuation = pow(saturate(1.0f - (sDistance / gSpotLight.distance)), gSpotLight.decay);
        
        float32_t3 sFactor = -spotDirP;
        float32_t sCos = pow(dot(normal, sFactor) * 0.5f + 0.5f, 2.0f);
        float32_t3 sDiffuse = baseColor * gSpotLight.color.rgb * sCos * gSpotLight.intensity * sAttenuation * sFalloff;
        
        // [修正] 鏡面反射計算の切り替え
        float32_t sSpecularPow;
        if (gMaterial.useBlinnPhong != 0)
        {
            float32_t3 halfVector = normalize(sFactor + toEye);
            sSpecularPow = pow(saturate(dot(normal, halfVector)), shininess);
        }
        else
        {
            sSpecularPow = pow(saturate(dot(reflect(-sFactor, normal), toEye)), shininess);
        }
        float32_t3 sSpecular = gSpotLight.color.rgb * gSpotLight.intensity * sSpecularPow * sAttenuation * sFalloff;

        // --- 4. Environment Mapping ---
        float32_t3 reflectedVector = reflect(normalize(input.worldPosition - gCamera.worldPosition), normal);
        float32_t3 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectedVector).rgb;

        // 全てのライティングを合計
        // インクの上はつやを強くするため、鏡面反射だけ倍率を掛けて足す
        finalColor = dirDiffuse + pDiffuse + sDiffuse + (dirSpecular + pSpecular + sSpecular) * specularScale;
        finalColor += environmentColor * gMaterial.environmentCoefficient;
    }
    else
    {
        finalColor = baseColor;
    }

    // --- 遠くを空に溶け込ませる ---
    // 地面のように遠くで切れる物の切れ目が見えないよう、遠くほど色を「その方向に見える空(スカイボックス)の色」へ近づける
    // スカイボックスはカメラを中心に回転させずに描いているため、カメラからこのピクセルへの向きで同じキューブマップを読めば、真後ろに見える空と同じ色になる
    // 半透明の物も同じ色へ近づければ、空の色の上に空の色を重ねるだけになり、切れ目は見えない
    // カメラからの距離は奥行き(カメラの遠平面で切れる位置)より必ず長いため、遠平面より手前で溶け込みきるようにすれば、カメラの高さによらず遠平面の切れ目も隠せる
    if (gMaterial.distanceFadeEnd > gMaterial.distanceFadeStart)
    {
        float32_t3 fromCamera = input.worldPosition - gCamera.worldPosition;
        float32_t fadeRate = smoothstep(gMaterial.distanceFadeStart, gMaterial.distanceFadeEnd, length(fromCamera));
        float32_t3 skyColor = gEnvironmentTexture.Sample(gSampler, normalize(fromCamera)).rgb;
        finalColor = lerp(finalColor, skyColor, fadeRate);
    }

    // --- 現れる演出 ---
    // まだ見えていない部分は描かずに捨て、透明だった体がだんだん元に戻って見えるようにする
    if (gMaterial.revealStyle != 0)
    {
        // このピクセルが、範囲の下端から上端までのどの高さにあるか(0〜1)
        float32_t heightRate = saturate((input.worldPosition.y - gMaterial.revealBottom) / max(gMaterial.revealHeight, kRevealMinHeight));
        if (gMaterial.revealStyle == kRevealStripes)
        {
            // しま1本の中での位置が進み具合より上なら捨てる(進むほど、見えている部分が太くなる)
            if (frac(heightRate * kRevealStripeCount) >= gMaterial.revealProgress)
            {
                discard;
            }
        }
        else if (gMaterial.revealStyle == kRevealSweep)
        {
            // 境界線は、進み具合 0 で下端より線の太さ分だけ下、1 で上端より線の太さ分だけ上にあり、始めと終わりで線が見えないようにする
            float32_t lineHeight = lerp(-kRevealEdgeWidth, 1.0f + kRevealEdgeWidth, gMaterial.revealProgress);
            if (heightRate > lineHeight)
            {
                discard;
            }
            // 線のすぐ下を光らせる
            float32_t edgeRate = 1.0f - saturate((lineHeight - heightRate) / kRevealEdgeWidth);
            finalColor = lerp(finalColor, kRevealEdgeColor, edgeRate);
        }
    }

    // 最終出力の計算
    output.color.rgb = finalColor;
    output.color.a = gMaterial.color.a * textureColor.a;

    // アルファテスト(しきい値はマテリアルごとに指定し、霧などの半透明は0にして透明部分だけ捨てる)
    if (textureColor.a <= gMaterial.alphaCutoff || output.color.a == 0.0f)
    {
        discard;
    }

    return output;
}