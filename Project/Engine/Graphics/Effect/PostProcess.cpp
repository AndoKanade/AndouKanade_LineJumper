#include "PostProcess.h"
#include <d3dx12.h>
#include <cassert>

// --- 初期化処理 ---
void PostProcess::Initialize(DXCommon* dxCommon){
	// ルートシグネチャの生成
	CreateRootSignature(dxCommon->GetDevice());

	// 各種ポストプロセス用パイプラインステート(PSO)の生成
	CreatePipelineState(dxCommon->GetDevice(),dxCommon,Type::PostProcess,L"Engine/Graphics/Shaders/PostProcess/PostProcess.PS.hlsl");
	CreatePipelineState(dxCommon->GetDevice(),dxCommon,Type::BoxFilter,L"Engine/Graphics/Shaders/PostProcess/BoxFilter.PS.hlsl");
	CreatePipelineState(dxCommon->GetDevice(),dxCommon,Type::Grayscale,L"Engine/Graphics/Shaders/PostProcess/Grayscale.PS.hlsl");
	CreatePipelineState(dxCommon->GetDevice(),dxCommon,Type::Vignette,L"Engine/Graphics/Shaders/PostProcess/Vignette.PS.hlsl");
	CreatePipelineState(dxCommon->GetDevice(),dxCommon,Type::GaussianBlur,L"Engine/Graphics/Shaders/PostProcess/GaussianBlur.PS.hlsl");
	CreatePipelineState(dxCommon->GetDevice(),dxCommon,Type::LuminanceOutline,L"Engine/Graphics/Shaders/PostProcess/LuminanceBasedOutline.PS.hlsl");
	CreatePipelineState(dxCommon->GetDevice(),dxCommon,Type::DepthOutline,L"Engine/Graphics/Shaders/PostProcess/DepthBasedOutline.PS.hlsl");
	CreatePipelineState(dxCommon->GetDevice(),dxCommon,Type::RadialBlur,L"Engine/Graphics/Shaders/PostProcess/RadialBlur.PS.hlsl");
	CreatePipelineState(dxCommon->GetDevice(),dxCommon,Type::Dissolve,L"Engine/Graphics/Shaders/PostProcess/Dissolve.PS.hlsl");
	CreatePipelineState(dxCommon->GetDevice(),dxCommon,Type::Random,L"Engine/Graphics/Shaders/PostProcess/Random.PS.hlsl");
	CreatePipelineState(dxCommon->GetDevice(),dxCommon,Type::Glitch,L"Engine/Graphics/Shaders/PostProcess/Glitch.PS.hlsl");
	CreatePipelineState(dxCommon->GetDevice(),dxCommon,Type::DamageInk,L"Engine/Graphics/Shaders/PostProcess/DamageInk.PS.hlsl");

	// 定数バッファの生成とマッピング
	constBuff_ = dxCommon->CreateBufferResource(sizeof(PostProcessData));
	constBuff_->Map(0,nullptr,reinterpret_cast<void**>(&constMap_));

	// 各種パラメータの初期値設定
	constMap_->kernelSize = 1;
	constMap_->vignetteIntensity = 0.5f;
	constMap_->vignetteScale = 0.8f;
	// スライドの定数値を初期値として設定
	constMap_->radialBlurCenter = {0.5f, 0.5f};
	constMap_->radialBlurWidth = 0.01f;
	constMap_->dissolveThreshold = 0.5f;
	constMap_->randomIntensity = 0.5f;
	constMap_->randomTime = 0.0f;
	constMap_->damageAmount = 0.0f;
	constMap_->damageFlash = 0.0f;
}

// --- 描画処理 ---
void PostProcess::Draw(ID3D12GraphicsCommandList* commandList,
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
	D3D12_GPU_DESCRIPTOR_HANDLE depthTextureHandle,
	Type type){
	// 使用するパイプラインとルートシグネチャの設定
	commandList->SetPipelineState(pipelineStates_[type].Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	// リソース（元画像、定数バッファ、深度画像）のバインド
	commandList->SetGraphicsRootDescriptorTable(0,textureHandle);
	commandList->SetGraphicsRootConstantBufferView(1,constBuff_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(2,depthTextureHandle);

	// トポロジー設定と描画（頂点シェーダー側で生成する3頂点による全画面三角形）
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(3,1,0,0);
}

// --- 内部関数: ルートシグネチャ生成 ---
void PostProcess::CreateRootSignature(ID3D12Device* device){
	HRESULT hr;
	D3D12_ROOT_PARAMETER rootParameters[3] = {};

	// [0]: 元画像テクスチャ用 (register t0)
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange[0];
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// [1]: ポストプロセス設定用定数バッファ (register b1)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].Descriptor.ShaderRegister = 1;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// [2]: 深度テクスチャ用 (register t1)
	D3D12_DESCRIPTOR_RANGE depthDescriptorRange[1] = {};
	depthDescriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	depthDescriptorRange[0].NumDescriptors = 1;
	depthDescriptorRange[0].BaseShaderRegister = 1;
	depthDescriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &depthDescriptorRange[0];
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// 静的サンプラー設定（s0: 線形補間、s1: ポイントサンプリング/深度用）
	D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[1].ShaderRegister = 1;
	staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// ルートシグネチャの構築
	D3D12_ROOT_SIGNATURE_DESC description = {};
	description.pParameters = rootParameters;
	description.NumParameters = _countof(rootParameters);
	description.pStaticSamplers = staticSamplers;
	description.NumStaticSamplers = _countof(staticSamplers);
	description.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> signatureBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&description,D3D_ROOT_SIGNATURE_VERSION_1,&signatureBlob,&errorBlob);
	if(FAILED(hr)){ assert(false); }

	hr = device->CreateRootSignature(0,signatureBlob->GetBufferPointer(),signatureBlob->GetBufferSize(),IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

// --- 内部関数: パイプラインステート生成 ---
void PostProcess::CreatePipelineState(ID3D12Device* device,DXCommon* dxCommon,Type type,const std::wstring& filename){
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	// 各種シェーダーのコンパイルと設定
	auto vsBlob = dxCommon->CompileShader(L"Engine/Graphics/Shaders/PostProcess/PostProcess.VS.hlsl",L"vs_6_0");
	auto psBlob = dxCommon->CompileShader(filename,L"ps_6_0");
	psoDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
	psoDesc.PS = {psBlob->GetBufferPointer(), psBlob->GetBufferSize()};

	// 各種レンダリングステートの設定（ポストプロセスのため入力レイアウトや深度テストは不使用）
	psoDesc.InputLayout.pInputElementDescs = nullptr;
	psoDesc.InputLayout.NumElements = 0;
	psoDesc.DepthStencilState.DepthEnable = false;
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// 出力先設定（R8G8B8A8_SRGB）
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleDesc.Count = 1;

	// パイプラインステートの生成と保存
	ComPtr<ID3D12PipelineState> pipelineState;
	HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc,IID_PPV_ARGS(&pipelineState));
	assert(SUCCEEDED(hr));

	pipelineStates_[type] = pipelineState;
}