#pragma once

// --- 標準ライブラリ ---
#include <list>
#include <random>
#include <map>
#include <unordered_map>
#include <string>
#include <memory>

// --- DirectX / Math ---
#include <d3d12.h>
#include <wrl.h>
#include "MyMath.h"

// --- エンジン内ヘッダー ---
#include "DXCommon.h"
#include "SrvManager.h"

// 前方宣言
class Camera;

// =============================================================================
// データ構造体
// =============================================================================

struct Particle{
    Vector3 translate;
    float padding1;

    Vector3 scale;
    float lifeTime;

    Vector3 velocity;
    float currentTime;

    Vector4 color;

    Vector2 uvOffset;
    uint32_t particleType;
    float padding2;
};

// GPUで更新する1粒分のデータ(シェーダーの Particle 構造体と並びを合わせる)
struct ParticleForGPU{
    Vector3 translate;
    float lifeTime;

    Vector3 velocity;
    float currentTime;

    // 今の大きさ(寿命に合わせて startScale から endScale へ変わる)
    Vector3 scale;
    // 下向きを負とする重力加速度(m/s^2)
    float gravity;

    Vector3 startScale;
    // 空気抵抗の強さ(1秒あたりに速度が減る割合の指数)
    float drag;

    Vector3 endScale;
    uint32_t particleType;

    // 今の色(寿命に合わせて startColor から endColor へ変わる)
    Vector4 color;
    Vector4 startColor;
    Vector4 endColor;

    Vector2 uvOffset;
    float padding[2];
};

struct PerView{
    Matrix4x4 viewProjection;
    Matrix4x4 billboardMatrix;
};

/// <summary>
/// パーティクルを出すときの設定
/// </summary>
struct ParticleEmitParams{
    // 出す中心の座標
    Vector3 position = {0.0f, 0.0f, 0.0f};
    // 中心からこの半径の球の中にばらけて出す
    float radius = 0.0f;
    // 出す数
    uint32_t count = 1;
    // 寿命(秒)
    float lifeTime = 1.0f;
    // 基準の速度
    Vector3 velocity = {0.0f, 0.0f, 0.0f};
    // 基準の速度に足す、ばらばらの向きの速さの最大値
    float velocitySpread = 0.0f;
    // 出た瞬間と寿命が尽きる瞬間の大きさ(その間はなめらかに変わる)
    Vector3 startScale = {1.0f, 1.0f, 1.0f};
    Vector3 endScale = {1.0f, 1.0f, 1.0f};
    // 出た瞬間と寿命が尽きる瞬間の色(その間はなめらかに変わる。アルファを 0 にすると消えていく)
    Vector4 startColor = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 endColor = {1.0f, 1.0f, 1.0f, 0.0f};
    // 下向きを負とする重力加速度(m/s^2)
    float gravity = 0.0f;
    // 空気抵抗の強さ(大きいほど早く止まる)
    float drag = 0.0f;
};

// 1回分の発生要求(GPUへ渡す。シェーダーの EmitterSphere 構造体と並びを合わせる)
struct EmitterSphere{
    Vector3 translate;
    float radius;

    Vector3 velocity;
    float velocitySpread;

    Vector3 startScale;
    float lifeTime;

    Vector3 endScale;
    float gravity;

    Vector4 startColor;
    Vector4 endColor;

    uint32_t count;
    float drag;
    // 要求ごとに違うばらけ方にするための乱数の種(0〜1)
    float seed;
    float padding;
};

struct PerFrame{
    float time;
    float deltaTime;
};

struct ParticleGroup{
    // リソース情報
    std::string textureFilePath;
    uint32_t textureSrvIndex;
    static const uint32_t kNumMaxInstance = 1024;

    // GPUリソース
    uint32_t instancingSrvIndex;
    uint32_t instancingUavIndex;
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;
    ParticleForGPU* instancingData = nullptr;
    uint32_t numInstance = 0;

    // エミッター関連
    // 1フレームに何回でも出せるよう、発生要求を配列にためて描画のときにまとめてGPUで出す
    static const uint32_t kMaxEmitRequests = 64;
    D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
    Microsoft::WRL::ComPtr<ID3D12Resource> emitRequestResource;
    EmitterSphere* emitRequestData = nullptr;
    // このフレームにためた発生要求の数
    uint32_t emitRequestCount = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource;
    uint32_t freeListIndexUavIndex;
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource;
    uint32_t freeListUavIndex;

    Microsoft::WRL::ComPtr<ID3D12Resource> indexStagingResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> listStagingResource;


    // フラグ
    bool isRing = false;
    bool isCylinder = false;
    bool isShockwave = false;
    bool isSpark = false;
    bool isSmoke = false;
    bool isCharge = false;
    bool isAura = false;
    bool isWarp = false;
};

// =============================================================================
// クラス定義
// =============================================================================

class ParticleManager{
private:
    ParticleManager() = default;
    ~ParticleManager() = default;

public:
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

    static ParticleManager* GetInstance();

    // ライフサイクル
    void Initialize(DXCommon* dxCommon,SrvManager* srvManager);
    void Finalize();
    // このフレームに進める時間を渡す(0 を渡すと止まる。呼ばなかったフレームも止まる)
    void Update(float deltaTime);
    void Draw(Camera* camera);

    // パーティクル操作
    void CreateParticleGroup(const std::string& name,const std::string& textureFilePath,
        bool isRing = false,bool isCylinder = false,bool isShockwave = false,
        bool isSpark = false,bool isSmoke = false,bool isCharge = false,
        bool isAura = false,bool isWarp = false);

    void Emit(const std::string& name,const Transform& emitterTransform,uint32_t count,
        const Vector4& color,const Vector3& velocity,float lifeTime);

    // 設定をまとめて渡して出す(同じフレームに何回呼んでもすべて出る)
    void Emit(const std::string& name,const ParticleEmitParams& params);

    void EmitShockwave(const Vector3& position);
    void EmitSpark(const Vector3& position);
    void EmitSmoke(const Vector3& position);
    void EmitCharge(const Vector3& position);
    void EmitAura(const Vector3& position);
    void EmitWarp();

private:
    // 内部処理・ヘルパー
    void CreateGraphicsPipeline();
    void CreateComputePipeline();
    void CreateModel();
    void CreateRingModel();
    void CreateCylinderModel();
    Particle MakeNewParticle(const Vector3& translate);

    struct VertexData{
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    // メンバ変数
    DXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    std::mt19937 randomEngine_;
    std::unordered_map<std::string,std::unique_ptr<ParticleGroup>> particleGroups_;

    // グラフィックスパイプライン
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

    // コンピュートパイプライン
    Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> emitComputeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> emitComputePipelineState_;

    // モデル用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> ringVertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW ringVertexBufferView_{};
    uint32_t ringVertexCount_ = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> cylinderVertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW cylinderVertexBufferView_{};
    uint32_t cylinderVertexCount_ = 0;

    // 定数バッファ用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
    PerView* perViewData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource_;
    PerFrame* perFrameData_ = nullptr;
    // Update() で受け取り、次の Draw() でGPUの更新に使う時間(使ったら 0 に戻し、Update() を呼ばないフレームは止める)
    float pendingDeltaTime_ = 0.0f;
};