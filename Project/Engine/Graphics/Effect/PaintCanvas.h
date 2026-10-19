#pragma once
#include "MyMath.h"
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <vector>

// 前方宣言
class DXCommon;
class Obj3D;

/// <summary>
/// インクで塗れる3Dオブジェクト1つ分の塗りテクスチャ
/// モデルの UV の位置にインクを描き込み、描画時にモデルの色へ重ねる(UV 空間でのペイント)。
/// 塗る位置はワールド座標の球(インク1滴)で指定するため、弾やしぶきが当たった三角形を探さなくても塗れる。
/// モデルの UV は面どうしで重ならないように展開されている必要がある。
///
/// テクスチャには、色(RGB)と、インクの輪郭でちょうど 0.5 になるなめらかな値(A)を書き込む。
/// 描画側で A を 0.5 でしきい値処理することで、テクスチャの解像度より細かくくっきりした輪郭にする。
/// 生成は PaintManager が行う(シーンを作り直すたびに RTV・SRV を確保して使い切らないよう、名前ごとに使い回す)。
/// </summary>
class PaintCanvas{
public:
	// 1回の描き込み(1フレーム)で塗れるインクの最大数(超えた分は次のフレームに回す)
	static constexpr uint32_t kMaxSplatsPerFrame = 32;

	// GPU に送るインク1滴分(シェーダーの InkSplat と同じ並び)
	struct InkSplat{
		Vector3 position; // 中心(ワールド座標)
		float radius;     // 半径
		Vector4 color;    // インクの色
		Vector3 normal;   // 塗る面の向き(周りに飛ぶ小さなしぶきをこの面に沿って並べる)
		float seed;       // 輪郭の形と周りのしぶきの並びを1滴ごとに変える乱数の種
	};

	/// <summary>
	/// 塗りテクスチャを生成して、何も塗られていない(透明な)状態にする
	/// </summary>
	/// <param name="dxCommon">DirectX 基盤</param>
	/// <param name="size">テクスチャの一辺のピクセル数</param>
	void Initialize(DXCommon* dxCommon,uint32_t size);

	/// <summary>
	/// インクを1滴塗る予約をする(実際に塗るのは Flush() のとき)
	/// </summary>
	/// <param name="position">インクの中心(ワールド座標)</param>
	/// <param name="radius">インクの半径</param>
	/// <param name="normal">塗る面の向き(正規化済み)</param>
	/// <param name="color">インクの色</param>
	void AddSplat(const Vector3& position,float radius,const Vector3& normal,const Vector4& color);

	// 塗ったインクをすべて消す(実際に消すのは Flush() のとき。予約中のインクも取り消す)
	void Clear();

	/// <summary>
	/// 予約したインクを塗りテクスチャに描き込む(1フレームに1回、描画より前に呼ぶ)
	/// 定数バッファを1つしか持たないため、同じフレームで2回呼ぶと先に積んだ描き込みの内容が上書きされる
	/// </summary>
	/// <param name="target">塗る3Dオブジェクト(Update() でワールド行列を求めておくこと)</param>
	void Flush(const Obj3D& target);

	// 描画時に読む塗りテクスチャの SRV
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGpu() const{ return srvHandleGpu_; }
	// テクスチャの一辺のピクセル数
	uint32_t GetSize() const{ return size_; }

private:
	// 1回の描き込みで GPU に送るデータ(シェーダーの PaintPass と同じ並び)
	struct PaintPassData{
		Matrix4x4 world;
		Matrix4x4 worldInverseTranspose;
		uint32_t splatCount;
		float padding[3];
		InkSplat splats[kMaxSplatsPerFrame];
	};

	// 塗りテクスチャの状態を切り替える(描き込むときはレンダーターゲット、描画で読むときはシェーダーリソース)
	void Transition(D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after);

	DXCommon* dxCommon_ = nullptr;

	// 塗りテクスチャ
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGpu_{};
	uint32_t size_ = 0;

	// 描き込み1回分の定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> passResource_;
	PaintPassData* passData_ = nullptr;

	// まだ描き込んでいないインク
	std::vector<InkSplat> pendingSplats_;
	// 次の Flush() でテクスチャを消すか
	bool isClearRequested_ = false;
	// 1滴ごとに輪郭の形を変えるため、塗った数を数えて乱数の種にする
	uint32_t splatSerial_ = 0;
};
