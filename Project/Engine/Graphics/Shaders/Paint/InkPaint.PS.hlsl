#include "InkPaint.hlsli"

// 塗りテクスチャの1ピクセルごとに、そこに貼られる面のワールド座標がインクの範囲に入っているかを調べて塗る
// α には、インクの輪郭でちょうど 0.5、内側ほど 1、外側ほど 0 になるなめらかな値を書く(描画側で 0.5 でしきい値処理する)

// --- インクの形 ---
// 輪郭のなめらかさ(半径に対する割合)。輪郭から内側・外側へこの距離だけ進むと α が 1・0 になる
static const float32_t kSoftRate = 0.25f;
// 輪郭のでこぼこの細かさ(半径1あたりの波の数)と大きさ(半径に対する割合)
static const float32_t kEdgeNoiseFrequency = 2.5f;
static const float32_t kEdgeNoiseAmplitude = 0.22f;
// 2段目のでこぼこ(細かい波)の細かさと大きさ(1段目に対する倍率)
static const float32_t kEdgeDetailFrequencyRate = 2.7f;
static const float32_t kEdgeDetailAmplitudeRate = 0.45f;
// 乱数の種1あたりにノイズの座標をずらす量(1滴ごとに輪郭の形を変える)
static const float32_t kSeedNoiseOffset = 13.7f;

// --- 周りに飛ぶ小さなしぶき ---
static const uint32_t kSatelliteCount = 6;
// しぶきを出すかどうかの割合(乱数がこれより小さいしぶきは出さず、並びを不ぞろいにする)
static const float32_t kSatelliteSkipRate = 0.3f;
// 中心からの距離と半径(インクの半径に対する割合)
static const float32_t kSatelliteDistanceMin = 1.15f;
static const float32_t kSatelliteDistanceMax = 1.7f;
static const float32_t kSatelliteRadiusMin = 0.1f;
static const float32_t kSatelliteRadiusMax = 0.22f;
// しぶきの向きを等間隔から前後にずらす最大量(1つ分の間隔に対する割合)
static const float32_t kSatelliteAngleJitter = 0.3f;

// インクが届く最も遠い距離(半径に対する割合)。これより遠いピクセルは計算を省く
static const float32_t kSplatReachRate = kSatelliteDistanceMax + kSatelliteRadiusMax * (1.0f + kSoftRate) + kSoftRate;
// 塗る面の向きと、ピクセルの面の向きの内積がこれより小さい面は塗らない(薄い板の裏側まで塗れないようにする)
static const float32_t kMinFacing = -0.3f;

static const float32_t kTwoPi = 6.28318530718f;
// α の 0.5(輪郭の位置)
static const float32_t kEdgeValue = 0.5f;
// 面の向きが真上・真下に近いとき、しぶきを並べる平面の向きの基準を Y 軸から X 軸に変えるしきい値
static const float32_t kUpAxisThreshold = 0.99f;

// --- 乱数(座標から 0〜1 の値を作るハッシュ。Dave Hoskins の hash13) ---
static const float32_t kHashScale = 0.1031f;
static const float32_t kHashOffset = 31.32f;

float32_t Hash13(float32_t3 p)
{
    p = frac(p * kHashScale);
    p += dot(p, p.zyx + kHashOffset);
    return frac((p.x + p.y) * p.z);
}

// 0〜1 の値を -1〜1 に直す
float32_t ToSigned(float32_t value)
{
    return value + value - 1.0f;
}

// 格子点の乱数をなめらかに補間した 0〜1 のノイズ(値ノイズ)
float32_t ValueNoise(float32_t3 p)
{
    float32_t3 cell = floor(p);
    float32_t3 t = smoothstep(0.0f, 1.0f, frac(p));

    float32_t n000 = Hash13(cell);
    float32_t n100 = Hash13(cell + float32_t3(1.0f, 0.0f, 0.0f));
    float32_t n010 = Hash13(cell + float32_t3(0.0f, 1.0f, 0.0f));
    float32_t n110 = Hash13(cell + float32_t3(1.0f, 1.0f, 0.0f));
    float32_t n001 = Hash13(cell + float32_t3(0.0f, 0.0f, 1.0f));
    float32_t n101 = Hash13(cell + float32_t3(1.0f, 0.0f, 1.0f));
    float32_t n011 = Hash13(cell + float32_t3(0.0f, 1.0f, 1.0f));
    float32_t n111 = Hash13(cell + float32_t3(1.0f, 1.0f, 1.0f));

    float32_t nx00 = lerp(n000, n100, t.x);
    float32_t nx10 = lerp(n010, n110, t.x);
    float32_t nx01 = lerp(n001, n101, t.x);
    float32_t nx11 = lerp(n011, n111, t.x);
    return lerp(lerp(nx00, nx10, t.y), lerp(nx01, nx11, t.y), t.z);
}

// 中心からの距離と輪郭までの半径から、輪郭で 0.5 になるなめらかな値を求める
float32_t EdgeField(float32_t distance, float32_t edgeRadius, float32_t softWidth)
{
    return saturate(kEdgeValue + (edgeRadius - distance) / (softWidth + softWidth));
}

// インク1滴が、指定したワールド座標をどれだけ覆っているか(輪郭で 0.5)
float32_t SplatField(float32_t3 worldPosition, InkSplat splat)
{
    const float32_t softWidth = splat.radius * kSoftRate;

    // 本体: 球の輪郭を、ワールド座標で決まる2段のノイズでゆがませて、液体が飛び散ったような形にする
    float32_t3 local = (worldPosition - splat.position) / splat.radius;
    float32_t3 noisePosition = local * kEdgeNoiseFrequency + splat.seed * kSeedNoiseOffset;
    float32_t noise = ValueNoise(noisePosition) + ValueNoise(noisePosition * kEdgeDetailFrequencyRate) * kEdgeDetailAmplitudeRate;
    // 2段のノイズの和(0〜1+倍率)を 0〜1 に収めてから -1〜1 に直し、輪郭の半径を増減させる
    float32_t noiseRange = 1.0f + kEdgeDetailAmplitudeRate;
    float32_t edgeRadius = splat.radius * (1.0f + kEdgeNoiseAmplitude * ToSigned(noise / noiseRange));
    float32_t field = EdgeField(length(worldPosition - splat.position), edgeRadius, softWidth);

    // 周りのしぶき: 塗る面に沿った平面の上に、本体を囲むように小さな球を不ぞろいに並べる
    float32_t3 reference = abs(splat.normal.y) < kUpAxisThreshold ? float32_t3(0.0f, 1.0f, 0.0f) : float32_t3(1.0f, 0.0f, 0.0f);
    float32_t3 tangent = normalize(cross(splat.normal, reference));
    float32_t3 bitangent = cross(splat.normal, tangent);
    for (uint32_t i = 0; i < kSatelliteCount; ++i)
    {
        float32_t3 hashBase = float32_t3(splat.seed, (float32_t) i, 0.0f);
        if (Hash13(hashBase) < kSatelliteSkipRate)
        {
            continue;
        }
        float32_t3 axisY = float32_t3(0.0f, 1.0f, 0.0f);
        float32_t3 axisZ = float32_t3(0.0f, 0.0f, 1.0f);
        float32_t angle = ((float32_t) i + ToSigned(Hash13(hashBase + axisY)) * kSatelliteAngleJitter) * (kTwoPi / (float32_t) kSatelliteCount);
        float32_t distanceRate = lerp(kSatelliteDistanceMin, kSatelliteDistanceMax, Hash13(hashBase + axisZ));
        float32_t radiusRate = lerp(kSatelliteRadiusMin, kSatelliteRadiusMax, Hash13(hashBase + axisY + axisZ));

        float32_t3 center = splat.position + (tangent * cos(angle) + bitangent * sin(angle)) * (splat.radius * distanceRate);
        float32_t satelliteRadius = splat.radius * radiusRate;
        field = max(field, EdgeField(length(worldPosition - center), satelliteRadius, satelliteRadius * kSoftRate));
    }
    return field;
}

float32_t4 main(PaintVertexOutput input) : SV_TARGET0
{
    float32_t3 normal = normalize(input.normal);
    float32_t field = 0.0f;
    float32_t3 color = float32_t3(0.0f, 0.0f, 0.0f);

    for (uint32_t i = 0; i < gPaintPass.splatCount; ++i)
    {
        InkSplat splat = gPaintPass.splats[i];

        // 届かない距離のピクセル・裏側を向いた面は、ノイズの計算をせずに飛ばす
        float32_t3 diff = input.worldPosition - splat.position;
        float32_t reach = splat.radius * kSplatReachRate;
        if (dot(diff, diff) > reach * reach || dot(normal, splat.normal) < kMinFacing)
        {
            continue;
        }

        float32_t splatField = SplatField(input.worldPosition, splat);
        if (splatField <= 0.0f)
        {
            continue;
        }
        // 後から塗ったインクほど上に重なるよう、色は後のインクで置き換える
        color = splat.color.rgb;
        field = max(field, splatField);
    }

    // どのインクも届かないピクセルは、前に塗った色を残すため書き込まない
    if (field <= 0.0f)
    {
        discard;
    }
    return float32_t4(color, field);
}
