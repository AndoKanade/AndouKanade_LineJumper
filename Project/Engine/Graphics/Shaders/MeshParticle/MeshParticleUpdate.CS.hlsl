#include "MeshParticle.hlsli"

// 出ている粒を1フレーム分進め、描画に使う行列と色を求める
// 動き方と見た目の変わり方は、GPU へ移す前の InkEffectManager::Update() / UpdatePieceTransform() と同じ計算にしている

ConstantBuffer<FrameConstants> gFrame : register(b0);
RWStructuredBuffer<MeshParticle> gParticles : register(u0);
RWStructuredBuffer<MeshParticleInstance> gInstances : register(u1);
RWStructuredBuffer<CrossingReport> gReports : register(u2);
RWStructuredBuffer<uint32_t> gReportCount : register(u3);

// 拡大率の逆数を求めるときに、0 で割らないための最小の拡大率(寿命の終わりに 0 まで縮むため)
static const float32_t kMinScale = 1e-6f;

// 拡大・回転・移動の行列(MyMath.h の同じ名前の関数と同じ並び。行ベクトルに右から掛ける)
float32_t4x4 MakeScaleMatrix(float32_t3 scale)
{
    return float32_t4x4(
        scale.x, 0.0f, 0.0f, 0.0f,
        0.0f, scale.y, 0.0f, 0.0f,
        0.0f, 0.0f, scale.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

float32_t4x4 MakeTranslateMatrix(float32_t3 translate)
{
    return float32_t4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        translate.x, translate.y, translate.z, 1.0f);
}

float32_t4x4 MakeRotateXMatrix(float32_t radian)
{
    float32_t s = sin(radian);
    float32_t c = cos(radian);
    return float32_t4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, c, s, 0.0f,
        0.0f, -s, c, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

float32_t4x4 MakeRotateYMatrix(float32_t radian)
{
    float32_t s = sin(radian);
    float32_t c = cos(radian);
    return float32_t4x4(
        c, 0.0f, -s, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        s, 0.0f, c, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

float32_t4x4 MakeRotateZMatrix(float32_t radian)
{
    float32_t s = sin(radian);
    float32_t c = cos(radian);
    return float32_t4x4(
        c, s, 0.0f, 0.0f,
        -s, c, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

// 出ていない粒の描画用の値
// 行列の拡大を 0 にして全頂点を原点の1点に集め、面積の無い三角形にして描かれないようにする
MeshParticleInstance MakeHiddenInstance()
{
    MeshParticleInstance instance;
    instance.world = float32_t4x4(
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    instance.worldInverseTranspose = MakeScaleMatrix(float32_t3(1.0f, 1.0f, 1.0f));
    instance.color = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    return instance;
}

// 出ている粒の描画用の行列と色
MeshParticleInstance MakeInstance(MeshParticle particle, KindParams kind)
{
    // 寿命に対する経過の割合(0〜1)
    float32_t lifeRate = particle.lifeTime / particle.maxLifeTime;
    float32_t4 color = particle.color;
    float32_t3 scale = float32_t3(particle.baseScale, particle.baseScale, particle.baseScale);
    float32_t3 rotate = particle.rotate;

    if (kind.expandsAndFades != 0)
    {
        // 閃光のように、一気に広がりながら透明になって消える
        scale *= lerp(1.0f, kind.expandScale, lifeRate);
        color.a = 1.0f - lifeRate;
    }
    else
    {
        // 寿命の後半で小さくなりながら消える
        if (lifeRate > kind.shrinkStartRate)
        {
            float32_t shrink = 1.0f - (lifeRate - kind.shrinkStartRate) / (1.0f - kind.shrinkStartRate);
            scale *= shrink;
        }

        // しぶきのように、飛んでいる向きに伸ばして勢いのある液体に見せる
        // モデルの Z 軸を速度の向きへ回す(Y 軸回転で左右、X 軸回転で上下を合わせる)
        float32_t speed = length(particle.velocity);
        if (kind.stretchesAlongVelocity != 0 && speed > kind.minStretchSpeed)
        {
            float32_t3 direction = particle.velocity / speed;
            float32_t horizontalLength = sqrt(direction.x * direction.x + direction.z * direction.z);
            rotate = float32_t3(atan2(-direction.y, horizontalLength), atan2(direction.x, direction.z), 0.0f);
            float32_t stretch = min(1.0f + speed * kind.stretchPerSpeed, kind.maxStretch);
            scale.z *= stretch;
        }
    }

    // モデルを差し替えたときに大きさを揃える倍率
    scale *= kind.modelScale;

    // ワールド行列(モデルの頂点をずらす → 拡大 → X → Y → Z の回転 → 移動)
    float32_t4x4 rotateMatrix = mul(mul(MakeRotateXMatrix(rotate.x), MakeRotateYMatrix(rotate.y)), MakeRotateZMatrix(rotate.z));
    float32_t4x4 world = mul(mul(mul(MakeTranslateMatrix(kind.modelOffset), MakeScaleMatrix(scale)), rotateMatrix), MakeTranslateMatrix(particle.position));

    // 法線を変換する行列(ワールド行列の逆行列の転置)
    // 移動は法線に関係しないため、拡大と回転の部分だけを使う。拡大 S・回転 R のとき、(S R) の逆行列の転置は S の逆数 × R になる
    float32_t3 inverseScale = 1.0f / max(scale, float32_t3(kMinScale, kMinScale, kMinScale));

    MeshParticleInstance instance;
    instance.world = world;
    instance.worldInverseTranspose = mul(MakeScaleMatrix(inverseScale), rotateMatrix);
    instance.color = color;
    return instance;
}

// 1グループのスレッド数(MeshParticleSystem.cpp の kThreadGroupSize と合わせる)
[numthreads(64, 1, 1)]
void main(uint32_t3 dispatchId : SV_DispatchThreadID)
{
    uint32_t index = dispatchId.x;
    if (index >= gFrame.particleCount)
    {
        return;
    }

    MeshParticle particle = gParticles[index];
    if (particle.isAlive == 0)
    {
        gInstances[index] = MakeHiddenInstance();
        return;
    }

    KindParams kind = gFrame.kinds[particle.kind];
    float32_t deltaTime = gFrame.deltaTime;

    // 寿命が尽きたら空きにする
    particle.lifeTime += deltaTime;
    if (particle.lifeTime >= particle.maxLifeTime)
    {
        particle.isAlive = 0;
        gParticles[index] = particle;
        gInstances[index] = MakeHiddenInstance();
        return;
    }

    // 種類ごとの重力を速度に加えてから位置を進める
    particle.velocity.y -= kind.gravity * deltaTime;
    float32_t3 previousPosition = particle.position;
    particle.position += particle.velocity * deltaTime;
    particle.rotate += particle.angularVelocity * deltaTime;

    // 決まった高さを上から下へ横切ったら CPU へ知らせる(面に着いたか・インクを塗るかは CPU で調べる)
    // 面に着いていれば、次のフレームに CPU から消す要求が来る。それまでの1フレームは面の下にいるため、ほとんど面に隠れて見えない
    if (gFrame.detectsCrossing != 0 && particle.reportsCrossing != 0 &&
        previousPosition.y >= gFrame.crossingHeight && particle.position.y < gFrame.crossingHeight)
    {
        uint32_t reportIndex;
        InterlockedAdd(gReportCount[0], 1, reportIndex);
        if (reportIndex < gFrame.maxReports)
        {
            CrossingReport report;
            report.from = previousPosition;
            report.baseScale = particle.baseScale;
            report.to = particle.position;
            report.slot = index;
            report.generation = particle.generation;
            report.padding = uint32_t3(0, 0, 0);
            gReports[reportIndex] = report;
        }
    }

    gParticles[index] = particle;
    gInstances[index] = MakeInstance(particle, kind);
}
