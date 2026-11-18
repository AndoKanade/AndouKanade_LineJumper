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

// 1回分の発生要求(ParticleManager.h の EmitterSphere と並びを合わせる)
struct EmitterSphere
{
    float3 translate;
    float radius;
    float3 velocity;
    float velocitySpread;
    float3 startScale;
    float lifeTime;
    float3 endScale;
    float gravity;
    float4 startColor;
    float4 endColor;
    uint count;
    float drag;
    float seed;
    float padding;
};

float32_t rand3dToid(float32_t3 value)
{
    return frac(sin(dot(value, float32_t3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);
}

float32_t3 rand3dTo3d(float32_t3 value)
{
    return float32_t3(
        rand3dToid(value),
        rand3dToid(value + float32_t3(0.123f, 0.456f, 0.789f)),
        rand3dToid(value + float32_t3(0.321f, 0.654f, 0.987f))
    );
}

// ばらけた向きを正規化するときに、0 で割らないための最小の長さ
static const float32_t kMinDirectionLength = 0.0001f;
// 向きが決められなかったときに使う向き(真上)
static const float32_t3 kFallbackDirection = float32_t3(0.0f, 1.0f, 0.0f);

class RandomGenerator
{
    float32_t3 seed;

    float32_t3 Generate3d()
    {
        seed = rand3dTo3d(seed);
        return seed;
    }

    float32_t GenerateId()
    {
        float32_t result = rand3dToid(seed);
        seed.x = result;
        return result;
    }

    // 半径 1 の球の中のばらけた点(ばらけた向きに 0〜1 の長さを掛けたもの)
    float32_t3 GenerateInSphere()
    {
        float32_t3 cube = (Generate3d() - 0.5f) * 2.0f;
        float32_t cubeLength = length(cube);
        float32_t3 direction = (cubeLength > kMinDirectionLength) ? cube / cubeLength : kFallbackDirection;
        return direction * GenerateId();
    }
};

StructuredBuffer<EmitterSphere> gEmitters : register(t0);
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

// 1スレッドで発生要求を1つ出す(Dispatch の X に要求の数を渡す)
[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    EmitterSphere emitter = gEmitters[DTid.x];

    // 要求ごとにCPUで決めた種を使い、同じ場所・同じフレームの要求でも違うばらけ方にする
    RandomGenerator generator;
    generator.seed = float32_t3(emitter.seed, emitter.seed, emitter.seed);

    for (uint32_t countIndex = 0; countIndex < emitter.count; ++countIndex)
    {
        // フリーリストの末尾から空いている粒を1つ取り出す(gFreeListIndex は空きの数)
        int32_t freeCount;
        InterlockedAdd(gFreeListIndex[0], -1, freeCount);

        if (freeCount <= 0)
        {
            // 空きが無いので取り出した分を戻して終える
            InterlockedAdd(gFreeListIndex[0], 1);
            break;
        }

        uint32_t particleIndex = gFreeList[freeCount - 1];

        Particle particle;
        particle.translate = emitter.translate + generator.GenerateInSphere() * emitter.radius;
        particle.lifeTime = emitter.lifeTime;
        particle.velocity = emitter.velocity + generator.GenerateInSphere() * emitter.velocitySpread;
        particle.currentTime = 0.0f;
        particle.scale = emitter.startScale;
        particle.gravity = emitter.gravity;
        particle.startScale = emitter.startScale;
        particle.drag = emitter.drag;
        particle.endScale = emitter.endScale;
        particle.particleType = 0;
        particle.color = emitter.startColor;
        particle.startColor = emitter.startColor;
        particle.endColor = emitter.endColor;
        particle.uvOffset = float2(0.0f, 0.0f);
        particle.padding = float2(0.0f, 0.0f);
        gParticles[particleIndex] = particle;
    }
}
