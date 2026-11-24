#include "MeshParticle.hlsli"

// CPU からの要求どおりに、指定の入れ物へ出したときの状態を書き込む
// 1つの入れ物への要求は CPU 側で1フレームに1つにまとめてあるため、スレッド同士で同じ入れ物に書き込むことはない

ConstantBuffer<SpawnConstants> gSpawn : register(b1);
StructuredBuffer<SpawnRequest> gRequests : register(t0);
RWStructuredBuffer<MeshParticle> gParticles : register(u0);

// 1グループのスレッド数(MeshParticleSystem.cpp の kThreadGroupSize と合わせる)
[numthreads(64, 1, 1)]
void main(uint32_t3 dispatchId : SV_DispatchThreadID)
{
    uint32_t requestIndex = dispatchId.x;
    if (requestIndex >= gSpawn.requestCount)
    {
        return;
    }

    SpawnRequest request = gRequests[requestIndex];
    gParticles[request.slot] = request.particle;
}
