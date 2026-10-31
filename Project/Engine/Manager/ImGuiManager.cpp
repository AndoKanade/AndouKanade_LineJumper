#include "ImGuiManager.h"

// プロジェクト内ヘッダー
#include "WinAPI.h"
#include "DXCommon.h"
#include "SrvManager.h"
#include "EditorContext.h"

// ImGuiライブラリ
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
// imgui.ini に独自の項目を読み書きする仕組み(ImGuiSettingsHandler)を使う
#include <imgui_internal.h>

// 安全確認用
#include <cassert>
#include <cstring>
// フォントのファイルがあるかを調べる
#include <filesystem>

#ifdef USE_IMGUI
namespace{
	// ImGui の標準フォントに無い日本語の文字を描くフォント(操作説明の画像と同じフォント)
	constexpr const char* kJapaneseFontPath = "resource/fonts/MochiyPopPOne-Regular.ttf";

	// imgui.ini に書き出すエディターの状態の、種類名と項目名
	// 下段の選択パネルで選んだエディターを覚えておき、次に起動したときも同じエディターを開く
	constexpr const char* kEditorSettingsTypeName = "EditorContext";
	constexpr const char* kActiveToolPanelKey = "ActiveToolPanel=";

	// imgui.ini の [EditorContext][...] の見出しを読んだとき(nullptr 以外を返すと、続く行が EditorSettingsReadLine に渡される)
	void* EditorSettingsReadOpen(ImGuiContext*,ImGuiSettingsHandler*,const char*){
		return EditorContext::GetInstance();
	}

	// 見出しに続く1行を読んで、選んでいたエディターを戻す
	void EditorSettingsReadLine(ImGuiContext*,ImGuiSettingsHandler*,void*,const char* line){
		const size_t keyLength = std::strlen(kActiveToolPanelKey);
		if(std::strncmp(line,kActiveToolPanelKey,keyLength) == 0){
			EditorContext::GetInstance()->SetActiveToolPanel(line + keyLength);
		}
	}

	// imgui.ini を書き出すときに、選んでいるエディターを書き込む
	void EditorSettingsWriteAll(ImGuiContext*,ImGuiSettingsHandler* handler,ImGuiTextBuffer* buffer){
		buffer->appendf("[%s][State]\n",handler->TypeName);
		buffer->appendf("%s%s\n\n",kActiveToolPanelKey,EditorContext::GetInstance()->GetActiveToolPanel().c_str());
	}
}
#endif

// ==========================================================================
// シングルトンインスタンスの取得
// ==========================================================================
ImGuiManager* ImGuiManager::GetInstance(){
	static ImGuiManager instance;
	return &instance;
}

// ==========================================================================
// 初期化処理
// ==========================================================================
void ImGuiManager::Initialize([[maybe_unused]] WinAPI* winApi,[[maybe_unused]] DXCommon* dxCommon){
#ifdef USE_IMGUI

	// メンバ変数の保存
	dxCommon_ = dxCommon;

	// --- 依存オブジェクトのNullチェック (安全対策) ---
	assert(winApi && "WinAPI instance is null.");
	assert(dxCommon && "DXCommon instance is null.");
	assert(dxCommon->GetCommandQueue() && "CommandQueue is null. Check DXCommon initialization.");

	// 1. ImGuiコンテキストの作成
	ImGui::CreateContext();

	// 下段の選択パネルで選んだエディターを imgui.ini に読み書きする(imgui.ini は最初の NewFrame で読まれるため、その前に登録する)
	ImGuiSettingsHandler editorSettingsHandler;
	editorSettingsHandler.TypeName = kEditorSettingsTypeName;
	editorSettingsHandler.TypeHash = ImHashStr(kEditorSettingsTypeName);
	editorSettingsHandler.ReadOpenFn = EditorSettingsReadOpen;
	editorSettingsHandler.ReadLineFn = EditorSettingsReadLine;
	editorSettingsHandler.WriteAllFn = EditorSettingsWriteAll;
	ImGui::AddSettingsHandler(&editorSettingsHandler);

	// 2. スタイルの設定
	ImGui::StyleColorsDark();

	// 2-2. フォントの設定
	// 英数字は今までどおり ImGui の標準フォントで描き、標準フォントに無い日本語の文字だけを日本語フォントから足す(MergeMode)
	// 大きさは標準フォントにそろえる(0 を渡すと標準フォントと同じ大きさになる)ので、パネルの並びや幅は変わらない
	// ImGui 1.92 からは、使った文字だけをその場でフォントのテクスチャに読み込むため、読み込む文字の範囲(GlyphRanges)を指定しなくてよい
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontDefault();
	// フォントのファイルが無いときは、日本語は「?」のままにして起動は続ける
	if(std::filesystem::exists(kJapaneseFontPath)){
		ImFontConfig japaneseFontConfig;
		japaneseFontConfig.MergeMode = true;
		io.Fonts->AddFontFromFileTTF(kJapaneseFontPath,0.0f,&japaneseFontConfig);
	}

	// 3. Win32初期化
	ImGui_ImplWin32_Init(winApi->GetHwnd());

	// 4. SRVヒープ領域の確保 (フォントテクスチャ用)
	SrvManager* srvManager = SrvManager::GetInstance();
	assert(srvManager && "SrvManager instance is null.");

	for(uint32_t& fontSrvIndex : fontSrvIndices_){
		fontSrvIndex = srvManager->Allocate();
	}
	srvHeap_ = srvManager->GetDescriptorHeap();

	// 5. DirectX12初期化設定
	ImGui_ImplDX12_InitInfo initInfo = {};

	initInfo.Device = dxCommon_->GetDevice();
	initInfo.CommandQueue = dxCommon_->GetCommandQueue();
	initInfo.NumFramesInFlight = static_cast<int>(dxCommon_->GetSwapChainResourcesNum());

	// エラー回避のためのフォーマット設定 (SRGB)
	initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN; // 深度バッファは使用しない

	// ヒープ設定 (SrvManagerのものを使用)
	initInfo.SrvDescriptorHeap = srvHeap_;

	// フォントのテクスチャを作るとき・消すときに、SRV を貸し出す・返してもらう
	// SRV を1つだけ渡す古い設定(LegacySingleSrv)では、フォントのテクスチャを作り直すときに2つ目が要って assert で止まるため、
	// 最初に確保しておいた kFontSrvCount 個の中から、空いているものを貸し出す
	initInfo.UserData = this;
	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info,D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle){
		ImGuiManager* manager = static_cast<ImGuiManager*>(info->UserData);
		for(size_t i = 0; i < kFontSrvCount; ++i){
			if(!manager->isFontSrvInUse_[i]){
				manager->isFontSrvInUse_[i] = true;
				*outCpuHandle = SrvManager::GetInstance()->GetCPUDescriptorHandle(manager->fontSrvIndices_[i]);
				*outGpuHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(manager->fontSrvIndices_[i]);
				return;
			}
		}
		assert(false && "ImGui のフォント用の SRV が足りません。kFontSrvCount を増やしてください。");
	};
	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info,D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,D3D12_GPU_DESCRIPTOR_HANDLE){
		ImGuiManager* manager = static_cast<ImGuiManager*>(info->UserData);
		for(size_t i = 0; i < kFontSrvCount; ++i){
			if(SrvManager::GetInstance()->GetCPUDescriptorHandle(manager->fontSrvIndices_[i]).ptr == cpuHandle.ptr){
				manager->isFontSrvInUse_[i] = false;
				return;
			}
		}
	};

	// 初期化実行
	ImGui_ImplDX12_Init(&initInfo);

#endif
}

// ==========================================================================
// フレーム開始処理
// ==========================================================================
void ImGuiManager::Begin(){
#ifdef USE_IMGUI
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif
}

// ==========================================================================
// フレーム終了処理 (内部コマンド生成)
// ==========================================================================
void ImGuiManager::End(){
#ifdef USE_IMGUI
	ImGui::Render();
#endif
}

// ==========================================================================
// 描画処理 (GPUコマンド発行)
// ==========================================================================
void ImGuiManager::Draw(){
#ifdef USE_IMGUI

	// コマンドリスト取得
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	// ディスクリプタヒープのセット
	// ※SrvManager管理の共通ヒープを使用する (ゲーム内テクスチャと共存させるため)
	ID3D12DescriptorHeap* ppHeaps[] = {SrvManager::GetInstance()->GetDescriptorHeap()};
	commandList->SetDescriptorHeaps(_countof(ppHeaps),ppHeaps);

	// 描画コマンド発行
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),commandList);

#endif
}

// ==========================================================================
// 終了処理
// ==========================================================================
void ImGuiManager::Finalize(){
#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
}