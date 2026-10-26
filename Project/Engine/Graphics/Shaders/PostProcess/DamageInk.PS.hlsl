#include "PostProcess.hlsli"

// --- リソース (Texture & Sampler) ---
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// --- 定数 ---
// 敵のインクの色(HUD の「GAME OVER」と同じピンク。出力先が sRGB のため、線形の値で書く)
static const float32_t3 kInkColor = float32_t3(1.0f, 0.033f, 0.262f);
// インクの縁の、盛り上がって影になった色(インクより暗いピンク)
static const float32_t3 kInkRimColor = float32_t3(0.305f, 0.003f, 0.08f);
// インクの縁の位置(画面の中心から縁までを 1 とした距離。画面の角は約 1.41)
// 体力が満タンのときは角より外に置いて見せず、尽きたときは画面の中ほどまで迫らせる
static const float32_t kInkStartRadius = 1.45f;
static const float32_t kInkFullRadius = 0.75f;
// インクの縁のぼかしの幅と、縁の影の幅
static const float32_t kInkSoftness = 0.04f;
static const float32_t kInkRimWidth = 0.06f;
// インクの不透明度(奥の景色がかすかに透けて見えるようにする)
static const float32_t kInkOpacity = 0.85f;
// インクの縁を波打たせる2つの波(周りに並ぶ山の数・動く速さ・振れ幅)
// 山の数は整数にして、角度が一周してつながる所で縁が途切れないようにする
static const float32_t kWaveCountA = 7.0f;
static const float32_t kWaveSpeedA = 1.3f;
static const float32_t kWaveAmplitudeA = 0.035f;
static const float32_t kWaveCountB = 13.0f;
static const float32_t kWaveSpeedB = 0.9f;
static const float32_t kWaveAmplitudeB = 0.02f;
// 体力が尽きたときに景色の色をどこまで抜くか(0: そのまま 〜 1: 白黒)
static const float32_t kMaxDesaturation = 0.6f;
// 被弾した瞬間に、画面全体をインクの色へどこまで寄せるか
static const float32_t kFlashTintRate = 0.35f;
// 輝度の計算に使う係数 (Rec.709)
static const float32_t3 kLuminanceWeights = float32_t3(0.2125f, 0.7154f, 0.0721f);

// --- メイン関数 ---
// 体力が減るほど、画面の縁から敵のインクが波打ちながら迫り、景色の色が抜けていく
// 被弾した瞬間は、画面全体を一瞬インクの色に寄せる
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 1. 元の色をサンプリング
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    // 2. 画面の中心からの距離と角度
    // 縦横どちらも中心から縁までを 1 にして、インクが画面の形に沿った楕円で迫るようにする
    float32_t2 centered = (input.texcoord - 0.5f) * 2.0f;
    float32_t distance = length(centered);
    float32_t angle = atan2(centered.y, centered.x);

    // 3. インクの縁の位置を、体力の減り具合で内側へ寄せ、角度と時間で波打たせる
    float32_t wobble = sin(angle * kWaveCountA + randomTime * kWaveSpeedA) * kWaveAmplitudeA
                     + sin(angle * kWaveCountB - randomTime * kWaveSpeedB) * kWaveAmplitudeB;
    float32_t inkRadius = lerp(kInkStartRadius, kInkFullRadius, saturate(damageAmount)) + wobble;

    // 4. 縁より外側をインクで覆い、縁のすぐ外側は暗くして、インクの盛り上がりに見せる
    float32_t inkMask = smoothstep(inkRadius, inkRadius + kInkSoftness, distance);
    float32_t rimMask = 1.0f - smoothstep(inkRadius + kInkSoftness, inkRadius + kInkSoftness + kInkRimWidth, distance);
    float32_t3 inkColor = lerp(kInkColor, kInkRimColor, rimMask);

    // 5. 体力が減るほど景色の色を抜き、被弾した瞬間はインクの色へ寄せる
    float32_t luminance = dot(textureColor.rgb, kLuminanceWeights);
    float32_t3 sceneColor = lerp(textureColor.rgb, (float32_t3) luminance, saturate(damageAmount) * kMaxDesaturation);
    sceneColor = lerp(sceneColor, kInkColor, saturate(damageFlash) * kFlashTintRate);

    // 6. 景色の上にインクを重ねて出力
    output.color.rgb = lerp(sceneColor, inkColor, inkMask * kInkOpacity);
    output.color.a = textureColor.a;

    return output;
}
