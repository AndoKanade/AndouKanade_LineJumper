// GPU で動かすモデルのパーティクル(MeshParticleSystem)で使う構造体
// どれも MeshParticleSystem.cpp の同じ名前の構造体と並びを合わせる

// 同時に使える種類の数の上限(MeshParticleSystem.h の kMaxKinds と合わせる)
static const uint32_t kMaxKinds = 4;

// 1つ分の状態(コンピュートシェーダーで毎フレーム進める)
struct MeshParticle
{
    float32_t3 position;
    // 出てからの経過時間(秒)
    float32_t lifeTime;
    float32_t3 velocity;
    // 寿命(秒)
    float32_t maxLifeTime;
    // 回転(ラジアン。X → Y → Z の順に回す)
    float32_t3 rotate;
    // 大きさ(全軸共通)
    float32_t baseScale;
    // 1秒あたりの回転量(ラジアン)
    float32_t3 angularVelocity;
    // 種類の番号(FrameConstants の kinds の添え字)
    uint32_t kind;
    float32_t4 color;
    // 出ているか(0 なら空き)
    uint32_t isAlive;
    // 決まった高さを上から下へ横切ったら CPU へ知らせるか(0 なら知らせない)
    uint32_t reportsCrossing;
    // この入れ物を使い回した回数(知らせた後に別の粒へ入れ替わっていないかを CPU で確かめる)
    uint32_t generation;
    float32_t padding;
};

// CPU から出す1つ分の要求(入れ物の番号と、入れる状態)
struct SpawnRequest
{
    MeshParticle particle;
    uint32_t slot;
    uint32_t3 padding;
};

// 描画に使う1つ分の行列と色(コンピュートシェーダーで求め、頂点シェーダーで読む)
struct MeshParticleInstance
{
    float32_t4x4 world;
    float32_t4x4 worldInverseTranspose;
    float32_t4 color;
};

// 決まった高さを上から下へ横切った粒の知らせ(CPU で読み、面に着いたかを調べる)
struct CrossingReport
{
    // このフレームの移動前・移動後の座標
    float32_t3 from;
    float32_t baseScale;
    float32_t3 to;
    uint32_t slot;
    uint32_t generation;
    uint32_t3 padding;
};

// 種類ごとの動き方と見た目の設定
struct KindParams
{
    // 下向きの重力加速度
    float32_t gravity;
    // 飛ぶ向きに伸ばす割合(速さ1あたり)・伸ばす上限・伸ばし始める速さ
    float32_t stretchPerSpeed;
    float32_t maxStretch;
    float32_t minStretchSpeed;
    // 寿命のこの割合を過ぎたら小さくなりながら消える
    float32_t shrinkStartRate;
    // 広がりながら消えるときの、寿命が尽きる瞬間の大きさ(出たときの何倍か)
    float32_t expandScale;
    // 広がりながら透明になって消えるか(0 以外なら消える)
    uint32_t expandsAndFades;
    // 飛ぶ向きにモデルの +Z を向けて伸ばすか(0 以外なら伸ばす)
    uint32_t stretchesAlongVelocity;
    // モデルの頂点に足す量(モデルの座標系)と、モデルの大きさを揃える倍率
    float32_t3 modelOffset;
    float32_t modelScale;
};

// 1フレーム分の設定(動かすコンピュートシェーダーで使う)
struct FrameConstants
{
    // このフレームに進める時間(秒)
    float32_t deltaTime;
    // 横切ったかを調べる高さ
    float32_t crossingHeight;
    // 横切ったかを調べるか(0 なら調べない)
    uint32_t detectsCrossing;
    // 入れ物の総数
    uint32_t particleCount;
    // 1フレームに知らせられる数の上限
    uint32_t maxReports;
    uint32_t3 padding;
    KindParams kinds[kMaxKinds];
};

// 出す要求の数(出すコンピュートシェーダーにルート定数で渡す)
struct SpawnConstants
{
    uint32_t requestCount;
};

// 描画1回分の設定(ルート定数で渡す)
struct DrawConstants
{
    // この種類の入れ物の先頭の番号(SV_InstanceID に足して、行列の配列の添え字にする)
    uint32_t instanceOffset;
    // 光の当たり方を計算するか(0 なら色をそのまま出す)
    uint32_t enableLighting;
};

// カメラと描き方の設定(頂点・ピクセルシェーダーで使う)
struct ViewConstants
{
    float32_t4x4 viewProjection;
    // カメラからの距離がこの間で、色を背景の空の色へ少しずつ近づける(Obj3D と同じ。End <= Start のときは近づけない)
    float32_t distanceFadeStart;
    float32_t distanceFadeEnd;
    // テクスチャのアルファがこの値以下のピクセルは描かない(Obj3D の既定値と同じ)
    float32_t alphaCutoff;
    // 鏡面反射の鋭さ(Obj3D の既定値と同じ)
    float32_t shininess;
};

// 頂点シェーダーからピクセルシェーダーへ渡す値
struct MeshParticleVertexOutput
{
    float32_t4 position : SV_POSITION;
    float32_t3 worldPosition : POSITION0;
    float32_t3 normal : NORMAL0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
};
