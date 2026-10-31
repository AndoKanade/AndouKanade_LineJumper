#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <d3d12.h>

// 前方宣言
// (ヘッダー内でインクルードする代わりに、クラスがあることだけを伝える)
class WinAPI;
class DXCommon;

class ImGuiManager{
public: // --- 公開メソッド ---

	// シングルトンインスタンス取得
	static ImGuiManager* GetInstance();

	// 初期化・終了
	void Initialize(WinAPI* winApi,DXCommon* dxCommon);
	void Finalize();

	// フレーム処理
	void Begin();
	void End();
	void Draw();

private: // --- 内部変数・メソッド ---

	// コンストラクタ隠蔽 (シングルトンパターン)
	ImGuiManager() = default;
	~ImGuiManager() = default;
	ImGuiManager(const ImGuiManager&) = delete;
	ImGuiManager& operator=(const ImGuiManager&) = delete;

	// 保持するポインタ
	DXCommon* dxCommon_ = nullptr;

	// SrvManagerから借りてくるフォント用ヒープ (解放責任なし)
	ID3D12DescriptorHeap* srvHeap_ = nullptr;

	// フォントのテクスチャに使う SRV の数
	// 日本語の文字は使うときにフォントのテクスチャへ読み込むため、文字が増えるとテクスチャが大きく作り直される
	// 作り直した後も、描画中のフレームが使い終わるまで古いテクスチャが残るため、1つでは足りない
	static constexpr size_t kFontSrvCount = 8;

	// フォントのテクスチャに使う SrvManager 上のインデックスと、それぞれを今使っているか
	std::array<uint32_t,kFontSrvCount> fontSrvIndices_ = {};
	std::array<bool,kFontSrvCount> isFontSrvInUse_ = {};
};