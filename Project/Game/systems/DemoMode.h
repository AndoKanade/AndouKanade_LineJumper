#pragma once

/// <summary>
/// 評価会などで、人がいなくてもゲームオーバーまでの演出を流し続けるデモ
/// 提出・掲示する Release 版では、起動したらすぐデモになる(Debug・Development 版はエディターで作業するため、今までどおり遊べる)。
/// 残機2でレールの始点から始まり、決まった間隔で被弾して体力が尽きる → リトライ → もう一度尽きる →
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
	// 被弾させる間隔(秒)。被弾後の無敵時間(1秒)より長くして、毎回確実に体力が減るようにする
	// 体力3が尽きるまで約4.5秒で、最初のレール(走り切るまで約8秒)の終点で落ちる前に尽きる
	static constexpr float kHitInterval = 1.5f;
	// ゲームオーバー画面を見せる時間(秒)。過ぎたら最初からやり直す
	static constexpr float kGameOverDisplayDuration = 4.0f;
};
