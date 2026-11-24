#include "ParticleManager.h"
#include "TextureManager.h"
#include "Camera.h"
#include "Logger.h"
#include <cassert>
#include <random>
#include <numbers>

using namespace Microsoft::WRL;

namespace{
    // 色と大きさだけを指定する Emit() で使う、発生範囲の半径と速さのばらつき
    constexpr float kSimpleEmitRadius = 1.0f;
    constexpr float kSimpleEmitVelocitySpread = 1.0f;
    // 寿命が尽きる瞬間の色のアルファ(だんだん透明にして消す)
    constexpr float kFadeOutAlpha = 0.0f;

    // 発生要求ごとの乱数の種の範囲
    constexpr float kSeedMin = 0.0f;
    constexpr float kSeedMax = 1.0f;

    // 更新のコンピュートシェーダーの1グループのスレッド数(UpdateParticle.CS.hlsl の numthreads と合わせる)
    constexpr uint32_t kUpdateThreadGroupSize = 256;

    // 被弾・撃破の火花(四方へ速く飛び散り、重力で落ちながら空気抵抗で減速し、小さくなって消える)
    constexpr uint32_t kSparkCount = 20;
    constexpr float kSparkLifeTime = 0.45f;
    constexpr float kSparkRadius = 0.1f;
    constexpr float kSparkSpeed = 8.0f;
    constexpr float kSparkGravity = -15.0f;
    constexpr float kSparkDrag = 3.0f;
    constexpr Vector3 kSparkStartScale = {0.25f, 0.25f, 0.25f};
    constexpr Vector3 kSparkEndScale = {0.0f, 0.0f, 0.0f};
    // 出た瞬間は明るい黄色、消える前は赤みがかった橙にする
    constexpr Vector4 kSparkStartColor = {1.0f, 0.8f, 0.3f, 1.0f};
    constexpr Vector4 kSparkEndColor = {1.0f, 0.3f, 0.0f, 0.0f};
}

ParticleManager* ParticleManager::GetInstance(){
    static ParticleManager instance;
    return &instance;
}

void ParticleManager::Initialize(DXCommon* dxCommon,SrvManager* srvManager){
    assert(dxCommon);
    assert(srvManager);

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    // 乱数生成器の初期化
    std::random_device seedGenerator;
    randomEngine_.seed(seedGenerator());

    // PerView用定数バッファの生成
    size_t sizeInBytes = (sizeof(PerView) + 0xff) & ~0xff;
    perViewResource_ = dxCommon_->CreateBufferResource(sizeInBytes);
    perViewResource_->Map(0,nullptr,reinterpret_cast<void**>(&perViewData_));
    *perViewData_ = {MakeIdentity4x4(), MakeIdentity4x4()};

    // PerFrame用定数バッファの生成
    size_t sizePerFrame = (sizeof(PerFrame) + 0xff) & ~0xff;
    perFrameResource_ = dxCommon_->CreateBufferResource(sizePerFrame);
    perFrameResource_->Map(0,nullptr,reinterpret_cast<void**>(&perFrameData_));
    perFrameData_->time = 0.0f;
    perFrameData_->deltaTime = 1.0f / 60.0f;

    // パイプラインとモデルの生成
    CreateGraphicsPipeline();
    CreateModel();
    CreateRingModel();
    CreateCylinderModel();
    CreateComputePipeline();
}

void ParticleManager::Finalize(){
    // グループごとのリソース解放
    for(auto& [name,group] : particleGroups_){
        group->instancingResource.Reset();
        group->freeListIndexResource.Reset();
        group->freeListResource.Reset();
        group->indexStagingResource.Reset();
        group->listStagingResource.Reset();
        group->emitRequestResource.Reset();
    }
    particleGroups_.clear();

    // 共有リソースの解放
    rootSignature_.Reset();
    graphicsPipelineState_.Reset();
    computeRootSignature_.Reset();
    computePipelineState_.Reset();
    emitComputeRootSignature_.Reset();
    emitComputePipelineState_.Reset();

    vertexBuffer_.Reset();
    ringVertexBuffer_.Reset();
    cylinderVertexBuffer_.Reset();
    vertexResource_.Reset();

    perViewResource_.Reset();
    perFrameResource_.Reset();
}

void ParticleManager::CreateParticleGroup(const std::string& name,const std::string& textureFilePath,bool isRing,bool isCylinder,bool isShockwave,bool isSpark,bool isSmoke,bool isCharge,bool isAura,bool isWarp){
    if(particleGroups_.contains(name)){
        return;
    }

    std::unique_ptr<ParticleGroup> group = std::make_unique<ParticleGroup>();

    // フラグの設定
    group->isRing = isRing;
    group->isCylinder = isCylinder;
    group->isShockwave = isShockwave;
    group->isSpark = isSpark;
    group->isSmoke = isSmoke;
    group->isCharge = isCharge;
    group->isAura = isAura;
    group->isWarp = isWarp;

    // テクスチャのロードとSRVインデックス取得
    group->textureFilePath = textureFilePath;
    TextureManager::GetInstance()->LoadTexture(textureFilePath);
    group->textureSrvIndex = TextureManager::GetInstance()->GetSrvIndex(textureFilePath);

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    // インスタンシング用リソースの生成
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeof(ParticleForGPU) * group->kNumMaxInstance;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&group->instancingResource)
    );
    assert(SUCCEEDED(hr));

    // インスタンシング用SRVの作成
    group->instancingSrvIndex = srvManager_->Allocate();
    D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
    instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    instancingSrvDesc.Buffer.FirstElement = 0;
    instancingSrvDesc.Buffer.NumElements = group->kNumMaxInstance;
    instancingSrvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);
    instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    srvManager_->CreateSRVforStructuredBuffer(
        group->instancingSrvIndex,
        group->instancingResource.Get(),
        instancingSrvDesc.Buffer.NumElements,
        instancingSrvDesc.Buffer.StructureByteStride
    );

    // 各種UAVインデックスの確保
    group->instancingUavIndex = srvManager_->Allocate();
    group->freeListIndexUavIndex = srvManager_->Allocate();
    group->freeListUavIndex = srvManager_->Allocate();

    // インスタンシング用UAVの作成
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = group->kNumMaxInstance;
    uavDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);

    dxCommon_->GetDevice()->CreateUnorderedAccessView(
        group->instancingResource.Get(),
        nullptr,
        &uavDesc,
        srvManager_->GetCPUDescriptorHandle(group->instancingUavIndex)
    );

    // FreeListIndex用リソースの生成
    D3D12_RESOURCE_DESC indexResDesc{};
    indexResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    indexResDesc.Width = sizeof(int32_t);
    indexResDesc.Height = 1;
    indexResDesc.DepthOrArraySize = 1;
    indexResDesc.MipLevels = 1;
    indexResDesc.SampleDesc.Count = 1;
    indexResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    indexResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &indexResDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&group->freeListIndexResource)
    );
    assert(SUCCEEDED(hr));

    // FreeListIndex用UAVの作成
    D3D12_UNORDERED_ACCESS_VIEW_DESC indexUavDesc{};
    indexUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    indexUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    indexUavDesc.Buffer.NumElements = 1;
    indexUavDesc.Buffer.StructureByteStride = sizeof(int32_t);

    dxCommon_->GetDevice()->CreateUnorderedAccessView(
        group->freeListIndexResource.Get(),
        nullptr,
        &indexUavDesc,
        srvManager_->GetCPUDescriptorHandle(group->freeListIndexUavIndex)
    );

    // FreeList用リソースの生成
    D3D12_RESOURCE_DESC listResDesc = indexResDesc;
    listResDesc.Width = sizeof(uint32_t) * group->kNumMaxInstance;

    hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &listResDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&group->freeListResource)
    );
    assert(SUCCEEDED(hr));

    // FreeList用UAVの作成
    D3D12_UNORDERED_ACCESS_VIEW_DESC listUavDesc{};
    listUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    listUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    listUavDesc.Buffer.NumElements = group->kNumMaxInstance;
    listUavDesc.Buffer.StructureByteStride = sizeof(uint32_t);

    dxCommon_->GetDevice()->CreateUnorderedAccessView(
        group->freeListResource.Get(),
        nullptr,
        &listUavDesc,
        srvManager_->GetCPUDescriptorHandle(group->freeListUavIndex)
    );

    // FreeListIndex初期化用ステージングリソースの作成と書き込み
    group->indexStagingResource = dxCommon_->CreateBufferResource(sizeof(int32_t));
    int32_t* indexMap = nullptr;
    group->indexStagingResource->Map(0,nullptr,reinterpret_cast<void**>(&indexMap));
    *indexMap = group->kNumMaxInstance;
    group->indexStagingResource->Unmap(0,nullptr);

    // FreeList初期化用ステージングリソースの作成と書き込み
    group->listStagingResource = dxCommon_->CreateBufferResource(sizeof(uint32_t) * group->kNumMaxInstance);
    uint32_t* listMap = nullptr;
    group->listStagingResource->Map(0,nullptr,reinterpret_cast<void**>(&listMap));
    for(uint32_t i = 0; i < group->kNumMaxInstance; ++i){
        listMap[i] = i;
    }
    group->listStagingResource->Unmap(0,nullptr);

    // 初期データをGPUへコピー
    auto commandList = dxCommon_->GetCommandList();
    commandList->CopyBufferRegion(group->freeListIndexResource.Get(),0,group->indexStagingResource.Get(),0,sizeof(int32_t));
    commandList->CopyBufferRegion(group->freeListResource.Get(),0,group->listStagingResource.Get(),0,sizeof(uint32_t) * group->kNumMaxInstance);

    // コピー先リソースをUAVとして使用するための状態遷移バリア
    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = group->freeListIndexResource.Get();
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = group->freeListResource.Get();
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(2,barriers);

    // 発生要求をためる配列の生成(CPUから書き込み、GPUからは StructuredBuffer として読む)
    // GPUの処理はフレームごとに待ってから次のフレームを書くため、1つのバッファを毎フレーム書き直して使える
    group->emitRequestResource = dxCommon_->CreateBufferResource(sizeof(EmitterSphere) * ParticleGroup::kMaxEmitRequests);
    group->emitRequestResource->Map(0,nullptr,reinterpret_cast<void**>(&group->emitRequestData));
    group->emitRequestCount = 0;

    group->currentState = D3D12_RESOURCE_STATE_COMMON;

    particleGroups_.insert(std::make_pair(name,std::move(group)));
}

void ParticleManager::Update(float deltaTime){
    // 全パーティクルグループの描画数を設定する
    for(auto& [name,group] : particleGroups_){
        group->numInstance = group->kNumMaxInstance;
    }

    // 進める時間は、GPUの更新を記録する Draw() で定数バッファへ書き込む
    // カメラ行列(perViewData_)もカメラ更新後の値を使うため、Draw()で書き込む
    pendingDeltaTime_ = deltaTime;
}

void ParticleManager::Draw(Camera* camera){
    assert(dxCommon_);
    assert(camera);
    auto commandList = dxCommon_->GetCommandList();

    // カメラ行列の計算
    // Update()の時点ではカメラ行列が前フレームのままのため、カメラ更新後の描画時に求めて1フレームの遅れを防ぐ
    Matrix4x4 billboardMatrix = camera->GetWorldMatrix();
    billboardMatrix.m[3][0] = 0.0f;
    billboardMatrix.m[3][1] = 0.0f;
    billboardMatrix.m[3][2] = 0.0f;

    if(perViewData_){
        perViewData_->viewProjection = camera->GetViewProjectionMatrix();
        perViewData_->billboardMatrix = billboardMatrix;
    }

    // このフレームに進める時間を書き込み、使い終えたら 0 に戻す
    // (ヘルプを開いている間など Update() を呼ばないフレームは、パーティクルを止めたままにする)
    if(perFrameData_){
        perFrameData_->deltaTime = pendingDeltaTime_;
        perFrameData_->time += pendingDeltaTime_;
    }
    pendingDeltaTime_ = 0.0f;

    for(auto& [name,group] : particleGroups_){
        if(group->numInstance == 0) continue;

        // SRVからUAVへの状態遷移バリア
        D3D12_RESOURCE_BARRIER barrierToUAV{};
        barrierToUAV.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrierToUAV.Transition.pResource = group->instancingResource.Get();
        barrierToUAV.Transition.StateBefore = group->currentState;
        barrierToUAV.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrierToUAV.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1,&barrierToUAV);
        group->currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

        // エミット用コンピュートシェーダーの実行
        // このフレームにためた発生要求を1要求1スレッドでまとめて出す
        if(group->emitRequestCount > 0){
            commandList->SetComputeRootSignature(emitComputeRootSignature_.Get());
            commandList->SetPipelineState(emitComputePipelineState_.Get());

            commandList->SetComputeRootDescriptorTable(0,srvManager_->GetGPUDescriptorHandle(group->instancingUavIndex));
            commandList->SetComputeRootDescriptorTable(1,srvManager_->GetGPUDescriptorHandle(group->freeListIndexUavIndex));
            commandList->SetComputeRootDescriptorTable(2,srvManager_->GetGPUDescriptorHandle(group->freeListUavIndex));
            commandList->SetComputeRootShaderResourceView(3,group->emitRequestResource->GetGPUVirtualAddress());

            // EmitParticle.CS.hlsl は numthreads(1,1,1) のため、要求の数だけグループを並べる
            commandList->Dispatch(group->emitRequestCount,1,1);

            // 出した粒とフリーリストの書き込みを、更新のコンピュートシェーダーより先に終わらせる
            D3D12_RESOURCE_BARRIER barrierUAV{};
            barrierUAV.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrierUAV.UAV.pResource = nullptr;
            commandList->ResourceBarrier(1,&barrierUAV);

            // 記録し終えたので、次のフレームの発生要求を最初からためる
            group->emitRequestCount = 0;
        }

        // 更新用コンピュートシェーダーの実行
        commandList->SetComputeRootSignature(computeRootSignature_.Get());
        commandList->SetPipelineState(computePipelineState_.Get());

        commandList->SetComputeRootDescriptorTable(0,srvManager_->GetGPUDescriptorHandle(group->instancingUavIndex));
        commandList->SetComputeRootDescriptorTable(1,srvManager_->GetGPUDescriptorHandle(group->freeListIndexUavIndex));
        commandList->SetComputeRootDescriptorTable(2,srvManager_->GetGPUDescriptorHandle(group->freeListUavIndex));
        commandList->SetComputeRootConstantBufferView(3,perFrameResource_->GetGPUVirtualAddress());

        uint32_t groupCount = (group->kNumMaxInstance + kUpdateThreadGroupSize - 1) / kUpdateThreadGroupSize;
        commandList->Dispatch(groupCount,1,1);

        // UAVからSRVへの状態遷移バリア
        D3D12_RESOURCE_BARRIER barrierToSRV{};
        barrierToSRV.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrierToSRV.Transition.pResource = group->instancingResource.Get();
        barrierToSRV.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrierToSRV.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        barrierToSRV.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1,&barrierToSRV);
        group->currentState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

        // FreeListIndex用リソースのUAVバリア
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = group->freeListIndexResource.Get();
        commandList->ResourceBarrier(1,&barrier);
    }

    // グラフィックス描画処理
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(graphicsPipelineState_.Get());
    commandList->SetGraphicsRootConstantBufferView(2,perViewResource_->GetGPUVirtualAddress());

    for(auto& [name,group] : particleGroups_){
        if(group->numInstance == 0) continue;

        uint32_t vertexCount = 4;
        if(group->isCylinder){
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            commandList->IASetVertexBuffers(0,1,&cylinderVertexBufferView_);
            vertexCount = cylinderVertexCount_;
        } else if(group->isRing){
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            commandList->IASetVertexBuffers(0,1,&ringVertexBufferView_);
            vertexCount = ringVertexCount_;
        } else{
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            commandList->IASetVertexBuffers(0,1,&vertexBufferView_);
            vertexCount = 4;
        }

        commandList->SetGraphicsRootDescriptorTable(0,srvManager_->GetGPUDescriptorHandle(group->textureSrvIndex));
        commandList->SetGraphicsRootDescriptorTable(1,srvManager_->GetGPUDescriptorHandle(group->instancingSrvIndex));

        commandList->DrawInstanced(vertexCount,group->numInstance,0,0);
    }
}

Particle ParticleManager::MakeNewParticle(const Vector3& translate){
    Particle particle;
    std::uniform_real_distribution<float> distributionVel(-1.0f,1.0f);
    std::uniform_real_distribution<float> distColor(0.0f,1.0f);
    std::uniform_real_distribution<float> distTime(1.0f,3.0f);

    particle.translate = translate;
    particle.velocity = {distributionVel(randomEngine_), distributionVel(randomEngine_), distributionVel(randomEngine_)};
    particle.color = {distColor(randomEngine_), distColor(randomEngine_), distColor(randomEngine_), 1.0f};
    particle.lifeTime = distTime(randomEngine_);
    particle.currentTime = 0.0f;
    particle.scale = {1.0f, 1.0f, 1.0f};

    return particle;
}

void ParticleManager::Emit(const std::string& name,const Transform& emitterTransform,uint32_t count,const Vector4& color,const Vector3& velocity,float lifeTime){
    // 大きさは変えず、色はそのままでだんだん透明にして消す
    ParticleEmitParams params;
    params.position = emitterTransform.translate;
    params.radius = kSimpleEmitRadius;
    params.count = count;
    params.lifeTime = lifeTime;
    params.velocity = velocity;
    params.velocitySpread = kSimpleEmitVelocitySpread;
    params.startScale = emitterTransform.scale;
    params.endScale = emitterTransform.scale;
    params.startColor = color;
    params.endColor = {color.x, color.y, color.z, kFadeOutAlpha};
    Emit(name,params);
}

void ParticleManager::Emit(const std::string& name,const ParticleEmitParams& params){
    auto it = particleGroups_.find(name);
    if(it == particleGroups_.end()) return;
    ParticleGroup* group = it->second.get();

    // 1粒も出ない要求と、寿命が無く出た瞬間から更新されない(空きに戻らない)粒は出さない
    if(params.count == 0 || params.lifeTime <= 0.0f) return;
    // 1フレームにためられる数を超えた分は出さない
    if(group->emitRequestCount >= ParticleGroup::kMaxEmitRequests) return;

    std::uniform_real_distribution<float> distSeed(kSeedMin,kSeedMax);

    EmitterSphere& request = group->emitRequestData[group->emitRequestCount];
    request.translate = params.position;
    request.radius = params.radius;
    request.velocity = params.velocity;
    request.velocitySpread = params.velocitySpread;
    request.startScale = params.startScale;
    request.lifeTime = params.lifeTime;
    request.endScale = params.endScale;
    request.gravity = params.gravity;
    request.startColor = params.startColor;
    request.endColor = params.endColor;
    request.count = params.count;
    request.drag = params.drag;
    request.seed = distSeed(randomEngine_);
    request.padding = 0.0f;
    ++group->emitRequestCount;
}

void ParticleManager::CreateGraphicsPipeline(){
    HRESULT hr = S_OK;

    D3D12_DESCRIPTOR_RANGE descriptorRangeTexture[1] = {};
    descriptorRangeTexture[0].BaseShaderRegister = 0;
    descriptorRangeTexture[0].NumDescriptors = 1;
    descriptorRangeTexture[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeTexture[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE descriptorRangeInstancing[1] = {};
    descriptorRangeInstancing[0].BaseShaderRegister = 0;
    descriptorRangeInstancing[0].NumDescriptors = 1;
    descriptorRangeInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[3] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRangeTexture;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeTexture);

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeInstancing;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeInstancing);

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[2].Descriptor.ShaderRegister = 0;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature,D3D_ROOT_SIGNATURE_VERSION_1,&signatureBlob,&errorBlob);
    if(FAILED(hr)){
        Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }
    hr = dxCommon_->GetDevice()->CreateRootSignature(0,signatureBlob->GetBufferPointer(),signatureBlob->GetBufferSize(),IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));

    ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Particle/Particle.VS.hlsl",L"vs_6_0");
    ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Particle/Particle.PS.hlsl",L"ps_6_0");

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = {inputElementDescs, _countof(inputElementDescs)};
    psoDesc.VS = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};
    psoDesc.PS = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()};

    D3D12_RENDER_TARGET_BLEND_DESC& blendDesc = psoDesc.BlendState.RenderTarget[0];
    blendDesc.BlendEnable = TRUE;
    blendDesc.LogicOpEnable = FALSE;
    blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.DestBlend = D3D12_BLEND_ONE;
    blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    psoDesc.DepthStencilState.DepthEnable = true;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;

    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc,IID_PPV_ARGS(&graphicsPipelineState_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateModel(){
    VertexData vertices[4] = {
        {{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{-1.0f,  1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {{ 1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{ 1.0f,  1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
    };
    vertexBuffer_ = dxCommon_->CreateBufferResource(sizeof(vertices));
    VertexData* vertexData = nullptr;
    vertexBuffer_->Map(0,nullptr,reinterpret_cast<void**>(&vertexData));
    std::memcpy(vertexData,vertices,sizeof(vertices));
    vertexBuffer_->Unmap(0,nullptr);
    vertexBufferView_ = {vertexBuffer_->GetGPUVirtualAddress(), sizeof(vertices), sizeof(VertexData)};
}

void ParticleManager::CreateRingModel(){
    const uint32_t kRingDivide = 32;
    const float kOuterRadius = 1.0f,kInnerRadius = 0.2f;
    const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kRingDivide);

    ringVertexCount_ = kRingDivide * 6;
    std::vector<VertexData> vertices;
    vertices.reserve(ringVertexCount_);
    const Vector3 kNormal = {0.0f, 0.0f, -1.0f};

    for(uint32_t i = 0; i < kRingDivide; ++i){
        float s = std::sin(i * radianPerDivide),c = std::cos(i * radianPerDivide);
        float sN = std::sin((i + 1) * radianPerDivide),cN = std::cos((i + 1) * radianPerDivide);
        float u = float(i) / float(kRingDivide),uN = float(i + 1) / float(kRingDivide);

        vertices.push_back({{-s * kOuterRadius, c * kOuterRadius, 0.0f, 1.0f}, {u, 0.0f}, kNormal});
        vertices.push_back({{-sN * kOuterRadius, cN * kOuterRadius, 0.0f, 1.0f}, {uN, 0.0f}, kNormal});
        vertices.push_back({{-s * kInnerRadius, c * kInnerRadius, 0.0f, 1.0f}, {u, 1.0f}, kNormal});
        vertices.push_back({{-s * kInnerRadius, c * kInnerRadius, 0.0f, 1.0f}, {u, 1.0f}, kNormal});
        vertices.push_back({{-sN * kOuterRadius, cN * kOuterRadius, 0.0f, 1.0f}, {uN, 0.0f}, kNormal});
        vertices.push_back({{-sN * kInnerRadius, cN * kInnerRadius, 0.0f, 1.0f}, {uN, 1.0f}, kNormal});
    }

    size_t size = sizeof(VertexData) * vertices.size();
    ringVertexBuffer_ = dxCommon_->CreateBufferResource(size);
    VertexData* vData = nullptr;
    ringVertexBuffer_->Map(0,nullptr,reinterpret_cast<void**>(&vData));
    std::memcpy(vData,vertices.data(),size);
    ringVertexBuffer_->Unmap(0,nullptr);
    ringVertexBufferView_ = {ringVertexBuffer_->GetGPUVirtualAddress(), (UINT)size, sizeof(VertexData)};
}

void ParticleManager::CreateCylinderModel(){
    const uint32_t kDiv = 32;
    const float kR = 1.0f,kH = 3.0f;
    const float rad = 2.0f * std::numbers::pi_v<float> / float(kDiv);

    cylinderVertexCount_ = kDiv * 6;
    std::vector<VertexData> vtxs;
    vtxs.reserve(cylinderVertexCount_);

    for(uint32_t i = 0; i < kDiv; ++i){
        float s = std::sin(i * rad),c = std::cos(i * rad);
        float sN = std::sin((i + 1) * rad),cN = std::cos((i + 1) * rad);
        float u = float(i) / float(kDiv),uN = float(i + 1) / float(kDiv);

        vtxs.push_back({{-s * kR, kH, c * kR, 1.0f}, {u, 1.0f}, {-s, 0.0f, c}});
        vtxs.push_back({{-sN * kR, kH, cN * kR, 1.0f}, {uN, 1.0f}, {-sN, 0.0f, cN}});
        vtxs.push_back({{-s * kR, 0.0f, c * kR, 1.0f}, {u, 0.0f}, {-s, 0.0f, c}});
        vtxs.push_back({{-s * kR, 0.0f, c * kR, 1.0f}, {u, 0.0f}, {-s, 0.0f, c}});
        vtxs.push_back({{-sN * kR, kH, cN * kR, 1.0f}, {uN, 1.0f}, {-sN, 0.0f, cN}});
        vtxs.push_back({{-sN * kR, 0.0f, cN * kR, 1.0f}, {uN, 0.0f}, {-sN, 0.0f, cN}});
    }
    size_t size = sizeof(VertexData) * vtxs.size();
    cylinderVertexBuffer_ = dxCommon_->CreateBufferResource(size);
    VertexData* p = nullptr;
    cylinderVertexBuffer_->Map(0,nullptr,reinterpret_cast<void**>(&p));
    std::memcpy(p,vtxs.data(),size);
    cylinderVertexBuffer_->Unmap(0,nullptr);
    cylinderVertexBufferView_ = {cylinderVertexBuffer_->GetGPUVirtualAddress(), (UINT)size, sizeof(VertexData)};
}

void ParticleManager::CreateComputePipeline(){
    HRESULT hr = S_OK;
    D3D12_DESCRIPTOR_RANGE uavRange0 = {};
    uavRange0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange0.NumDescriptors = 1;
    uavRange0.BaseShaderRegister = 0;

    D3D12_DESCRIPTOR_RANGE uavRange1 = {};
    uavRange1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange1.NumDescriptors = 1;
    uavRange1.BaseShaderRegister = 1;

    D3D12_DESCRIPTOR_RANGE uavRange2 = {};
    uavRange2.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange2.NumDescriptors = 1;
    uavRange2.BaseShaderRegister = 2;

    D3D12_ROOT_PARAMETER param[4] = {};
    param[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param[0].DescriptorTable.NumDescriptorRanges = 1;
    param[0].DescriptorTable.pDescriptorRanges = &uavRange0;
    param[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    param[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param[1].DescriptorTable.NumDescriptorRanges = 1;
    param[1].DescriptorTable.pDescriptorRanges = &uavRange1;
    param[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    param[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param[2].DescriptorTable.NumDescriptorRanges = 1;
    param[2].DescriptorTable.pDescriptorRanges = &uavRange2;
    param[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // 進める時間(PerFrame)
    param[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    param[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param[3].Descriptor.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC desc = {_countof(param), param, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> sig,err;
    hr = D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&err);
    hr = dxCommon_->GetDevice()->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&computeRootSignature_));

    ComPtr<IDxcBlob> cs = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Particle/UpdateParticle.CS.hlsl",L"cs_6_0");
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {computeRootSignature_.Get(), {cs->GetBufferPointer(), cs->GetBufferSize()}};
    hr = dxCommon_->GetDevice()->CreateComputePipelineState(&psoDesc,IID_PPV_ARGS(&computePipelineState_));
    assert(SUCCEEDED(hr));

    D3D12_ROOT_PARAMETER rootParametersEmit[4] = {};
    rootParametersEmit[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParametersEmit[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParametersEmit[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParametersEmit[0].DescriptorTable.pDescriptorRanges = &uavRange0;

    rootParametersEmit[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParametersEmit[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParametersEmit[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParametersEmit[1].DescriptorTable.pDescriptorRanges = &uavRange1;

    rootParametersEmit[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParametersEmit[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParametersEmit[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParametersEmit[2].DescriptorTable.pDescriptorRanges = &uavRange2;

    // このフレームの発生要求の配列(StructuredBuffer をディスクリプタを使わずアドレスで渡す)
    rootParametersEmit[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParametersEmit[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParametersEmit[3].Descriptor.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignatureEmit{};
    descriptionRootSignatureEmit.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    descriptionRootSignatureEmit.pParameters = rootParametersEmit;
    descriptionRootSignatureEmit.NumParameters = _countof(rootParametersEmit);

    ComPtr<ID3DBlob> signatureBlobEmit;
    ComPtr<ID3DBlob> errorBlobEmit;
    hr = D3D12SerializeRootSignature(&descriptionRootSignatureEmit,D3D_ROOT_SIGNATURE_VERSION_1,&signatureBlobEmit,&errorBlobEmit);
    if(FAILED(hr)){
        Logger::Log(reinterpret_cast<char*>(errorBlobEmit->GetBufferPointer()));
        assert(false);
    }
    hr = dxCommon_->GetDevice()->CreateRootSignature(0,signatureBlobEmit->GetBufferPointer(),signatureBlobEmit->GetBufferSize(),IID_PPV_ARGS(&emitComputeRootSignature_));
    assert(SUCCEEDED(hr));

    ComPtr<IDxcBlob> emitComputeShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Particle/EmitParticle.CS.hlsl",L"cs_6_0");
    assert(emitComputeShaderBlob != nullptr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC emitComputePipelineStateDesc{};
    emitComputePipelineStateDesc.pRootSignature = emitComputeRootSignature_.Get();
    emitComputePipelineStateDesc.CS = {emitComputeShaderBlob->GetBufferPointer(), emitComputeShaderBlob->GetBufferSize()};

    hr = dxCommon_->GetDevice()->CreateComputePipelineState(&emitComputePipelineStateDesc,IID_PPV_ARGS(&emitComputePipelineState_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::EmitShockwave(const Vector3& pos){
    ParticleManager::GetInstance()->Emit("Shockwave",{{0.1f, 0.1f, 0.1f}, {0, 0, 0}, pos},1,{1, 1, 1, 1},{0, 0, 0},0.3f);
}

void ParticleManager::EmitSpark(const Vector3& position){
    ParticleEmitParams params;
    params.position = position;
    params.radius = kSparkRadius;
    params.count = kSparkCount;
    params.lifeTime = kSparkLifeTime;
    params.velocitySpread = kSparkSpeed;
    params.startScale = kSparkStartScale;
    params.endScale = kSparkEndScale;
    params.startColor = kSparkStartColor;
    params.endColor = kSparkEndColor;
    params.gravity = kSparkGravity;
    params.drag = kSparkDrag;

    ParticleManager::GetInstance()->Emit("Spark",params);
}

void ParticleManager::EmitSmoke(const Vector3& position){
    Transform transform;
    transform.translate = position;
    transform.scale = {0.2f, 0.2f, 0.2f};
    transform.rotate = {0.0f, 0.0f, 0.0f};

    ParticleManager::GetInstance()->Emit("Smoke",transform,5,{0.5f, 0.5f, 0.5f, 0.8f},{0.0f, 1.0f, 0.0f},1.5f);
}

void ParticleManager::EmitCharge(const Vector3& position){
    Transform transform;
    transform.translate = position;
    transform.scale = {0.1f, 0.5f, 0.1f};
    transform.rotate = {0.0f, 0.0f, 0.0f};

    ParticleManager::GetInstance()->Emit("Charge",transform,30,{0.2f, 0.8f, 1.0f, 1.0f},{0.0f, 0.0f, 0.0f},0.6f);
}

void ParticleManager::EmitAura(const Vector3& position){
    Transform transform;
    transform.translate = position;
    transform.scale = {0.3f, 0.3f, 0.3f};
    transform.rotate = {0.0f, 0.0f, 0.0f};

    ParticleManager::GetInstance()->Emit("Aura",transform,1,{0.8f, 1.0f, 0.2f, 0.6f},{0.0f, 2.0f, 0.0f},1.0f);
}

void ParticleManager::EmitWarp(){
    Transform transform;
    transform.translate = {0.0f, 0.0f, 0.0f};
    transform.scale = {1.0f, 1.0f, 1.0f};
    transform.rotate = {0.0f, 0.0f, 0.0f};

    ParticleManager::GetInstance()->Emit("Warp",transform,100,{0.5f, 0.8f, 1.0f, 0.8f},{0.0f, 0.0f, 0.0f},1.0f);
}