struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// --- ポストプロセス共通定数バッファ (register b1) ---
cbuffer PostProcessConfig : register(b1)
{
    // --- 汎用/フィルタ用 ---
    int32_t gKernelSize; // ボックスフィルタ等のカーネルサイズ
    float gVignetteIntensity; // ビネットの強さ
    float gVignetteScale; // ビネットの範囲
    float gPadding; // 16バイトアライメント用のパディング

    // --- RadialBlur (放射状ブラー) 用 ---
    float2 radialBlurCenter; // ブラーの中心点 (UV座標)
    float radialBlurWidth; // ブラーの拡散幅
    
    // --- Dissolve (ディゾルブ) 用 ---
    float dissolveThreshold; // 消えるしきい値 (0.0〜1.0)
    float dissolveEdgeWidth; // エッジ（境界線）の太さ
    float3 dissolveEdgeColor; // エッジの発光色 (RGB)
    
    // --- Random / Noise 用 ---
    float randomIntensity; // ノイズの強さ
    float randomTime; // 時間（これがないとノイズが止まって見える）

    // --- DamageInk (体力の表現) 用 ---
    float damageAmount; // 体力の減り具合 (0: 満タン 〜 1: 尽きた)
    float damageFlash; // 被弾した瞬間の画面の色づき (0〜1)
};