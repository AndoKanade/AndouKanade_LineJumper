#pragma once

/// <summary>
/// 評価会などで、人がいなくてもゲームオーバーまでの演出を流し続けるデモ
/// 提出・掲示する Release 版では、起動したらすぐデモになる(Debug・Development 版はエディターで作業するため、今までどおり遊べる)。
/// 残機2でレールの始点から始まり、敵の弾に当たって体力が尽きる → リトライ → レールの終点から落ちる →
/// やられ演出 → ゲームオーバー画面 → 最初に戻る、をくり返す。
/// </summary>
struct DemoMode{
	// デモを流しているか(ImGui が無い Release 版だけ流す)
#ifdef USE_IMGUI
	static constexpr bool isEnabled = false;
#else
	static constexpr bool isEnabled = true;
#endif

	// デモを始めるときの残機
	static constexpr int kStartLives = 2;
	// ゲームオーバー画面を見せる時間(秒)。過ぎたら最初からやり直す
	static constexpr float kGameOverDisplayDuration = 4.0f;
};
