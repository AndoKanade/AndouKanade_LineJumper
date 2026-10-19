#pragma once
#include "PaintCanvas.h"
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <string>
#include <unordered_map>

// 前方宣言
class DXCommon;

/// <summary>
/// インクの塗り(UV 空間でのペイント)の共通部分
/// 塗りテクスチャへ描き込むパイプラインを持ち、塗りテクスチャ(PaintCanvas)を名前ごとに1つ作って使い回す。
/// RTV・SRV は確保したら解放しない作りのため、シーンを作り直すたびに新しく作らず、同じ名前のものを返す。
/// 塗りテクスチャを持たない3Dオブジェクトには、何も塗られていない1ピクセルのテクスチャを渡して同じシェーダーで描く。
/// </summary>
class PaintManager{
public:
	// インスタンス取得
	static PaintManager* GetInstance();

	// 初期化(描き込み用のパイプラインと、何も塗られていないテクスチャの生成)
	void Initialize(DXCommon* dxCommon);

	// 終了処理
	void Finalize();

	/// <summary>
	/// 名前に対応する塗りテクスチャを取得する(初めての名前なら生成する)
	/// 前に塗ったインクは残っているため、塗り直すときは PaintCanvas::Clear() を呼ぶ
	/// </summary>
	/// <param name="name">塗りテクスチャの名前(塗る3Dオブジェクトごとに決める)</param>
	/// <param name="size">テクスチャの一辺のピクセル数(同じ名前では同じ値にする)</param>
	PaintCanvas* GetCanvas(const std::string& name,uint32_t size);

	// 何も塗られていない塗りテクスチャの SRV(塗りテクスチャを持たない3Dオブジェクトの描画に使う)
	D3D12_GPU_DESCRIPTOR_HANDLE GetEmptyCanvasSrvHandleGpu() const{ return emptyCanvas_->GetSrvHandleGpu(); }

	// 塗りテクスチャへ描き込むルートシグネチャ・パイプライン
	ID3D12RootSignature* GetRootSignature() const{ return rootSignature_.Get(); }
	ID3D12PipelineState* GetPipelineState() const{ return pipelineState_.Get(); }

private:
	PaintManager() = default;
	~PaintManager() = default;
	PaintManager(const PaintManager&) = delete;
	PaintManager& operator=(const PaintManager&) = delete;

	// 塗りテクスチャへ描き込むルートシグネチャ(定数バッファ1つだけ)を生成する
	void CreateRootSignature();
	// 塗りテクスチャへ描き込むパイプラインを生成する
	void CreatePipelineState();

	DXCommon* dxCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

	// 名前ごとの塗りテクスチャ
	std::unordered_map<std::string,std::unique_ptr<PaintCanvas>> canvases_;
	// 何も塗られていない塗りテクスチャ
	std::unique_ptr<PaintCanvas> emptyCanvas_;
};
