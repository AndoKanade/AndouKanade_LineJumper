#include "WinAPI.h"
#include "imgui_impl_win32.h"

#ifdef USE_IMGUI
// ImGuiのWindows用ウィンドウプロシージャハンドラ
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lPalam);
#endif

// ウィンドウメッセージ処理のコールバック関数
LRESULT CALLBACK WinAPI::WindowProc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam){
#ifdef USE_IMGUI
	// ImGuiがイベントを消費した場合は処理を終了
	if(ImGui_ImplWin32_WndProcHandler(hwnd,msg,wparam,lparam)){
		return true;
	}
#endif

	switch(msg){
	case WM_DESTROY:
		// ウィンドウが破棄されたら終了メッセージを送る
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd,msg,wparam,lparam);
}

// ウィンドウの初期化処理
void WinAPI::Initialize(const wchar_t* title,int32_t width,int32_t height){
	// COMライブラリの初期化
	HRESULT hr = CoInitializeEx(0,COINIT_MULTITHREADED);

	// ウィンドウクラスの設定
	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = L"Andou_Kanade";
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr,IDC_ARROW);

	// ウィンドウクラスの登録
	RegisterClass(&wc);

	// ウィンドウサイズを調整（クライアント領域を基準にする）
	RECT wrc = {0, 0, width, height};
	AdjustWindowRect(&wrc,WS_OVERLAPPEDWINDOW,false);

	// ウィンドウの生成
	hwnd = CreateWindow(
		wc.lpszClassName,
		title, // ウィンドウタイトル
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);

	// ウィンドウの表示
	ShowWindow(hwnd,SW_SHOW);
}

// 更新処理（現在は空）
void WinAPI::Update(){}

// メッセージの取得とディスパッチ
bool WinAPI::ProcessMessage(){
	MSG msg{};
	bool isQuitRequested = false;

	// たまっているメッセージを、このフレームのうちにすべて処理する
	// 1フレームに1つずつだと、キーを押し続けたりマウスを動かしたりしてメッセージが多く来たときに処理が追いつかず、
	// メッセージから入力を受け取っているImGuiの反応が遅れる(DirectInputで読んでいるカメラ操作だけは遅れないため、ずれて見える)
	while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){
		// 終了メッセージが来たら、終了リクエストとして呼び出し側へ伝える
		if(msg.message == WM_QUIT){
			isQuitRequested = true;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return isQuitRequested;
}

// 終了処理
void WinAPI::Finalize(){
	CloseWindow(hwnd);
	CoUninitialize();
}