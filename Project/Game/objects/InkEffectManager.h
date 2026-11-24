#pragma once

#include "MyMath.h"
#include "MeshParticleSystem.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <vector>

// 前方宣言
class Obj3dCommon;

/// <summary>
/// インクのしぶき・的の破片・撃破時の閃光の演出
/// 出し方(数・速さ・ばらつき)をここで決め、動かすのと描くのは GPU のパーティクル(MeshParticleSystem)で行う。
/// 種類ごとに決まった数だけ使い回し、空きが無いときは一番古いものを上書きする。
/// インクのしぶきは、着地を調べる処理(SetLandingHandler)を渡すと、面に着いたところにインクを塗って消える。
/// 種類ごとのモデルは InkEffectManager.cpp の先頭の表で決めている(配布モデルから差し替えるときはそこを書き換える)。
/// </summary>
class InkEffectManager{
public:
	/// <summary>
	/// しぶきの着地を調べる処理の型
	/// 移動前の座標・移動後の座標・塗るインクの半径を受け取り、面に着いてインクを塗ったら true を返す
	/// </summary>
	using LandingHandler = std::function<bool(const Vector3& from,const Vector3& to,float inkRadius)>;

	InkEffectManager();
	~InkEffectManager();

	// 初期化処理(モデルの読み込みと、GPU のパーティクルの生成)
	void Initialize(Obj3dCommon* objCommon);

	// 出ている演出をすべて消す
	void Reset();

	/// <summary>
	/// しぶきの着地を調べる処理を設定する(設定しなければ、しぶきは何にも着かずに寿命まで飛ぶ)
	/// しぶきは GPU で動かしているため、surfaceHeight の高さを上から下へ横切ったしぶきだけを handler で調べる
	/// (調べられるのは水平な面1枚だけ。横切ったフレームの次のフレームに調べる)
	/// </summary>
	/// <param name="surfaceHeight">着地を調べる面の高さ</param>
	/// <param name="handler">着地を調べる処理</param>
	void SetLandingHandler(float surfaceHeight,LandingHandler handler);

	/// <summary>
	/// 的を壊したときの演出(破片・インクのしぶき・閃光)
	/// </summary>
	/// <param name="position">壊れた的の座標</param>
	void EmitTargetBreak(const Vector3& position);

	/// <summary>
	/// 指定した向きへ飛び散るインクのしぶき(弾を撃ったときの手元など)
	/// </summary>
	/// <param name="position">しぶきが出る座標</param>
	/// <param name="direction">飛び散る向き(正規化済み)</param>
	/// <param name="count">しぶきの数</param>
	void EmitInkSplash(const Vector3& position,const Vector3& direction,int count);

	/// <summary>
	/// レールを進んでいる間、足元から後ろへ飛ぶインクのしぶき(1回の呼び出しで1粒)
	/// </summary>
	/// <param name="position">足元の座標</param>
	/// <param name="backward">進行方向の逆向き(正規化済み)</param>
	void EmitRailTrail(const Vector3& position,const Vector3& backward);

	/// <summary>
	/// 出発の合図(足元やゲート)から、周り一面へ高く弧を描いて飛び散るインクのしぶき
	/// 長く飛ばして落ちてくるようにし、着地を調べる処理を設定していれば落ちたところにインクを塗る
	/// </summary>
	/// <param name="position">しぶきが出る座標(足元)</param>
	/// <param name="count">しぶきの数</param>
	void EmitLaunchBurst(const Vector3& position,int count);

	/// <summary>
	/// 最後のミスで自機が敵のインク(ピンク)になって弾け飛ぶしぶきと閃光(やられ演出)
	/// 全方向へ大きく飛び散らせる。しぶきは何かに着いても塗らない(スタート台をピンクに塗らないようにする)
	/// </summary>
	/// <param name="position">弾ける中心の座標(自機の体の中心)</param>
	void EmitDefeatBurst(const Vector3& position);

	/// <summary>
	/// ゴールの的を壊したときの演出(普通の的より大きな破片・多くのしぶき・大きな閃光)
	/// ステージクリアの瞬間なので、普通の的よりはっきり派手にする
	/// </summary>
	/// <param name="position">壊れたゴールの的の座標</param>
	void EmitGoalBreak(const Vector3& position);

	// 移動・回転・縮小・寿命の更新
	void Update(float deltaTime);

	// 出ている演出を描画する
	void Draw();

private:
	// 演出1つ分の種類(動き方と見た目が変わる。MeshParticleSystem に渡す種類の番号と同じ並び)
	enum class Kind : uint32_t{
		Droplet, // インクのしぶき(重力で落ち、飛ぶ向きに伸びる)
		Shard,   // 的の破片(重力で落ち、回転する)
		Flash,   // 撃破時の閃光(その場で広がって消える)
	};
	// 種類の数
	static constexpr size_t kKindCount = 3;

	// 1つ出す(一番古いものから順に上書きする)
	void Spawn(Kind kind,const MeshParticleSystem::SpawnDesc& desc);

	// 指定した範囲の乱数を返す
	float RandomRange(float min,float max);

	// 各軸が -1〜1 の乱数のベクトルを返す
	Vector3 RandomVector();

	// しぶき・破片・閃光を動かして描く GPU のパーティクル
	std::unique_ptr<MeshParticleSystem> particles_;

	// 飛び散り方のばらつきに使う乱数
	std::mt19937 randomEngine_;

	// しぶきの着地を調べる処理
	LandingHandler landingHandler_;

	// 使い回す個数
	static constexpr uint32_t kDropletCount = 160;
	static constexpr uint32_t kShardCount = 48;
	static constexpr uint32_t kFlashCount = 8;

	// しぶき・破片に掛ける重力加速度(1秒あたりの下方向への速度変化量)
	static constexpr float kDropletGravity = 12.0f;
	static constexpr float kShardGravity = 14.0f;
	// しぶきを飛ぶ向きへ伸ばす割合(速さ1あたり)と、その上限
	static constexpr float kDropletStretchPerSpeed = 0.25f;
	static constexpr float kDropletMaxStretch = 3.0f;
	// 縮み始めるまでの寿命の割合(これを過ぎると小さくなりながら消える)
	static constexpr float kShrinkStartRate = 0.5f;
	// 閃光が最大まで広がったときの大きさ(開始時の何倍か)
	static constexpr float kFlashExpandScale = 4.0f;
	// 速さがこれ未満のときは向きが定まらないため、しぶきを伸ばさない
	static constexpr float kMinStretchSpeed = 0.001f;

	// 的を壊したときの破片の数・速さ・大きさ・寿命
	static constexpr int kBreakShardCount = 10;
	static constexpr float kBreakShardSpeedMin = 3.0f;
	static constexpr float kBreakShardSpeedMax = 7.0f;
	static constexpr float kBreakShardUpSpeed = 3.0f;
	static constexpr float kBreakShardScaleMin = 0.08f;
	static constexpr float kBreakShardScaleMax = 0.16f;
	static constexpr float kBreakShardLifeMin = 0.6f;
	static constexpr float kBreakShardLifeMax = 0.9f;
	static constexpr float kBreakShardSpinMax = 15.0f;
	// 的を壊したときのしぶきの数・速さ・大きさ・寿命
	static constexpr int kBreakDropletCount = 14;
	static constexpr float kBreakDropletSpeedMin = 2.0f;
	static constexpr float kBreakDropletSpeedMax = 6.0f;
	static constexpr float kBreakDropletUpSpeed = 2.0f;
	static constexpr float kBreakDropletScaleMin = 0.06f;
	static constexpr float kBreakDropletScaleMax = 0.12f;
	static constexpr float kBreakDropletLifeMin = 0.4f;
	static constexpr float kBreakDropletLifeMax = 0.7f;
	// 閃光の大きさ・寿命
	static constexpr float kFlashScale = 0.15f;
	static constexpr float kFlashLife = 0.12f;
	// 撃ったときなどのしぶきの速さ・横へのばらつき・大きさ・寿命
	static constexpr float kSplashSpeedMin = 3.0f;
	static constexpr float kSplashSpeedMax = 6.0f;
	static constexpr float kSplashSpread = 1.5f;
	static constexpr float kSplashScaleMin = 0.02f;
	static constexpr float kSplashScaleMax = 0.04f;
	static constexpr float kSplashLifeMin = 0.2f;
	static constexpr float kSplashLifeMax = 0.35f;
	// レール走行中のしぶきの後ろ・上への速さ・ばらつき・大きさ・寿命
	static constexpr float kTrailBackSpeed = 3.0f;
	static constexpr float kTrailUpSpeed = 1.5f;
	static constexpr float kTrailSpread = 1.5f;
	static constexpr float kTrailScaleMin = 0.025f;
	static constexpr float kTrailScaleMax = 0.05f;
	static constexpr float kTrailLifeMin = 0.25f;
	static constexpr float kTrailLifeMax = 0.4f;
	// 出発の合図のしぶきの横・上への速さ・大きさ・寿命
	// 上への速さと重力で決まる滞空時間(およそ 0.7〜1.1 秒)より寿命を長くし、落ちて着地するまで消えないようにする
	static constexpr float kBurstHorizontalSpeedMin = 1.0f;
	static constexpr float kBurstHorizontalSpeedMax = 3.0f;
	static constexpr float kBurstUpSpeedMin = 4.0f;
	static constexpr float kBurstUpSpeedMax = 6.5f;
	static constexpr float kBurstScaleMin = 0.035f;
	static constexpr float kBurstScaleMax = 0.065f;
	static constexpr float kBurstLifeMin = 1.8f;
	static constexpr float kBurstLifeMax = 2.2f;
	// やられ演出のしぶきの数・速さ・上への速さ・大きさ・寿命と、閃光の大きさ・寿命
	// 普段のしぶきより大きく速くして、自機が弾けたことがはっきり分かるようにする
	static constexpr int kDefeatDropletCount = 48;
	static constexpr float kDefeatDropletSpeedMin = 3.0f;
	static constexpr float kDefeatDropletSpeedMax = 9.0f;
	static constexpr float kDefeatDropletUpSpeed = 3.0f;
	static constexpr float kDefeatDropletScaleMin = 0.06f;
	static constexpr float kDefeatDropletScaleMax = 0.14f;
	static constexpr float kDefeatDropletLifeMin = 0.8f;
	static constexpr float kDefeatDropletLifeMax = 1.4f;
	static constexpr float kDefeatFlashScale = 0.35f;
	static constexpr float kDefeatFlashLife = 0.25f;
	// ゴールの的を壊したときの破片の数・速さ・上への速さ・大きさ・寿命
	// ゴールの的は普通の的の2.5倍の大きさなので、破片も大きく、遠くまで長く飛ばす
	static constexpr int kGoalBreakShardCount = 24;
	static constexpr float kGoalBreakShardSpeedMin = 5.0f;
	static constexpr float kGoalBreakShardSpeedMax = 12.0f;
	static constexpr float kGoalBreakShardUpSpeed = 4.0f;
	static constexpr float kGoalBreakShardScaleMin = 0.2f;
	static constexpr float kGoalBreakShardScaleMax = 0.4f;
	static constexpr float kGoalBreakShardLifeMin = 1.0f;
	static constexpr float kGoalBreakShardLifeMax = 1.5f;
	// ゴールの的を壊したときのしぶきの数・速さ・上への速さ・大きさ・寿命
	static constexpr int kGoalBreakDropletCount = 40;
	static constexpr float kGoalBreakDropletSpeedMin = 4.0f;
	static constexpr float kGoalBreakDropletSpeedMax = 10.0f;
	static constexpr float kGoalBreakDropletUpSpeed = 3.0f;
	static constexpr float kGoalBreakDropletScaleMin = 0.1f;
	static constexpr float kGoalBreakDropletScaleMax = 0.2f;
	static constexpr float kGoalBreakDropletLifeMin = 0.8f;
	static constexpr float kGoalBreakDropletLifeMax = 1.2f;
	// ゴールの的を壊したときの閃光の大きさ・寿命
	static constexpr float kGoalBreakFlashScale = 0.6f;
	static constexpr float kGoalBreakFlashLife = 0.3f;
	// しぶきが面に着いたときに塗るインクの半径(しぶきの大きさに対する倍率)
	// しぶきは画面の邪魔にならないよう小さめにし、その分倍率を上げて、台に残る跡は見える大きさにする
	static constexpr float kLandingInkRadiusRate = 6.0f;
};
