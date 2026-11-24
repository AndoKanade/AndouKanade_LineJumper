#include "InkEffectManager.h"
#include "Obj3dCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include <cmath>
#include <numbers>

namespace{
	// --- 種類ごとのモデル ---
	// 配布モデルから差し替えるときは、ここのパス・倍率・ずらす量だけを書き換える(テクスチャはモデルの .mtl のものをそのまま使う)
	// ・倍率(ModelScale)は、差し替えたモデルが今と同じ大きさに見えるよう合わせる値
	//   (今のしぶき・閃光の球は半径 1、破片の三角柱は幅 1・高さ 1 ほど。半径 2 のモデルにしたら 0.5 にする)
	// ・ずらす量(ModelOffset)は、モデルの原点が形の中心にないときに、回る中心を形の中心へ合わせるためにモデルの頂点へ足す量
	//   (例: 原点が底にある高さ 1 のモデルなら {0, -0.5, 0})
	// ・しぶきは飛んでいる向きへモデルの +Z を向けて伸ばすため、しずく形などにするときは伸ばしたい向きを +Z にしておく
	// インクのしぶき
	const std::string kDropletModelPath = "Sphere/sphere.obj";
	constexpr float kDropletModelScale = 1.0f;
	constexpr Vector3 kDropletModelOffset = {0.0f, 0.0f, 0.0f};
	// 的の破片(薄い三角柱)
	const std::string kShardModelPath = "InkShard/shard.obj";
	constexpr float kShardModelScale = 1.0f;
	constexpr Vector3 kShardModelOffset = {0.0f, 0.0f, 0.0f};
	// 撃破時の閃光
	const std::string kFlashModelPath = "Sphere/sphere.obj";
	constexpr float kFlashModelScale = 1.0f;
	constexpr Vector3 kFlashModelOffset = {0.0f, 0.0f, 0.0f};

	// 縮まない種類に渡す、縮み始める寿命の割合(寿命が尽きるまで縮まない)
	constexpr float kNoShrinkRate = 1.0f;

	// プレイヤーのインクの色(黄色)
	constexpr Vector4 kInkColor = {1.0f, 0.85f, 0.1f, 1.0f};
	// 閃光の色(白)
	constexpr Vector4 kFlashColor = {1.0f, 1.0f, 0.9f, 1.0f};
	// 敵のインクの色(ピンク。やられ演出で自機が弾けるときに使う)
	constexpr Vector4 kEnemyInkColor = {1.0f, 0.2f, 0.55f, 1.0f};
	// 的の破片の色(的の外側から順に、緑・オレンジ・白・黒)
	constexpr Vector4 kShardColors[] = {
		{0.43f, 0.80f, 0.27f, 1.0f},
		{1.0f, 0.37f, 0.18f, 1.0f},
		{0.94f, 0.96f, 0.94f, 1.0f},
		{0.18f, 0.18f, 0.2f, 1.0f},
	};
	constexpr size_t kShardColorCount = sizeof(kShardColors) / sizeof(kShardColors[0]);

	// 上方向
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};
	// 1周分の角度(ラジアン)
	constexpr float kFullTurn = 2.0f * std::numbers::pi_v<float>;
}

// GPU のパーティクルは、Initialize() より前に SetLandingHandler() を呼ばれても受け取れるよう、先に作っておく
InkEffectManager::InkEffectManager() : particles_(std::make_unique<MeshParticleSystem>()){}
InkEffectManager::~InkEffectManager() = default;

// 初期化処理
// 種類ごとのモデル・数・動き方を GPU のパーティクルに渡す(並びは Kind と同じ)
void InkEffectManager::Initialize(Obj3dCommon* objCommon){
	std::vector<MeshParticleSystem::KindDesc> kinds(kKindCount);

	// インクのしぶき: 重力で落ち、飛ぶ向きに伸び、寿命の後半で小さくなる
	MeshParticleSystem::KindDesc& droplet = kinds[static_cast<size_t>(Kind::Droplet)];
	droplet.modelPath = kDropletModelPath;
	droplet.modelScale = kDropletModelScale;
	droplet.modelOffset = kDropletModelOffset;
	droplet.capacity = kDropletCount;
	droplet.gravity = kDropletGravity;
	droplet.stretchesAlongVelocity = true;
	droplet.stretchPerSpeed = kDropletStretchPerSpeed;
	droplet.maxStretch = kDropletMaxStretch;
	droplet.minStretchSpeed = kMinStretchSpeed;
	droplet.shrinkStartRate = kShrinkStartRate;

	// 的の破片: 重力で落ち、回転し、寿命の後半で小さくなる
	MeshParticleSystem::KindDesc& shard = kinds[static_cast<size_t>(Kind::Shard)];
	shard.modelPath = kShardModelPath;
	shard.modelScale = kShardModelScale;
	shard.modelOffset = kShardModelOffset;
	shard.capacity = kShardCount;
	shard.gravity = kShardGravity;
	shard.shrinkStartRate = kShrinkStartRate;

	// 撃破時の閃光: 周りの明るさに関係なく光って見えるようライティングを切り、その場で広がりながら透明になる
	MeshParticleSystem::KindDesc& flash = kinds[static_cast<size_t>(Kind::Flash)];
	flash.modelPath = kFlashModelPath;
	flash.modelScale = kFlashModelScale;
	flash.modelOffset = kFlashModelOffset;
	flash.capacity = kFlashCount;
	flash.enableLighting = false;
	flash.shrinkStartRate = kNoShrinkRate;
	flash.expandsAndFades = true;
	flash.expandScale = kFlashExpandScale;

	particles_->Initialize(objCommon,kinds);

	std::random_device seedGenerator;
	randomEngine_.seed(seedGenerator());
}

// 出ている演出をすべて消す
void InkEffectManager::Reset(){
	particles_->Clear();
}

// しぶきの着地を調べる処理を設定する
// GPU のパーティクルからは粒の大きさが届くため、塗るインクの半径に直してから渡す
void InkEffectManager::SetLandingHandler(float surfaceHeight,LandingHandler handler){
	landingHandler_ = std::move(handler);
	particles_->SetCrossing(surfaceHeight,[this](const Vector3& from,const Vector3& to,float baseScale){
		return landingHandler_ && landingHandler_(from,to,baseScale * kLandingInkRadiusRate);
	});
}

// 1つ出す(一番古いものから順に上書きする)
void InkEffectManager::Spawn(Kind kind,const MeshParticleSystem::SpawnDesc& desc){
	particles_->Spawn(static_cast<uint32_t>(kind),desc);
}

// 指定した範囲の乱数を返す
float InkEffectManager::RandomRange(float min,float max){
	std::uniform_real_distribution<float> distribution(min,max);
	return distribution(randomEngine_);
}

// 各軸が -1〜1 の乱数のベクトルを返す
Vector3 InkEffectManager::RandomVector(){
	return {RandomRange(-1.0f,1.0f), RandomRange(-1.0f,1.0f), RandomRange(-1.0f,1.0f)};
}

// 的を壊したときの演出
// 的の色の破片とインクのしぶきを周りへ飛び散らせ、中心に一瞬だけ閃光を出す
void InkEffectManager::EmitTargetBreak(const Vector3& position){
	for(int i = 0; i < kBreakShardCount; ++i){
		MeshParticleSystem::SpawnDesc shard;
		shard.position = position;
		shard.velocity = Normalize(RandomVector()) * RandomRange(kBreakShardSpeedMin,kBreakShardSpeedMax) + kWorldUp * kBreakShardUpSpeed;
		shard.angularVelocity = RandomVector() * kBreakShardSpinMax;
		shard.color = kShardColors[static_cast<size_t>(i) % kShardColorCount];
		shard.baseScale = RandomRange(kBreakShardScaleMin,kBreakShardScaleMax);
		shard.lifeTime = RandomRange(kBreakShardLifeMin,kBreakShardLifeMax);
		Spawn(Kind::Shard,shard);
	}

	for(int i = 0; i < kBreakDropletCount; ++i){
		MeshParticleSystem::SpawnDesc droplet;
		droplet.position = position;
		droplet.velocity = Normalize(RandomVector()) * RandomRange(kBreakDropletSpeedMin,kBreakDropletSpeedMax) + kWorldUp * kBreakDropletUpSpeed;
		droplet.color = kInkColor;
		droplet.baseScale = RandomRange(kBreakDropletScaleMin,kBreakDropletScaleMax);
		droplet.lifeTime = RandomRange(kBreakDropletLifeMin,kBreakDropletLifeMax);
		droplet.reportsCrossing = true;
		Spawn(Kind::Droplet,droplet);
	}

	MeshParticleSystem::SpawnDesc flash;
	flash.position = position;
	flash.color = kFlashColor;
	flash.baseScale = kFlashScale;
	flash.lifeTime = kFlashLife;
	Spawn(Kind::Flash,flash);
}

// 指定した向きへ飛び散るインクのしぶき
void InkEffectManager::EmitInkSplash(const Vector3& position,const Vector3& direction,int count){
	for(int i = 0; i < count; ++i){
		MeshParticleSystem::SpawnDesc droplet;
		droplet.position = position;
		droplet.velocity = direction * RandomRange(kSplashSpeedMin,kSplashSpeedMax) + RandomVector() * kSplashSpread;
		droplet.color = kInkColor;
		droplet.baseScale = RandomRange(kSplashScaleMin,kSplashScaleMax);
		droplet.lifeTime = RandomRange(kSplashLifeMin,kSplashLifeMax);
		droplet.reportsCrossing = true;
		Spawn(Kind::Droplet,droplet);
	}
}

// レールを進んでいる間、足元から後ろへ飛ぶインクのしぶき
// 毎フレーム1粒ずつ出し続けることで、レールの上を滑っている勢いを見せる
void InkEffectManager::EmitRailTrail(const Vector3& position,const Vector3& backward){
	MeshParticleSystem::SpawnDesc droplet;
	droplet.position = position;
	droplet.velocity = backward * kTrailBackSpeed + kWorldUp * kTrailUpSpeed + RandomVector() * kTrailSpread;
	droplet.color = kInkColor;
	droplet.baseScale = RandomRange(kTrailScaleMin,kTrailScaleMax);
	droplet.lifeTime = RandomRange(kTrailLifeMin,kTrailLifeMax);
	droplet.reportsCrossing = true;
	Spawn(Kind::Droplet,droplet);
}

// 出発の合図に、足元から周り一面へ高く弧を描いて飛び散るインクのしぶき
// 横へ飛ぶ向きは水平面の全方向からばらばらに選び、落ちる場所が足元の周りに散らばるようにする
void InkEffectManager::EmitLaunchBurst(const Vector3& position,int count){
	for(int i = 0; i < count; ++i){
		const float angle = RandomRange(0.0f,kFullTurn);
		const float horizontalSpeed = RandomRange(kBurstHorizontalSpeedMin,kBurstHorizontalSpeedMax);
		MeshParticleSystem::SpawnDesc droplet;
		droplet.position = position;
		droplet.velocity = {std::cos(angle) * horizontalSpeed, RandomRange(kBurstUpSpeedMin,kBurstUpSpeedMax), std::sin(angle) * horizontalSpeed};
		droplet.color = kInkColor;
		droplet.baseScale = RandomRange(kBurstScaleMin,kBurstScaleMax);
		droplet.lifeTime = RandomRange(kBurstLifeMin,kBurstLifeMax);
		droplet.reportsCrossing = true;
		Spawn(Kind::Droplet,droplet);
	}
}

// 最後のミスで自機が敵のインクになって弾け飛ぶしぶきと閃光
// 全方向へばらばらの速さで飛ばし、少し上向きの勢いを足して、弧を描いて落ちていくようにする
// しぶきは何かに着いても塗らない(スタート台をピンクに塗らないようにする)
void InkEffectManager::EmitDefeatBurst(const Vector3& position){
	for(int i = 0; i < kDefeatDropletCount; ++i){
		MeshParticleSystem::SpawnDesc droplet;
		droplet.position = position;
		droplet.velocity = Normalize(RandomVector()) * RandomRange(kDefeatDropletSpeedMin,kDefeatDropletSpeedMax) + kWorldUp * kDefeatDropletUpSpeed;
		droplet.color = kEnemyInkColor;
		droplet.baseScale = RandomRange(kDefeatDropletScaleMin,kDefeatDropletScaleMax);
		droplet.lifeTime = RandomRange(kDefeatDropletLifeMin,kDefeatDropletLifeMax);
		droplet.reportsCrossing = false;
		Spawn(Kind::Droplet,droplet);
	}

	MeshParticleSystem::SpawnDesc flash;
	flash.position = position;
	flash.color = kEnemyInkColor;
	flash.baseScale = kDefeatFlashScale;
	flash.lifeTime = kDefeatFlashLife;
	Spawn(Kind::Flash,flash);
}

// ゴールの的を壊したときの演出(普通の的を壊したときと同じ作りで、数・大きさ・速さ・寿命を増やす)
void InkEffectManager::EmitGoalBreak(const Vector3& position){
	for(int i = 0; i < kGoalBreakShardCount; ++i){
		MeshParticleSystem::SpawnDesc shard;
		shard.position = position;
		shard.velocity = Normalize(RandomVector()) * RandomRange(kGoalBreakShardSpeedMin,kGoalBreakShardSpeedMax) + kWorldUp * kGoalBreakShardUpSpeed;
		shard.angularVelocity = RandomVector() * kBreakShardSpinMax;
		shard.color = kShardColors[static_cast<size_t>(i) % kShardColorCount];
		shard.baseScale = RandomRange(kGoalBreakShardScaleMin,kGoalBreakShardScaleMax);
		shard.lifeTime = RandomRange(kGoalBreakShardLifeMin,kGoalBreakShardLifeMax);
		Spawn(Kind::Shard,shard);
	}

	for(int i = 0; i < kGoalBreakDropletCount; ++i){
		MeshParticleSystem::SpawnDesc droplet;
		droplet.position = position;
		droplet.velocity = Normalize(RandomVector()) * RandomRange(kGoalBreakDropletSpeedMin,kGoalBreakDropletSpeedMax) + kWorldUp * kGoalBreakDropletUpSpeed;
		droplet.color = kInkColor;
		droplet.baseScale = RandomRange(kGoalBreakDropletScaleMin,kGoalBreakDropletScaleMax);
		droplet.lifeTime = RandomRange(kGoalBreakDropletLifeMin,kGoalBreakDropletLifeMax);
		droplet.reportsCrossing = true;
		Spawn(Kind::Droplet,droplet);
	}

	MeshParticleSystem::SpawnDesc flash;
	flash.position = position;
	flash.color = kFlashColor;
	flash.baseScale = kGoalBreakFlashScale;
	flash.lifeTime = kGoalBreakFlashLife;
	Spawn(Kind::Flash,flash);
}

// 移動・回転・縮小・寿命の更新
// 動かすのは次の Draw() で GPU が行うため、ここでは進める時間を渡し、前のフレームに面へ着いたしぶきにインクを塗る
void InkEffectManager::Update(float deltaTime){
	particles_->Update(deltaTime);
}

// 出ている演出を描画する(GPU で動かしてから、種類ごとにまとめて描く)
void InkEffectManager::Draw(){
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		particles_->Draw(*activeCamera);
	}
}
