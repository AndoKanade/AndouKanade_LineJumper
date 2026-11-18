// 1粒分のデータ(ParticleManager.h の ParticleForGPU と並びを合わせる)
struct Particle
{
    float3 translate;
    float lifeTime;
    float3 velocity;
    float currentTime;
    float3 scale;
    float gravity;
    float3 startScale;
    float drag;
    float3 endScale;
    uint particleType;
    float4 color;
    float4 startColor;
    float4 endColor;
    float2 uvOffset;
    float2 padding;
};

struct PerFrame
{
    float time;
    float deltaTime;
};

// リソースバインド
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);

// 定数定義
static const uint32_t kMaxParticles = 1024;

// メイン処理(パーティクルの更新と寿命による回収)
[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;

    // 範囲外スレッドは終了
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    Particle particle = gParticles[particleIndex];

    // 寿命が尽きている(空いている)粒は更新しない
    if (particle.currentTime >= particle.lifeTime)
    {
        return;
    }

    // 時間はゲーム側から渡された分だけ進める(ヒットストップやスローのときは小さく、止めるときは 0)
    float deltaTime = gPerFrame.deltaTime;
    particle.currentTime += deltaTime;

    // 重力で速度を変え、空気抵抗で減速させてから位置を進める
    // 空気抵抗は時間の刻みが変わっても同じ減り方になるよう指数で減らす
    particle.velocity.y += particle.gravity * deltaTime;
    particle.velocity *= exp(-particle.drag * deltaTime);
    particle.translate += particle.velocity * deltaTime;

    // 寿命の進み具合に合わせて、大きさと色を出た瞬間の値から寿命が尽きる瞬間の値へ変える
    float lifeRate = saturate(particle.currentTime / particle.lifeTime);
    particle.scale = lerp(particle.startScale, particle.endScale, lifeRate);
    particle.color = lerp(particle.startColor, particle.endColor, lifeRate);

    // 寿命を迎えたパーティクルの回収処理
    if (particle.currentTime >= particle.lifeTime)
    {
        // スケールを0にして描画対象から除外する
        particle.scale = float3(0.0f, 0.0f, 0.0f);

        // FreeListの末尾にインデックスを戻す(gFreeListIndex は空きの数なので、増やす前の値の位置が末尾になる)
        int32_t freeListIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);

        if (freeListIndex < int32_t(kMaxParticles))
        {
            gFreeList[freeListIndex] = particleIndex;
        }
        else
        {
            // 想定外の状況に対する安全策(増やした分を戻す)
            InterlockedAdd(gFreeListIndex[0], -1);
        }
    }

    gParticles[particleIndex] = particle;
}
