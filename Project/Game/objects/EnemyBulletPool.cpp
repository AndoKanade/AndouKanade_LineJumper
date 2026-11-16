#include "EnemyBulletPool.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include <string>

namespace{
	// 敵弾の表示に使用するモデル
	const std::string kEnemyBulletModelPath = "Sphere/sphere.obj";
}

EnemyBulletPool::EnemyBulletPool() = default;
EnemyBulletPool::~EnemyBulletPool() = default;

// 初期化処理
// 発射のたびに生成すると無駄な処理が毎フレーム発生するため、ここで最大数ぶんまとめて作り、以降は使い回す
void EnemyBulletPool::Initialize(Obj3dCommon* objCommon,const Settings& settings){
	settings_ = settings;

	ModelManager::GetInstance()->LoadModel(kEnemyBulletModelPath);
	bulletModel_ = ModelManager::GetInstance()->FindModel(kEnemyBulletModelPath);
	bullets_.resize(settings_.count);
	for(auto& bullet : bullets_){
		bullet.obj = std::make_unique<Obj3D>();
		bullet.obj->Initialize(objCommon);
		bullet.obj->SetModel(kEnemyBulletModelPath);
		bullet.obj->SetScale({settings_.scale, settings_.scale, settings_.scale});

		// 弾の色は生成時に一度設定するだけでよい
		if(Model::Material* material = bullet.obj->GetMaterial()){
			material->color = settings_.color;
		}
	}
}

// 発射済みの弾をすべて未使用に戻す
void EnemyBulletPool::Clear(){
	for(auto& bullet : bullets_){
		bullet.isAlive = false;
		bullet.lifeTime = 0.0f;

		// 追尾情報も消して、再利用した弾が前回の設定を引きずらないようにする
		bullet.isHoming = false;
		bullet.speed = 0.0f;
	}
}

// 弾の更新処理(移動・追尾の向き補正・寿命切れの判定・描画用トランスフォームの更新)
void EnemyBulletPool::Update(const Vector3& playerPosition,float deltaTime){
	// アクティブカメラは全弾で共通なので、ループの外で一度だけ取得する
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();

	for(auto& bullet : bullets_){
		// 未使用の弾は移動も描画更新も不要
		if(!bullet.isAlive){
			continue;
		}

		// 追尾弾の向き補正
		// 現在の進行方向をプレイヤー方向へ少しずつ寄せることで、急に折れ曲がらない緩やかな追尾にする
		if(bullet.isHoming){
			Vector3 toPlayer = playerPosition - bullet.position;
			if(Length(toPlayer) > kMinDirectionLength){
				Vector3 currentDirection = Normalize(bullet.velocity);
				Vector3 targetDirection = Normalize(toPlayer);

				// 1フレームで補正する割合(上限を超えると一気に向き切り替わってしまうため制限する)
				float turnRatio = settings_.homingTurnRate * deltaTime;
				if(turnRatio > kMaxHomingTurnRatio){
					turnRatio = kMaxHomingTurnRatio;
				}

				// 速さは保ったまま向きだけを変える
				Vector3 newDirection = Normalize(currentDirection + (targetDirection - currentDirection) * turnRatio);
				bullet.velocity = newDirection * bullet.speed;
			}
		}

		// 等速直線運動で前進させる(プレイヤーが避けやすいよう重力は掛けない)
		bullet.position += bullet.velocity * deltaTime;

		// 何にも当たらなかった弾は一定時間で未使用に戻し、再利用できるようにする
		bullet.lifeTime += deltaTime;
		if(bullet.lifeTime >= settings_.lifeTime){
			bullet.isAlive = false;
			continue;
		}

		// 描画用オブジェクトへ反映する
		if(bullet.obj){
			bullet.obj->SetTranslate(bullet.position);
			if(activeCamera){
				bullet.obj->SetCamera(activeCamera);
			}
			bullet.obj->Update();
		}
	}
}

// 指定した方向へ弾を1発発射する
void EnemyBulletPool::Fire(const Vector3& spawnPosition,const Vector3& direction,float speed,bool isHoming){
	// 未使用の弾を探して使い回す(全弾使用中のときは発射しない)
	for(auto& bullet : bullets_){
		if(bullet.isAlive){
			continue;
		}

		bullet.position = spawnPosition;
		bullet.velocity = Normalize(direction) * speed;
		bullet.speed = speed;
		bullet.isHoming = isHoming;
		bullet.lifeTime = 0.0f;
		bullet.isAlive = true;
		return;
	}
}

// 発射済みの弾とプレイヤーの当たり判定
int EnemyBulletPool::CheckHitToPlayer(const Model::BoundingSphere& playerSphere){
	int hitCount = 0;

	// 弾のモデルが無いときは境界球を求められないため判定しない
	if(!bulletModel_){
		return hitCount;
	}

	for(auto& bullet : bullets_){
		if(!bullet.isAlive){
			continue;
		}

		// 発射したばかりの弾は描画用オブジェクトの行列がまだ更新されていないため、
		// 弾の現在座標と表示スケールから境界球を求める(弾は球なので回転は考慮しなくてよい)
		Model::BoundingSphere bulletSphere = bulletModel_->GetBoundingSphere(bullet.position,settings_.scale);

		// 弾とプレイヤーの境界球が重なっていれば命中とみなし、その弾を未使用に戻す
		if(bulletSphere.IsHit(playerSphere)){
			bullet.isAlive = false;
			++hitCount;
		}
	}

	return hitCount;
}

// 発射中の弾を描画する
void EnemyBulletPool::Draw(){
	for(auto& bullet : bullets_){
		if(bullet.isAlive && bullet.obj){
			bullet.obj->Draw();
		}
	}
}

// 発射済みで生存している弾の数を取得(デバッグ表示用)
int EnemyBulletPool::GetActiveCount() const{
	int count = 0;
	for(const auto& bullet : bullets_){
		if(bullet.isAlive){
			++count;
		}
	}
	return count;
}
