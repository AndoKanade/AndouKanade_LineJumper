#include "InkEffectManager.h"
#include "Obj3dCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace{
	// --- 種類ごとのモデル ---
	// 配布モデルから差し替えるときは、ここのパス・倍率・ずらす量だけを書き換える(テクスチャはモデルの .mtl のものをそのまま使う)
	// ・倍率(ModelScale)は、差し替えたモデルが今と同じ大きさに見えるよう合わせる値
	//   (今のしぶき・閃光・火花の球は半径 1、破片の三角柱は幅 1・高さ 1 ほど。半径 2 のモデルにしたら 0.5 にする)
	// ・ずらす量(ModelOffset)は、モデルの原点が形の中心にないときに、回る中心を形の中心へ合わせるためにモデルの頂点へ足す量
	//   (例: 原点が底にある高さ 1 のモデルなら {0, -0.5, 0})
	// ・しぶきは飛んでいる向きへモデルの +Z を向けて伸ばすため、しずく形などにするときは伸ばしたい向きを +Z にしておく
	// インクのしぶき
	// しぶき・閃光・火花は、凹凸の無いなめらかな球(tools/generate_smooth_sphere.ps1 で生成)を使う
	const std::string kDropletModelPath = "SmoothSphere/smoothSphere.obj";
	constexpr float kDropletModelScale = 1.0f;
	constexpr Vector3 kDropletModelOffset = {0.0f, 0.0f, 0.0f};
	// 的の破片(薄い三角柱)
	const std::string kShardModelPath = "InkShard/shard.obj";
	constexpr float kShardModelScale = 1.0f;
	constexpr Vector3 kShardModelOffset = {0.0f, 0.0f, 0.0f};
	// 撃破時の閃光
	const std::string kFlashModelPath = "SmoothSphere/smoothSphere.obj";
	constexpr float kFlashModelScale = 1.0f;
	constexpr Vector3 kFlashModelOffset = {0.0f, 0.0f, 0.0f};
	// 被弾の火花
	const std::string kSparkModelPath = "SmoothSphere/smoothSphere.obj";
	constexpr float kSparkModelScale = 1.0f;
	constexpr Vector3 kSparkModelOffset = {0.0f, 0.0f, 0.0f};
	// ボスの予備動作の光の粒(飛んでいる向きへモデルの +Z を向けて伸ばす)
	const std::string kChargeModelPath = "SmoothSphere/smoothSphere.obj";
	constexpr float kChargeModelScale = 1.0f;
	constexpr Vector3 kChargeModelOffset = {0.0f, 0.0f, 0.0f};

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
	// 火花の出た瞬間の色(明るい黄色)と、消える瞬間の色(赤みがかった橙で透明)
	constexpr Vector4 kSparkStartColor = {1.0f, 0.8f, 0.3f, 1.0f};
	constexpr Vector4 kSparkEndColor = {1.0f, 0.3f, 0.0f, 0.0f};
	// ボスの予備動作の光の粒の色(白く光る体に合わせて、少し黄みがかった白)
	constexpr Vector4 kChargeColor = {1.0f, 1.0f, 0.85f, 1.0f};

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

	// 被弾の火花: 光って見えるようライティングを切り、重力で落ちながら空気抵抗で減速し、色を変えながら小さくなって消える
	MeshParticleSystem::KindDesc& spark = kinds[static_cast<size_t>(Kind::Spark)];
	spark.modelPath = kSparkModelPath;
	spark.modelScale = kSparkModelScale;
	spark.modelOffset = kSparkModelOffset;
	spark.capacity = kSparkCount;
	spark.gravity = kSparkGravity;
	spark.drag = kSparkDrag;
	spark.enableLighting = false;
	spark.shrinkStartRate = kSparkShrinkStartRate;
	spark.changesColor = true;
	spark.endColor = kSparkEndColor;

	// ボスの予備動作の光の粒: 光って見えるようライティングを切り、重力をかけずにまっすぐ体へ飛ばし、飛ぶ向きに伸ばして吸い込まれる勢いを見せる
	MeshParticleSystem::KindDesc& charge = kinds[static_cast<size_t>(Kind::Charge)];
	charge.modelPath = kChargeModelPath;
	charge.modelScale = kChargeModelScale;
	charge.modelOffset = kChargeModelOffset;
	charge.capacity = kChargeCount;
	charge.enableLighting = false;
	charge.stretchesAlongVelocity = true;
	charge.stretchPerSpeed = kChargeStretchPerSpeed;
	charge.maxStretch = kChargeMaxStretch;
	charge.minStretchSpeed = kMinStretchSpeed;
	charge.shrinkStartRate = kChargeShrinkStartRate;

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

// 最後のステージへ続く通路の壁が壊れたときの演出
// ゴールの的を壊したときと同じ数・速さ・大きさで、破片を壁の色(1つおきに暗くした色)にし、壁が砕けたように見せる
void InkEffectManager::EmitBarrierBreak(const Vector3& position,const Vector4& barrierColor){
	const Vector4 shadedColor = {barrierColor.x * kBarrierShardShadeRate, barrierColor.y * kBarrierShardShadeRate, barrierColor.z * kBarrierShardShadeRate, barrierColor.w};
	for(int i = 0; i < kGoalBreakShardCount; ++i){
		MeshParticleSystem::SpawnDesc shard;
		shard.position = position;
		shard.velocity = Normalize(RandomVector()) * RandomRange(kGoalBreakShardSpeedMin,kGoalBreakShardSpeedMax) + kWorldUp * kGoalBreakShardUpSpeed;
		shard.angularVelocity = RandomVector() * kBreakShardSpinMax;
		shard.color = (i % kBarrierShardShadeInterval == 0)?barrierColor:shadedColor;
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

// 弾が当たったときの火花
// 中心の周りの小さな球の中からばらばらの向き・速さで四方へ飛ばす
void InkEffectManager::EmitSpark(const Vector3& position){
	for(int i = 0; i < kSparkEmitCount; ++i){
		MeshParticleSystem::SpawnDesc spark;
		spark.position = position + Normalize(RandomVector()) * RandomRange(0.0f,kSparkRadius);
		spark.velocity = Normalize(RandomVector()) * RandomRange(0.0f,kSparkSpeed);
		spark.color = kSparkStartColor;
		spark.baseScale = kSparkScale;
		spark.lifeTime = kSparkLife;
		Spawn(Kind::Spark,spark);
	}
}

// 体力のある相手に弾が当たったときに、当たった面から跳ねるインクのしぶき
// 跳ねる向きを中心に横へばらけさせ、重力で弧を描いて落ちるようにする
void InkEffectManager::EmitHitSplash(const Vector3& position,const Vector3& direction){
	for(int i = 0; i < kHitSplashCount; ++i){
		MeshParticleSystem::SpawnDesc droplet;
		droplet.position = position;
		droplet.velocity = direction * RandomRange(kHitSplashSpeedMin,kHitSplashSpeedMax) + RandomVector() * kHitSplashSpread;
		droplet.color = kInkColor;
		droplet.baseScale = RandomRange(kHitSplashScaleMin,kHitSplashScaleMax);
		droplet.lifeTime = RandomRange(kHitSplashLifeMin,kHitSplashLifeMax);
		droplet.reportsCrossing = true;
		Spawn(Kind::Droplet,droplet);
	}
}

// ボスの予備動作で、周りから体へ吸い込まれる光の粒
// 体の周りのばらばらの向き・距離から出し、寿命が尽きる瞬間にちょうど体の表面へ届く速さで中心へ飛ばす
// 寿命を撃つまでの残り時間にそろえるため、予備動作の間に出した粒は撃つ瞬間にまとめて体へ届く
void InkEffectManager::EmitBossCharge(const Vector3& center,float bodyRadius,float arriveTime){
	if(arriveTime < kChargeMinLife){
		return;
	}
	const float lifeTime = (std::min)(arriveTime,kChargeMaxLife);

	for(int i = 0; i < kChargeEmitCount; ++i){
		const Vector3 direction = Normalize(RandomVector());
		const float distance = bodyRadius * RandomRange(kChargeSpawnDistanceRateMin,kChargeSpawnDistanceRateMax);
		MeshParticleSystem::SpawnDesc charge;
		charge.position = center + direction * distance;
		charge.velocity = direction * (-(distance - bodyRadius) / lifeTime);
		charge.color = kChargeColor;
		charge.baseScale = RandomRange(kChargeScaleMin,kChargeScaleMax);
		charge.lifeTime = lifeTime;
		Spawn(Kind::Charge,charge);
	}
}

// レール・スタート台へ着地したときに、足元から輪の形に広がるインクのしぶき
// 1周を等しい角度に分けて向きを決め(最初の向きはばらばらにして、毎回同じ形に見えないようにする)、速さだけをばらつかせる
void InkEffectManager::EmitLandingRing(const Vector3& position){
	const float startAngle = RandomRange(0.0f,kFullTurn);
	for(int i = 0; i < kLandingRingCount; ++i){
		const float angle = startAngle + kFullTurn * static_cast<float>(i) / static_cast<float>(kLandingRingCount);
		const float horizontalSpeed = RandomRange(kLandingRingHorizontalSpeedMin,kLandingRingHorizontalSpeedMax);
		MeshParticleSystem::SpawnDesc droplet;
		droplet.position = position;
		droplet.velocity = {std::cos(angle) * horizontalSpeed, RandomRange(kLandingRingUpSpeedMin,kLandingRingUpSpeedMax), std::sin(angle) * horizontalSpeed};
		droplet.color = kInkColor;
		droplet.baseScale = RandomRange(kLandingRingScaleMin,kLandingRingScaleMax);
		droplet.lifeTime = RandomRange(kLandingRingLifeMin,kLandingRingLifeMax);
		droplet.reportsCrossing = true;
		Spawn(Kind::Droplet,droplet);
	}
}

// 鍵の的を壊して出たレールの、伸びている管の先端から飛び散るインクのしぶき
// 上へ跳ね上げてからばらばらの向きへ散らし、先端が進んだ跡にしぶきが残っていくように見せる
// 台に落ちると自分のインク(黄色)で塗られて色が合わないため、面に着いても塗らない
void InkEffectManager::EmitRailGrowSplash(const Vector3& position,const Vector4& color){
	for(int i = 0; i < kRailGrowSplashCount; ++i){
		MeshParticleSystem::SpawnDesc droplet;
		droplet.position = position;
		droplet.velocity = kWorldUp * kRailGrowUpSpeed + RandomVector() * kRailGrowSpread;
		droplet.color = color;
		droplet.baseScale = RandomRange(kRailGrowScaleMin,kRailGrowScaleMax);
		droplet.lifeTime = RandomRange(kRailGrowLifeMin,kRailGrowLifeMax);
		droplet.reportsCrossing = false;
		Spawn(Kind::Droplet,droplet);
	}
}

// 飛んでいる弾から垂れるインクの粒
// 弾の勢いは引き継がずにその場へ残し、わずかにばらけさせて重力で垂れ落ちさせる
void InkEffectManager::EmitBulletTrail(const Vector3& position,const Vector4& color){
	MeshParticleSystem::SpawnDesc droplet;
	droplet.position = position;
	droplet.velocity = RandomVector() * kBulletTrailSpread;
	droplet.color = color;
	droplet.baseScale = RandomRange(kBulletTrailScaleMin,kBulletTrailScaleMax);
	droplet.lifeTime = RandomRange(kBulletTrailLifeMin,kBulletTrailLifeMax);
	droplet.reportsCrossing = false;
	Spawn(Kind::Droplet,droplet);
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
