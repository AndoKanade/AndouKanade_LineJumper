#include "InkPaint.hlsli"

// モデルの三角形を、画面の位置ではなく UV の位置に広げて描く
// こうすると、ピクセルシェーダーが塗りテクスチャの1ピクセルごとに「そのピクセルに貼られる面のワールド座標」を受け取れる

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

// UV(0〜1、左上が原点で下向きが+V)を、描き込み先の座標(-1〜1、上向きが+Y)に直す倍率とずらす量
static const float32_t kUvToClipScale = 2.0f;
static const float32_t kUvToClipOffset = 1.0f;

PaintVertexOutput main(VertexShaderInput input)
{
    PaintVertexOutput output;
    output.position = float32_t4(
        input.texcoord.x * kUvToClipScale - kUvToClipOffset,
        kUvToClipOffset - input.texcoord.y * kUvToClipScale,
        0.0f,
        1.0f);
    output.worldPosition = mul(input.position, gPaintPass.World).xyz;
    output.normal = normalize(mul(input.normal, (float32_t3x3) gPaintPass.WorldInverseTranspose));
    return output;
}
