#include "PlayerBulletManager.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include <cmath>
#include <string>

namespace{
	// 弾の表示に使用するモデル
	const std::string kBulletModelPath = "Sphere/sphere.obj";

	// 弾の色(プレイヤーのインクの黄色。敵弾のオレンジより明るくして区別する)
	constexpr Vector4 kBulletColor = {1.0f, 0.9f, 0.2f, 1.0f};

	// 弾のばらつきの横方向を求めるときに使う真上の向き
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};
	// 狙う方向が真上・真下に近く、横方向を求められないときに代わりに使う横方向
	constexpr Vector3 kFallbackRight = {1.0f, 0.0f, 0.0f};
	// 横方向を求められないとみなす長さ
	constexpr float kParallelEpsilon = 0.0001f;
}

PlayerBulletManager::PlayerBulletManager() = default;
PlayerBulletManager::~PlayerBulletManager() = default;

// 初期化処理
void PlayerBulletManager::Initialize(Obj3dCommon* objCommon){
	objCommon_ = objCommon;

	ModelManager::GetInstance()->LoadModel(kBulletModelPath);
	model_ = ModelManager::GetInstance()->FindModel(kBulletModelPath);

	// 弾の生成
	// 発射のたびに生成すると無駄な処理が毎回発生するため、ここで最大数ぶんまとめて作り、以降は使い回す
	bullets_.resize(kMaxBulletCount);
	for(auto& bullet : bullets_){
		bullet.obj = std::make_unique<Obj3D>();
		bullet.obj->Initialize(objCommon_);
		bullet.obj->SetModel(kBulletModelPath);

		// 弾の色は生成時に一度設定するだけでよいため、ここでインクの色にしておく
		// 周りの明るさに関係なく光る筋に見えるよう、ライティングは切る
		if(Model::Material* material = bullet.obj->GetMaterial()){
			material->color = kBulletColor;
			material->enableLighting = 0;
		}
	}

	std::random_device seedGenerator;
	randomEngine_.seed(seedGenerator());
}

// 狙う方向を、ばらつきの角度の範囲でランダムにずらした向きを返す
Vector3 PlayerBulletManager::ApplySpread(const Vector3& direction){
	// 狙う方向に垂直な横方向と縦方向を求める
	Vector3 right = Cross(kWorldUp,direction);
	if(Length(right) < kParallelEpsilon){
		right = kFallbackRight;
	}
	right = Normalize(right);
	const Vector3 up = Cross(direction,right);

	// 横・縦それぞれに、最大の角度までのランダムな角度だけ傾ける
	std::uniform_real_distribution<float> horizontalDistribution(-kSpreadAngleHorizontal,kSpreadAngleHorizontal);
	std::uniform_real_distribution<float> verticalDistribution(-kSpreadAngleVertical,kSpreadAngleVertical);
	const float horizontalAngle = horizontalDistribution(randomEngine_);
	const float verticalAngle = verticalDistribution(randomEngine_);
	return Normalize(direction + right * std::tan(horizontalAngle) + up * std::tan(verticalAngle));
}

// 弾を1発発射する
void PlayerBulletManager::Fire(const Vector3& position,const Vector3& direction,bool canHit){
	if(bullets_.empty()){
		return;
	}

	// 未使用の弾を探して使い回す
	// 全弾使用中のときは、撃つのが止まらないよう、一番長く飛んでいる弾(一番先に消える弾)を消して使い回す
	Bullet* selected = nullptr;
	for(auto& bullet : bullets_){
		if(!bullet.isAlive){
			selected = &bullet;
			break;
		}
		if(!selected || bullet.lifeTime > selected->lifeTime){
			selected = &bullet;
		}
	}
	if(selected->isAlive){
		++poolFullCount_;
	}

	selected->position = position;
	selected->velocity = ApplySpread(direction) * kSpeed;
	selected->lifeTime = 0.0f;
	selected->traveledDistance = 0.0f;
	selected->canHit = canHit;
	selected->isAlive = true;
}

// 飛んでいる弾の数を数える
int PlayerBulletManager::GetAliveCount() const{
	int count = 0;
	for(const auto& bullet : bullets_){
		if(bullet.isAlive){
			++count;
		}
	}
	return count;
}

// 弾の移動と生存時間の更新
void PlayerBulletManager::Move(float deltaTime){
	for(auto& bullet : bullets_){
		if(!bullet.isAlive) continue;

		// 射程を過ぎた弾は、勢いを落としながら重力で落とす
		// 速度の変化を先に適用してから位置を更新する(半陰的オイラー法)
		if(bullet.traveledDistance >= kRange){
			bullet.velocity = bullet.velocity * (1.0f - kFallDrag * deltaTime);
			bullet.velocity.y -= kFallGravity * deltaTime;
		}

		const Vector3 previousPosition = bullet.position;
		const Vector3 movement = bullet.velocity * deltaTime;
		bullet.position += movement;
		bullet.traveledDistance += Length(movement);
		bullet.lifeTime += deltaTime;
		if(bullet.lifeTime >= kLifeTime){
			bullet.isAlive = false;
			continue;
		}

		// このフレームの移動の線分で面に着いたかを調べる(速い弾でも面をすり抜けない)
		// 着いたらそこにインクを塗って消滅させる
		if(landingHandler_ && landingHandler_(previousPosition,bullet.position,kLandingInkRadius)){
			bullet.isAlive = false;
		}
	}
}

// 指定した境界球に重なる生存中の弾を1発探し、見つかればその弾を消滅させる
// 描画用オブジェクトの行列は判定より後で更新されるため、弾の現在座標と表示スケールから境界球を求める
bool PlayerBulletManager::CheckHit(const Model::BoundingSphere& sphere){
	if(!model_){
		return false;
	}

	for(auto& bullet : bullets_){
		if(!bullet.isAlive || !bullet.canHit) continue;

		Model::BoundingSphere bulletSphere = model_->GetBoundingSphere(bullet.position,kScale);
		if(sphere.IsHit(bulletSphere)){
			bullet.isAlive = false;
			return true;
		}
	}
	return false;
}

// 弾のトランスフォームを更新する(描画用)
// 球を飛んでいる向きへ細長く伸ばして、インクの筋に見せる
// 未使用の弾は描かないため更新しない
void PlayerBulletManager::UpdateTransforms(){
	for(auto& bullet : bullets_){
		if(bullet.isAlive && bullet.obj){
			// 球モデルのZ軸を速度の向きへ回す(Y軸回転で左右、X軸回転で上下を合わせる)
			Vector3 direction = Normalize(bullet.velocity);
			float horizontalLength = std::sqrt(direction.x * direction.x + direction.z * direction.z);
			bullet.obj->SetRotate({std::atan2(-direction.y,horizontalLength), std::atan2(direction.x,direction.z), 0.0f});

			bullet.obj->SetTranslate(bullet.position);
			bullet.obj->SetScale({kScale * kStreakWidthRate, kScale * kStreakWidthRate, kScale * kStreakLengthRate});
			if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
				bullet.obj->SetCamera(activeCamera);
			}
			bullet.obj->Update();
		}
	}
}

// 発射中の弾を描画する
void PlayerBulletManager::Draw(){
	for(auto& bullet : bullets_){
		if(bullet.isAlive && bullet.obj){
			bullet.obj->Draw();
		}
	}
}

// すべての弾を未使用に戻す(生成した弾は消さずに、次の発射で使い回す)
void PlayerBulletManager::Clear(){
	for(auto& bullet : bullets_){
		bullet.isAlive = false;
	}
}
