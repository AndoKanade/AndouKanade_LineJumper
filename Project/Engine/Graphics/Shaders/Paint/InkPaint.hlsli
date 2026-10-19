// インクの塗り(UV 空間でのペイント)の共通定義
// C++ 側の PaintCanvas::InkSplat / PaintCanvas::PaintPassData と同じ並びにすること

// 1回の描き込みで塗れるインクの最大数(PaintCanvas::kMaxSplatsPerFrame と合わせる)
static const uint32_t kMaxSplatsPerFrame = 32;

// インク1滴分
struct InkSplat
{
    float32_t3 position; // 中心(ワールド座標)
    float32_t radius;    // 半径
    float32_t4 color;    // インクの色
    float32_t3 normal;   // 塗る面の向き
    float32_t seed;      // 輪郭の形と周りのしぶきの並びを変える乱数の種
};

// 1回の描き込みで使うデータ
struct PaintPass
{
    float32_t4x4 World;
    float32_t4x4 WorldInverseTranspose;
    uint32_t splatCount;
    float32_t3 padding;
    InkSplat splats[kMaxSplatsPerFrame];
};

ConstantBuffer<PaintPass> gPaintPass : register(b0);

struct PaintVertexOutput
{
    float32_t4 position : SV_POSITION;
    float32_t3 worldPosition : POSITION0;
    float32_t3 normal : NORMAL0;
};
