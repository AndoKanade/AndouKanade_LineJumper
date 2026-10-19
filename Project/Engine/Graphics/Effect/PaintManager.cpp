#include "PaintManager.h"
#include "DXCommon.h"
#include "Logger.h"
#include <cassert>

using namespace Microsoft::WRL;

namespace{
	// 何も塗られていない塗りテクスチャの一辺のピクセル数(全面が透明なので1ピクセルで足りる)
	constexpr uint32_t kEmptyCanvasSize = 1;
	// 塗りテクスチャの形式(PaintCanvas と合わせる)
	constexpr DXGI_FORMAT kCanvasFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
}

// インスタンス取得
PaintManager* PaintManager::GetInstance(){
	static PaintManager instance;
	return &instance;
}

// 初期化
void PaintManager::Initialize(DXCommon* dxCommon){
	assert(dxCommon);
	dxCommon_ = dxCommon;

	CreateRootSignature();
	CreatePipelineState();

	emptyCanvas_ = std::make_unique<PaintCanvas>();
	emptyCanvas_->Initialize(dxCommon_,kEmptyCanvasSize);
}

// 終了処理
void PaintManager::Finalize(){
	canvases_.clear();
	emptyCanvas_.reset();
	pipelineState_.Reset();
	rootSignature_.Reset();
}

// 塗りテクスチャへ描き込むルートシグネチャを生成する
void PaintManager::CreateRootSignature(){
	// インク・ワールド行列をまとめた定数バッファ(頂点シェーダーとピクセルシェーダーの両方で読む)
	D3D12_ROOT_PARAMETER rootParameters[1] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,D3D_ROOT_SIGNATURE_VERSION_1,&signatureBlob,&errorBlob);
	if(FAILED(hr)){
		Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	hr = dxCommon_->GetDevice()->CreateRootSignature(0,signatureBlob->GetBufferPointer(),signatureBlob->GetBufferSize(),IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

// 塗りテクスチャへ描き込むパイプラインを生成する
void PaintManager::CreatePipelineState(){
	ID3D12Device* device = dxCommon_->GetDevice();

	ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Paint/InkPaint.VS.hlsl",L"vs_6_0");
	assert(vertexShaderBlob != nullptr);
	ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Paint/InkPaint.PS.hlsl",L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// 頂点は Obj3D の描画と同じモデルの頂点バッファをそのまま使う
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// 色は新しいインクの色で置き換え、αは大きいほうを残す
	// αは輪郭で 0.5 になるなめらかな値なので、大きいほうを残すと重なったインクがつながった1つの形になる
	// 色をαで混ぜると、何も塗られていない(黒い)ピクセルと混ざって輪郭の外側の色が暗くなり、
	// 描画時に補間して読んだ輪郭が黒ずむため、インクが届いたピクセルの色はそのまま置き換える
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_MAX;

	// UV に広げた三角形は表裏が決まらないため、裏面も捨てない
	// 対応している GPU では保守的ラスタライズを使い、三角形に少しでもかかったピクセルをすべて塗る
	// (UV の面の境目のピクセルが塗り残され、描画時に面のつなぎ目が線になって見えるのを防ぐ)
	D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
	bool isConservativeSupported = SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,&options,sizeof(options))) &&
		options.ConservativeRasterizationTier != D3D12_CONSERVATIVE_RASTERIZATION_TIER_NOT_SUPPORTED;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.ConservativeRaster = isConservativeSupported?D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON:D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	// 塗りテクスチャには奥行きが無いため、深度は使わない
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
	pipelineStateDesc.pRootSignature = rootSignature_.Get();
	pipelineStateDesc.InputLayout = inputLayoutDesc;
	pipelineStateDesc.VS = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};
	pipelineStateDesc.PS = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()};
	pipelineStateDesc.BlendState = blendDesc;
	pipelineStateDesc.RasterizerState = rasterizerDesc;
	pipelineStateDesc.DepthStencilState = depthStencilDesc;
	pipelineStateDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	pipelineStateDesc.NumRenderTargets = 1;
	pipelineStateDesc.RTVFormats[0] = kCanvasFormat;
	pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateDesc.SampleDesc.Count = 1;
	pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	HRESULT hr = device->CreateGraphicsPipelineState(&pipelineStateDesc,IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}

// 名前に対応する塗りテクスチャを取得する
PaintCanvas* PaintManager::GetCanvas(const std::string& name,uint32_t size){
	auto it = canvases_.find(name);
	if(it != canvases_.end()){
		assert(it->second->GetSize() == size && "同じ名前の塗りテクスチャを違う大きさで取得しようとしている");
		return it->second.get();
	}

	auto canvas = std::make_unique<PaintCanvas>();
	canvas->Initialize(dxCommon_,size);
	PaintCanvas* result = canvas.get();
	canvases_.emplace(name,std::move(canvas));
	return result;
}
