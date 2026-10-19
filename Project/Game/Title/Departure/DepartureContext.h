#pragma once

// 前方宣言
class Player;
class RailEditor;
class StartPlatform;
class InkEffectManager;
class ScreenImpact;
class SceneManager;
class Camera;

/// <summary>
/// 出発の演出が動かす・参照するもの(すべて GameScene が持ち、演出は借りて使うだけ)
/// 新しい演出で必要なものが増えたら、ここに足す
/// </summary>
struct DepartureContext{
	// レールへ乗せ、演出中の表示を上書きするプレイヤー
	Player* player = nullptr;
	// 乗せる先のレール(始点の位置と向きを使う)
	const RailEditor* railEditor = nullptr;
	// 出発台の位置を持ち、インクを塗れるスタート台
	StartPlatform* startPlatform = nullptr;
	// インクのしぶき
	InkEffectManager* inkEffect = nullptr;
	// ヒットストップ・画面の揺れ
	ScreenImpact* screenImpact = nullptr;
	// 画面をインクで覆う切り替えの演出
	SceneManager* sceneManager = nullptr;
	// プレイ用のカメラ(視野角を変える・画面の座標を求めるのに使う)
	Camera* camera = nullptr;
};
