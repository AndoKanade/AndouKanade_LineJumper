#include "TimeManager.h"
#include <Windows.h>
#include <timeapi.h>
#include <thread>

// timeBeginPeriod()を使うためのライブラリ
#pragma comment(lib,"winmm.lib")

namespace{
	// sleep_forで待つときのタイマーの精度(ミリ秒)
	constexpr UINT kTimerResolutionMs = 1;
}

TimeManager* TimeManager::GetInstance(){
    static TimeManager instance;
    return &instance;
}

void TimeManager::Initialize(){
    // Windowsのタイマーの精度は初期状態で約15.6ミリ秒のため、少しだけ待つつもりでも最大15.6ミリ秒ほど眠ってしまう
    // そうなると次のフレームが画面の更新(垂直同期)に間に合わず、ときどき1フレーム分止まって見えるため、精度を1ミリ秒に上げる
    // (アプリが終了するとWindowsが元の精度に戻す)
    timeBeginPeriod(kTimerResolutionMs);

    reference_ = std::chrono::steady_clock::now();
}

void TimeManager::Update(){
    const std::chrono::microseconds kMinTime(16666); // 1/60sec
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

    if(elapsed < kMinTime){
        std::this_thread::sleep_for(kMinTime - elapsed);
    }
    reference_ = std::chrono::steady_clock::now();
}