#include "MeshParticleSystem.h"
#include "DXCommon.h"
#include "Obj3DCommon.h"
#include "ModelManager.h"
#include "ModelCommon.h"
#include "Model.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "Camera.h"
#include "Logger.h"
#include <algorithm>
#include <cassert>
#include <cstring>

using Microsoft::WRL::ComPtr;

// =============================================================================
// GPU へ渡す構造体(MeshParticle.hlsli の同じ名前の構造体と並びを合わせる)
// =============================================================================

struct MeshParticleSystem::GpuParticle{
	Vector3 position;
	float lifeTime;
	Vector3 velocity;
	float maxLifeTime;
	Vector3 rotate;
	float baseScale;
	Vector3 angularVelocity;
	uint32_t kind;
	Vector4 color;
	uint32_t isAlive;
	uint32_t reportsCrossing;
	uint32_t generation;
	float padding;
};

struct MeshParticleSystem::GpuSpawnRequest{
	GpuParticle particle;
	uint32_t slot;
	uint32_t padding[3];
};

struct MeshParticleSystem::GpuInstance{
	Matrix4x4 world;
	Matrix4x4 worldInverseTranspose;
	Vector4 color;
};

struct MeshParticleSystem::GpuCrossingReport{
	Vector3 from;
	float baseScale;
	Vector3 to;
	uint32_t slot;
	uint32_t generation;
	uint32_t padding[3];
};

struct MeshParticleSystem::GpuKindParams{
	float gravity;
	float stretchPerSpeed;
	float maxStretch;
	float minStretchSpeed;
	float shrinkStartRate;
	float expandScale;
	uint32_t expandsAndFades;
	uint32_t stretchesAlongVelocity;
	Vector3 modelOffset;
	float modelScale;
	float drag;
	uint32_t changesColor;
	float padding[2];
	Vector4 endColor;
};

struct MeshParticleSystem::GpuFrameConstants{
	float deltaTime;
	float crossingHeight;
	uint32_t detectsCrossing;
	uint32_t particleCount;
	uint32_t maxReports;
	uint32_t padding[3];
	GpuKindParams kinds[kMaxKinds];
};

struct MeshParticleSystem::GpuViewConstants{
	Matrix4x4 viewProjection;
	float distanceFadeStart;
	float distanceFadeEnd;
	float alphaCutoff;
	float shininess;
};

struct MeshParticleSystem::GpuDrawConstants{
	uint32_t instanceOffset;
	uint32_t enableLighting;
};

namespace{
	// シェーダー側の構造体の大きさ(並びがずれていないかを確かめる。MeshParticle.hlsli を変えたらこちらも変える)
	constexpr size_t kGpuParticleSize = 96;
	constexpr size_t kGpuSpawnRequestSize = 112;
	constexpr size_t kGpuInstanceSize = 144;
	constexpr size_t kGpuCrossingReportSize = 48;
	constexpr size_t kGpuKindParamsSize = 80;

	// コンピュートシェーダーの1グループのスレッド数(MeshParticleSpawn.CS.hlsl / MeshParticleUpdate.CS.hlsl の numthreads と合わせる)
	constexpr uint32_t kThreadGroupSize = 64;
	// 定数バッファは 256 バイト単位で確保する
	constexpr size_t kConstantBufferAlignment = 256;
	// このフレームにまだ要求を積んでいない入れ物の印
	constexpr uint32_t kNoRequest = UINT32_MAX;
	// 知らせを CPU で読むバッファの中の、知らせの並びの始まり(先頭の数の後ろを、16 バイトの並びに合わせて空ける)
	constexpr size_t kReportDataOffset = 16;
	// 鏡面反射の鋭さ(Obj3D のマテリアルの初期値と同じ)
	constexpr float kShininess = 20.0f;
	// 遠くを溶け込ませる空の色を読むスカイボックスのテクスチャ(Obj3D と同じ)
	const std::string kEnvironmentTexturePath = "resource/Skybox/celestialSphere.dds";

	// コンピュートシェーダーのルートパラメーターの番号
	enum ComputeRootParameter : UINT{
		kComputeFrameConstants,  // b0 1フレーム分の設定
		kComputeSpawnConstants,  // b1 出す要求の数(ルート定数)
		kComputeSpawnRequests,   // t0 出す要求
		kComputeParticles,       // u0 粒の状態
		kComputeInstances,       // u1 描画用の行列と色
		kComputeReports,         // u2 横切った粒の知らせ
		kComputeReportCount,     // u3 知らせの数
		kComputeRootParameterCount,
	};
	// 出す要求の数のルート定数の個数
	constexpr UINT kSpawnConstantCount = 1;

	// 描画のルートパラメーターの番号
	enum GraphicsRootParameter : UINT{
		kGraphicsDrawConstants,     // b0 描画1回分の設定(ルート定数)
		kGraphicsViewConstants,     // b1 カメラと描き方の設定
		kGraphicsInstances,         // t0(頂点シェーダー) 描画用の行列と色
		kGraphicsTexture,           // t0(ピクセルシェーダー) モデルのテクスチャ
		kGraphicsEnvironment,       // t1 スカイボックス
		kGraphicsDirectionalLight,  // b2 平行光源
		kGraphicsCamera,            // b3 カメラの位置
		kGraphicsPointLight,        // b4 点光源
		kGraphicsSpotLight,         // b5 スポットライト
		kGraphicsRootParameterCount,
	};
	// 描画1回分の設定のルート定数の個数
	constexpr UINT kDrawConstantCount = 2;

	// count 個の要素を処理するのに必要なスレッドグループの数
	uint32_t GetDispatchGroupCount(uint32_t count){
		return (count + kThreadGroupSize - 1) / kThreadGroupSize;
	}

	// 定数バッファの大きさを 256 バイト単位に切り上げる
	size_t AlignConstantBufferSize(size_t size){
		return (size + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1);
	}

	// 状態を切り替えるバリア
	D3D12_RESOURCE_BARRIER MakeTransitionBarrier(ID3D12Resource* resource,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource;
		barrier.Transition.StateBefore = before;
		barrier.Transition.StateAfter = after;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		return barrier;
	}

	// ルートシグネチャを作る(失敗したら理由を出力ウィンドウに出して止める)
	ComPtr<ID3D12RootSignature> CreateRootSignature(ID3D12Device* device,const D3D12_ROOT_SIGNATURE_DESC& desc){
		ComPtr<ID3DBlob> signatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&signatureBlob,&errorBlob);
		if(FAILED(hr)){
			Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
			assert(false);
		}
		ComPtr<ID3D12RootSignature> rootSignature;
		hr = device->CreateRootSignature(0,signatureBlob->GetBufferPointer(),signatureBlob->GetBufferSize(),IID_PPV_ARGS(&rootSignature));
		assert(SUCCEEDED(hr));
		return rootSignature;
	}
}

MeshParticleSystem::MeshParticleSystem() = default;
MeshParticleSystem::~MeshParticleSystem() = default;

// =============================================================================
// 初期化
// =============================================================================

void MeshParticleSystem::Initialize(Obj3dCommon* objCommon,const std::vector<KindDesc>& kinds){
	// GPU へ渡す構造体の並びが、シェーダー側とずれていないかを確かめる
	static_assert(sizeof(GpuParticle) == kGpuParticleSize,"MeshParticle.hlsli の MeshParticle と大きさが違う");
	static_assert(sizeof(GpuSpawnRequest) == kGpuSpawnRequestSize,"MeshParticle.hlsli の SpawnRequest と大きさが違う");
	static_assert(sizeof(GpuInstance) == kGpuInstanceSize,"MeshParticle.hlsli の MeshParticleInstance と大きさが違う");
	static_assert(sizeof(GpuCrossingReport) == kGpuCrossingReportSize,"MeshParticle.hlsli の CrossingReport と大きさが違う");
	static_assert(sizeof(GpuKindParams) == kGpuKindParamsSize,"MeshParticle.hlsli の KindParams と大きさが違う");

	assert(objCommon);
	assert(!kinds.empty() && kinds.size() <= kMaxKinds);
	objCommon_ = objCommon;
	dxCommon_ = objCommon->GetDxCommon();

	// 種類ごとのモデルを読み込み、全種類の入れ物を1つの配列に順に並べる
	kinds_.clear();
	kinds_.reserve(kinds.size());
	particleCount_ = 0;
	for(const KindDesc& desc : kinds){
		ModelManager::GetInstance()->LoadModel(desc.modelPath);
		Kind kind;
		kind.model = ModelManager::GetInstance()->FindModel(desc.modelPath);
		assert(kind.model && "MeshParticleSystem: モデルを読み込めなかった");
		kind.firstSlot = particleCount_;
		kind.capacity = desc.capacity;
		kind.enableLighting = desc.enableLighting;
		kinds_.push_back(kind);
		particleCount_ += desc.capacity;
	}
	assert(particleCount_ > 0);

	generations_.assign(particleCount_,0);
	requestIndexOfSlot_.assign(particleCount_,kNoRequest);
	requestedSlots_.clear();
	requestedSlots_.reserve(particleCount_);
	spawnRequestCount_ = 0;
	hasPendingReports_ = false;
	pendingDeltaTime_ = 0.0f;

	CreateBuffers();
	CreateComputePipelines();
	CreateGraphicsPipeline();

	// 種類ごとの動き方と見た目の設定は変わらないため、ここで一度だけ書き込む
	for(size_t i = 0; i < kinds.size(); ++i){
		const KindDesc& desc = kinds[i];
		GpuKindParams params{};
		params.gravity = desc.gravity;
		params.stretchPerSpeed = desc.stretchPerSpeed;
		params.maxStretch = desc.maxStretch;
		params.minStretchSpeed = desc.minStretchSpeed;
		params.shrinkStartRate = desc.shrinkStartRate;
		params.expandScale = desc.expandScale;
		params.expandsAndFades = desc.expandsAndFades?1u:0u;
		params.stretchesAlongVelocity = desc.stretchesAlongVelocity?1u:0u;
		params.modelOffset = desc.modelOffset;
		params.modelScale = desc.modelScale;
		params.drag = desc.drag;
		params.changesColor = desc.changesColor?1u:0u;
		params.endColor = desc.endColor;
		frameConstants_->kinds[i] = params;
	}

	// 作ったばかりのバッファの中身に頼らず、すべての入れ物を空きにしてから始める
	Clear();
}

// 既定のヒープに、コンピュートシェーダーから読み書きできるバッファを作る
ComPtr<ID3D12Resource> MeshParticleSystem::CreateDefaultBuffer(size_t sizeInBytes){
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	ComPtr<ID3D12Resource> resource;
	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(&heapProperties,D3D12_HEAP_FLAG_NONE,&resourceDesc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}

void MeshParticleSystem::CreateBuffers(){
	// GPU だけが読み書きするバッファ(知らせは、入れ物の数より多く出ることはない)
	particleBuffer_ = CreateDefaultBuffer(sizeof(GpuParticle) * particleCount_);
	instanceBuffer_ = CreateDefaultBuffer(sizeof(GpuInstance) * particleCount_);
	reportBuffer_ = CreateDefaultBuffer(sizeof(GpuCrossingReport) * particleCount_);
	reportCountBuffer_ = CreateDefaultBuffer(sizeof(uint32_t));

	// 知らせの数を毎フレーム 0 に戻すときの写し元
	zeroCountBuffer_ = dxCommon_->CreateBufferResource(sizeof(uint32_t));
	uint32_t* zeroCount = nullptr;
	zeroCountBuffer_->Map(0,nullptr,reinterpret_cast<void**>(&zeroCount));
	*zeroCount = 0;
	zeroCountBuffer_->Unmap(0,nullptr);

	// 知らせを CPU で読むバッファ(GPU からの書き込み先として作り、開いたままにしておく)
	D3D12_HEAP_PROPERTIES readbackHeap{};
	readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
	D3D12_RESOURCE_DESC readbackDesc{};
	readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	readbackDesc.Width = kReportDataOffset + sizeof(GpuCrossingReport) * particleCount_;
	readbackDesc.Height = 1;
	readbackDesc.DepthOrArraySize = 1;
	readbackDesc.MipLevels = 1;
	readbackDesc.SampleDesc.Count = 1;
	readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(&readbackHeap,D3D12_HEAP_FLAG_NONE,&readbackDesc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readbackBuffer_));
	assert(SUCCEEDED(hr));
	void* readbackData = nullptr;
	readbackBuffer_->Map(0,nullptr,&readbackData);
	readbackData_ = static_cast<const uint8_t*>(readbackData);

	// CPU から書き込むバッファ(GPU の処理はフレームごとに待ってから次のフレームを書くため、1つを毎フレーム書き直して使える)
	// 1つの入れ物への要求は1フレームに1つにまとめるため、要求の数は入れ物の数を超えない
	spawnRequestBuffer_ = dxCommon_->CreateBufferResource(sizeof(GpuSpawnRequest) * particleCount_);
	spawnRequestBuffer_->Map(0,nullptr,reinterpret_cast<void**>(&spawnRequests_));

	frameConstantBuffer_ = dxCommon_->CreateBufferResource(AlignConstantBufferSize(sizeof(GpuFrameConstants)));
	frameConstantBuffer_->Map(0,nullptr,reinterpret_cast<void**>(&frameConstants_));
	*frameConstants_ = {};

	viewConstantBuffer_ = dxCommon_->CreateBufferResource(AlignConstantBufferSize(sizeof(GpuViewConstants)));
	viewConstantBuffer_->Map(0,nullptr,reinterpret_cast<void**>(&viewConstants_));
	*viewConstants_ = {};
}

void MeshParticleSystem::CreateComputePipelines(){
	// 出す処理と進める処理で同じルートシグネチャを使う(バッファはディスクリプタを使わずアドレスで渡す)
	D3D12_ROOT_PARAMETER rootParameters[kComputeRootParameterCount] = {};

	rootParameters[kComputeFrameConstants].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[kComputeFrameConstants].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[kComputeFrameConstants].Descriptor.ShaderRegister = 0;

	rootParameters[kComputeSpawnConstants].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[kComputeSpawnConstants].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[kComputeSpawnConstants].Constants.ShaderRegister = 1;
	rootParameters[kComputeSpawnConstants].Constants.Num32BitValues = kSpawnConstantCount;

	rootParameters[kComputeSpawnRequests].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	rootParameters[kComputeSpawnRequests].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[kComputeSpawnRequests].Descriptor.ShaderRegister = 0;

	// u0〜u3 を順に、粒の状態・描画用の行列と色・知らせ・知らせの数に割り当てる
	const UINT uavParameters[] = {kComputeParticles, kComputeInstances, kComputeReports, kComputeReportCount};
	for(UINT i = 0; i < _countof(uavParameters); ++i){
		rootParameters[uavParameters[i]].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		rootParameters[uavParameters[i]].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParameters[uavParameters[i]].Descriptor.ShaderRegister = i;
	}

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	computeRootSignature_ = CreateRootSignature(dxCommon_->GetDevice(),rootSignatureDesc);

	ComPtr<IDxcBlob> spawnShader = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/MeshParticle/MeshParticleSpawn.CS.hlsl",L"cs_6_0");
	assert(spawnShader != nullptr);
	D3D12_COMPUTE_PIPELINE_STATE_DESC spawnDesc{};
	spawnDesc.pRootSignature = computeRootSignature_.Get();
	spawnDesc.CS = {spawnShader->GetBufferPointer(), spawnShader->GetBufferSize()};
	HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(&spawnDesc,IID_PPV_ARGS(&spawnPipelineState_));
	assert(SUCCEEDED(hr));

	ComPtr<IDxcBlob> updateShader = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/MeshParticle/MeshParticleUpdate.CS.hlsl",L"cs_6_0");
	assert(updateShader != nullptr);
	D3D12_COMPUTE_PIPELINE_STATE_DESC updateDesc{};
	updateDesc.pRootSignature = computeRootSignature_.Get();
	updateDesc.CS = {updateShader->GetBufferPointer(), updateShader->GetBufferSize()};
	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&updateDesc,IID_PPV_ARGS(&updatePipelineState_));
	assert(SUCCEEDED(hr));
}

void MeshParticleSystem::CreateGraphicsPipeline(){
	// モデルのテクスチャ(t0)とスカイボックス(t1)は、Obj3D と同じくディスクリプタテーブルで渡す
	D3D12_DESCRIPTOR_RANGE textureRange[1] = {};
	textureRange[0].BaseShaderRegister = 0;
	textureRange[0].NumDescriptors = 1;
	textureRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	textureRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE environmentRange[1] = {};
	environmentRange[0].BaseShaderRegister = 1;
	environmentRange[0].NumDescriptors = 1;
	environmentRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	environmentRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[kGraphicsRootParameterCount] = {};

	rootParameters[kGraphicsDrawConstants].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[kGraphicsDrawConstants].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[kGraphicsDrawConstants].Constants.ShaderRegister = 0;
	rootParameters[kGraphicsDrawConstants].Constants.Num32BitValues = kDrawConstantCount;

	rootParameters[kGraphicsViewConstants].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[kGraphicsViewConstants].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[kGraphicsViewConstants].Descriptor.ShaderRegister = 1;

	// 行列と色の配列は頂点シェーダーだけで読むため、ピクセルシェーダーのテクスチャと同じ t0 を使える
	rootParameters[kGraphicsInstances].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	rootParameters[kGraphicsInstances].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[kGraphicsInstances].Descriptor.ShaderRegister = 0;

	rootParameters[kGraphicsTexture].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[kGraphicsTexture].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[kGraphicsTexture].DescriptorTable.pDescriptorRanges = textureRange;
	rootParameters[kGraphicsTexture].DescriptorTable.NumDescriptorRanges = _countof(textureRange);

	rootParameters[kGraphicsEnvironment].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[kGraphicsEnvironment].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[kGraphicsEnvironment].DescriptorTable.pDescriptorRanges = environmentRange;
	rootParameters[kGraphicsEnvironment].DescriptorTable.NumDescriptorRanges = _countof(environmentRange);

	// b2〜b5 を順に、平行光源・カメラの位置・点光源・スポットライトに割り当てる(Obj3D と同じ番号)
	const UINT lightParameters[] = {kGraphicsDirectionalLight, kGraphicsCamera, kGraphicsPointLight, kGraphicsSpotLight};
	constexpr UINT kFirstLightRegister = 2;
	for(UINT i = 0; i < _countof(lightParameters); ++i){
		rootParameters[lightParameters[i]].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[lightParameters[i]].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[lightParameters[i]].Descriptor.ShaderRegister = kFirstLightRegister + i;
	}

	// サンプラーは Obj3D と同じ
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pStaticSamplers = staticSamplers;
	rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);
	graphicsRootSignature_ = CreateRootSignature(dxCommon_->GetDevice(),rootSignatureDesc);

	ComPtr<IDxcBlob> vertexShader = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/MeshParticle/MeshParticle.VS.hlsl",L"vs_6_0");
	assert(vertexShader != nullptr);
	ComPtr<IDxcBlob> pixelShader = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/MeshParticle/MeshParticle.PS.hlsl",L"ps_6_0");
	assert(pixelShader != nullptr);

	// 頂点の並びはモデルの頂点バッファ(Model::VertexData)をそのまま使うため、Obj3D と同じにする
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	// 合成・カリング・深度は Obj3D と同じ(通常のアルファブレンド・両面を描く・深度を書き込む)
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
	pipelineDesc.pRootSignature = graphicsRootSignature_.Get();
	pipelineDesc.InputLayout = {inputElementDescs, _countof(inputElementDescs)};
	pipelineDesc.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
	pipelineDesc.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
	pipelineDesc.BlendState = blendDesc;
	pipelineDesc.RasterizerState = rasterizerDesc;
	pipelineDesc.DepthStencilState = depthStencilDesc;
	pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&pipelineDesc,IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
}

// =============================================================================
// 出す・消す
// =============================================================================

void MeshParticleSystem::Spawn(uint32_t kindIndex,const SpawnDesc& desc){
	assert(kindIndex < kinds_.size());
	Kind& kind = kinds_[kindIndex];
	if(kind.capacity == 0){
		return;
	}

	// この種類の入れ物を順に回して使い、空きが無いときは一番古いものを上書きする
	const uint32_t slot = kind.firstSlot + kind.nextSlot;
	kind.nextSlot = (kind.nextSlot + 1) % kind.capacity;

	GpuParticle particle{};
	particle.position = desc.position;
	particle.lifeTime = 0.0f;
	particle.velocity = desc.velocity;
	particle.maxLifeTime = desc.lifeTime;
	particle.rotate = desc.rotate;
	particle.baseScale = desc.baseScale;
	particle.angularVelocity = desc.angularVelocity;
	particle.kind = kindIndex;
	particle.color = desc.color;
	particle.isAlive = 1;
	particle.reportsCrossing = desc.reportsCrossing?1u:0u;
	RequestWrite(slot,particle);
}

void MeshParticleSystem::Clear(){
	for(uint32_t slot = 0; slot < particleCount_; ++slot){
		RequestKill(slot);
	}
	// 消す前に届いていた知らせは、もう受け取らない
	hasPendingReports_ = false;
}

void MeshParticleSystem::RequestKill(uint32_t slot){
	// すべて 0 の状態(出ていない)を書き込む
	RequestWrite(slot,GpuParticle{});
}

void MeshParticleSystem::RequestWrite(uint32_t slot,const GpuParticle& particle){
	// 書き込むたびに使い回した回数を増やし、前の粒について届く知らせと区別できるようにする
	GpuSpawnRequest request{};
	request.particle = particle;
	request.particle.generation = ++generations_[slot];
	request.slot = slot;

	// 同じフレームに同じ入れ物へ書き込むときは、前の要求を上書きして1つにまとめる
	// (GPU で同じ入れ物へ同時に書き込むと、どちらが残るか決まらないため)
	uint32_t& requestIndex = requestIndexOfSlot_[slot];
	if(requestIndex == kNoRequest){
		requestIndex = spawnRequestCount_++;
		requestedSlots_.push_back(slot);
	}
	// GPU へ渡すバッファは CPU から読むと遅いため、組み立ててから丸ごと書き込む
	spawnRequests_[requestIndex] = request;
}

void MeshParticleSystem::SetCrossing(float height,CrossingHandler handler){
	crossingHeight_ = height;
	crossingHandler_ = std::move(handler);
}

// =============================================================================
// 更新・描画
// =============================================================================

void MeshParticleSystem::Update(float deltaTime){
	// 前のフレームに横切った粒を受け取る処理へ渡し、面に着いたものを消す
	// GPU の処理はフレームの終わりに待っているため、前のフレームに写した知らせはもう読める
	if(hasPendingReports_){
		hasPendingReports_ = false;

		uint32_t reportCount = 0;
		std::memcpy(&reportCount,readbackData_,sizeof(reportCount));
		reportCount = (std::min)(reportCount,particleCount_);

		for(uint32_t i = 0; i < reportCount; ++i){
			GpuCrossingReport report{};
			std::memcpy(&report,readbackData_ + kReportDataOffset + sizeof(GpuCrossingReport) * i,sizeof(report));

			// 知らせが届くまでの間に、別の粒へ入れ替わった(または消した)入れ物は受け取らない
			if(report.slot >= particleCount_ || report.generation != generations_[report.slot]){
				continue;
			}
			if(crossingHandler_ && crossingHandler_(report.from,report.to,report.baseScale)){
				RequestKill(report.slot);
			}
		}
	}

	pendingDeltaTime_ = deltaTime;
}

void MeshParticleSystem::Draw(const Camera& camera){
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	// このフレームに進める時間を受け取り、0 に戻す(Update() を呼ばないフレームは止める)
	const float deltaTime = pendingDeltaTime_;
	pendingDeltaTime_ = 0.0f;
	// 横切ったかを調べるのは、粒が動くフレームで、受け取る処理があり、前の知らせを読み終えているときだけ
	// (止まっているフレームに知らせの数を 0 に戻すと、まだ読んでいない知らせが消えてしまうため)
	const bool detectsCrossing = deltaTime > 0.0f && crossingHandler_ && !hasPendingReports_;

	// 1フレーム分の設定とカメラの設定を書き込む(GPU へ渡すバッファなので書き込むだけにする)
	frameConstants_->deltaTime = deltaTime;
	frameConstants_->crossingHeight = crossingHeight_;
	frameConstants_->detectsCrossing = detectsCrossing?1u:0u;
	frameConstants_->particleCount = particleCount_;
	frameConstants_->maxReports = particleCount_;

	GpuViewConstants viewConstants{};
	viewConstants.viewProjection = camera.GetViewProjectionMatrix();
	// 遠くを空の色に溶け込ませる距離は、Obj3D と同じく全オブジェクト共通の値を使う
	viewConstants.distanceFadeStart = objCommon_->GetSceneDistanceFadeStart();
	viewConstants.distanceFadeEnd = objCommon_->GetSceneDistanceFadeEnd();
	viewConstants.alphaCutoff = Model::kDefaultAlphaCutoff;
	viewConstants.shininess = kShininess;
	*viewConstants_ = viewConstants;

	// --- GPU で粒を進める ---
	// バッファはコマンドの実行が終わるたびに COMMON の状態に戻るため、毎フレーム COMMON から書き込める状態にする
	const D3D12_RESOURCE_BARRIER toUnorderedAccess[] = {
		MakeTransitionBarrier(particleBuffer_.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		MakeTransitionBarrier(instanceBuffer_.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		MakeTransitionBarrier(reportBuffer_.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
	};
	commandList->ResourceBarrier(_countof(toUnorderedAccess),toUnorderedAccess);

	if(detectsCrossing){
		// 知らせの数を 0 に戻してから書き込める状態にする
		const D3D12_RESOURCE_BARRIER toCopyDest = MakeTransitionBarrier(reportCountBuffer_.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1,&toCopyDest);
		commandList->CopyBufferRegion(reportCountBuffer_.Get(),0,zeroCountBuffer_.Get(),0,sizeof(uint32_t));
		const D3D12_RESOURCE_BARRIER copyToUav = MakeTransitionBarrier(reportCountBuffer_.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(1,&copyToUav);
	} else{
		const D3D12_RESOURCE_BARRIER toUav = MakeTransitionBarrier(reportCountBuffer_.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(1,&toUav);
	}

	commandList->SetComputeRootSignature(computeRootSignature_.Get());
	commandList->SetComputeRootConstantBufferView(kComputeFrameConstants,frameConstantBuffer_->GetGPUVirtualAddress());
	commandList->SetComputeRootShaderResourceView(kComputeSpawnRequests,spawnRequestBuffer_->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(kComputeParticles,particleBuffer_->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(kComputeInstances,instanceBuffer_->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(kComputeReports,reportBuffer_->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(kComputeReportCount,reportCountBuffer_->GetGPUVirtualAddress());

	// このフレームに積んだ要求どおりに、入れ物へ書き込む
	if(spawnRequestCount_ > 0){
		commandList->SetPipelineState(spawnPipelineState_.Get());
		commandList->SetComputeRoot32BitConstants(kComputeSpawnConstants,kSpawnConstantCount,&spawnRequestCount_,0);
		commandList->Dispatch(GetDispatchGroupCount(spawnRequestCount_),1,1);

		// 書き込み終えてから進める
		D3D12_RESOURCE_BARRIER uavBarrier{};
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = particleBuffer_.Get();
		commandList->ResourceBarrier(1,&uavBarrier);

		// 記録し終えたので、次のフレームの要求を最初から積む
		for(uint32_t slot : requestedSlots_){
			requestIndexOfSlot_[slot] = kNoRequest;
		}
		requestedSlots_.clear();
		spawnRequestCount_ = 0;
	}

	// 出ている粒を進め、描画用の行列と色を求める
	commandList->SetPipelineState(updatePipelineState_.Get());
	commandList->Dispatch(GetDispatchGroupCount(particleCount_),1,1);

	// 描画用の行列と色を頂点シェーダーで読める状態にする
	const D3D12_RESOURCE_BARRIER toShaderResource = MakeTransitionBarrier(instanceBuffer_.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1,&toShaderResource);

	// 横切った粒の知らせを CPU で読むバッファへ写す(次のフレームの Update() で読む)
	if(detectsCrossing){
		const D3D12_RESOURCE_BARRIER toCopySource[] = {
			MakeTransitionBarrier(reportCountBuffer_.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE),
			MakeTransitionBarrier(reportBuffer_.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE),
		};
		commandList->ResourceBarrier(_countof(toCopySource),toCopySource);
		commandList->CopyBufferRegion(readbackBuffer_.Get(),0,reportCountBuffer_.Get(),0,sizeof(uint32_t));
		commandList->CopyBufferRegion(readbackBuffer_.Get(),kReportDataOffset,reportBuffer_.Get(),0,sizeof(GpuCrossingReport) * particleCount_);
		hasPendingReports_ = true;
	}

	// --- 種類ごとにまとめて描く ---
	commandList->SetGraphicsRootSignature(graphicsRootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(kGraphicsViewConstants,viewConstantBuffer_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootShaderResourceView(kGraphicsInstances,instanceBuffer_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(kGraphicsEnvironment,TextureManager::GetInstance()->GetSrvHandleGPU(kEnvironmentTexturePath));
	// ライトとカメラの位置は Obj3D と同じものを使う
	commandList->SetGraphicsRootConstantBufferView(kGraphicsDirectionalLight,ModelManager::GetInstance()->GetModelCommon()->GetLightResource()->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(kGraphicsCamera,camera.GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(kGraphicsPointLight,objCommon_->GetPointLightDataGPU());
	commandList->SetGraphicsRootConstantBufferView(kGraphicsSpotLight,objCommon_->GetSpotLightDataGPU());

	for(const Kind& kind : kinds_){
		const GpuDrawConstants drawConstants = {kind.firstSlot, kind.enableLighting?1u:0u};
		commandList->SetGraphicsRoot32BitConstants(kGraphicsDrawConstants,kDrawConstantCount,&drawConstants,0);
		commandList->SetGraphicsRootDescriptorTable(kGraphicsTexture,SrvManager::GetInstance()->GetGPUDescriptorHandle(kind.model->GetModelData().material.textureIndex));
		// 出ていない入れ物も含めて全部描く(出ていないものは面積の無い三角形になり、描かれない)
		kind.model->DrawInstanced(kind.capacity);
	}
}
