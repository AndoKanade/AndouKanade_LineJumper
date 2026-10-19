#include "PaintCanvas.h"
#include "PaintManager.h"
#include "DXCommon.h"
#include "SrvManager.h"
#include "Obj3D.h"
#include "Model.h"
#include <algorithm>
#include <cassert>

namespace{
	// 塗りテクスチャの形式(色と、輪郭で 0.5 になる値を 0〜1 で持つ)
	constexpr DXGI_FORMAT kCanvasFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	// 何も塗られていない状態(色もαも0)。生成時の最適化クリア値と、Clear() で消すときの色をそろえる
	constexpr float kEmptyColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
	// 定数バッファは 256 バイト単位で確保する
	constexpr size_t kConstantBufferAlignment = 0xff;
	// 乱数の種を小さい値の範囲で回す(大きな値になってシェーダーの計算の精度が落ちないようにする)
	constexpr uint32_t kSeedPeriod = 1024;
}

// 塗りテクスチャを生成して、何も塗られていない状態にする
void PaintCanvas::Initialize(DXCommon* dxCommon,uint32_t size){
	assert(dxCommon);
	dxCommon_ = dxCommon;
	size_ = size;
	ID3D12Device* device = dxCommon_->GetDevice();

	// レンダーターゲットとして描き込み、シェーダーリソースとして読むテクスチャ
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = size_;
	resourceDesc.Height = size_;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = kCanvasFormat;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	// 消すときの色と最適化クリア値を一致させる(一致しないとデバッグレイヤーが警告を出す)
	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = kCanvasFormat;
	std::copy(std::begin(kEmptyColor),std::end(kEmptyColor),clearValue.Color);

	// 生成直後に消すため、レンダーターゲットの状態で作る
	HRESULT hr = device->CreateCommittedResource(&heapProperties,D3D12_HEAP_FLAG_NONE,&resourceDesc,D3D12_RESOURCE_STATE_RENDER_TARGET,&clearValue,IID_PPV_ARGS(&resource_));
	assert(SUCCEEDED(hr));

	// 描き込み用の RTV
	rtvHandle_ = dxCommon_->AllocateRtvDescriptor();
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = kCanvasFormat;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	device->CreateRenderTargetView(resource_.Get(),&rtvDesc,rtvHandle_);

	// 描画時に読む SRV
	SrvManager* srvManager = SrvManager::GetInstance();
	assert(srvManager->CanAllocate());
	uint32_t srvIndex = srvManager->Allocate();
	srvManager->CreateSRVTexture(srvIndex,resource_.Get(),kCanvasFormat,1);
	srvHandleGpu_ = srvManager->GetGPUDescriptorHandle(srvIndex);

	// 描き込み1回分の定数バッファ(毎フレーム書き換えるため、開いたままにしておく)
	size_t passSize = (sizeof(PaintPassData) + kConstantBufferAlignment) & ~kConstantBufferAlignment;
	passResource_ = dxCommon_->CreateBufferResource(passSize);
	passResource_->Map(0,nullptr,reinterpret_cast<void**>(&passData_));

	pendingSplats_.reserve(kMaxSplatsPerFrame);

	// 中身は不定のため、最初に消してから描画で読める状態にしておく
	dxCommon_->GetCommandList()->ClearRenderTargetView(rtvHandle_,kEmptyColor,0,nullptr);
	Transition(D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

// インクを1滴塗る予約をする
void PaintCanvas::AddSplat(const Vector3& position,float radius,const Vector3& normal,const Vector4& color){
	pendingSplats_.push_back({position, radius, color, normal, static_cast<float>(splatSerial_)});
	splatSerial_ = (splatSerial_ + 1) % kSeedPeriod;
}

// 塗ったインクをすべて消す
void PaintCanvas::Clear(){
	pendingSplats_.clear();
	isClearRequested_ = true;
}

// 予約したインクを塗りテクスチャに描き込む
void PaintCanvas::Flush(const Obj3D& target){
	// 描き込むものが無いフレームは、状態の切り替えもしない
	if(!isClearRequested_ && pendingSplats_.empty()){
		return;
	}
	Model* model = target.GetModel();
	if(!model){
		return;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);

	if(isClearRequested_){
		commandList->ClearRenderTargetView(rtvHandle_,kEmptyColor,0,nullptr);
		isClearRequested_ = false;
	}

	if(!pendingSplats_.empty()){
		// 最大数を超えた分は次のフレームに回す
		const uint32_t splatCount = (std::min)(static_cast<uint32_t>(pendingSplats_.size()),kMaxSplatsPerFrame);
		const Matrix4x4& world = target.GetWorldMatrix();
		passData_->world = world;
		passData_->worldInverseTranspose = Transpose(Inverse(world));
		passData_->splatCount = splatCount;
		std::copy_n(pendingSplats_.begin(),splatCount,passData_->splats);
		pendingSplats_.erase(pendingSplats_.begin(),pendingSplats_.begin() + splatCount);

		// テクスチャ全体を描き込み先にする(描画のビューポート・シザーは DXCommon::PreDraw で設定し直される)
		D3D12_VIEWPORT viewport{};
		viewport.Width = static_cast<float>(size_);
		viewport.Height = static_cast<float>(size_);
		viewport.MaxDepth = 1.0f;
		D3D12_RECT scissorRect{0, 0, static_cast<LONG>(size_), static_cast<LONG>(size_)};

		PaintManager* paintManager = PaintManager::GetInstance();
		commandList->OMSetRenderTargets(1,&rtvHandle_,FALSE,nullptr);
		commandList->RSSetViewports(1,&viewport);
		commandList->RSSetScissorRects(1,&scissorRect);
		commandList->SetGraphicsRootSignature(paintManager->GetRootSignature());
		commandList->SetPipelineState(paintManager->GetPipelineState());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->SetGraphicsRootConstantBufferView(0,passResource_->GetGPUVirtualAddress());

		// モデルの三角形を UV の位置に広げて描く(頂点・インデックスの設定と描画だけを行い、引数のスカイボックス・カメラは使わない)
		model->Draw(0,0);
	}

	Transition(D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

// 塗りテクスチャの状態を切り替える
void PaintCanvas::Transition(D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource_.Get();
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	dxCommon_->GetCommandList()->ResourceBarrier(1,&barrier);
}
