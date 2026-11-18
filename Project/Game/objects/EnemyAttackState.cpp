#include "EnemyAttackState.h"
#include "EnemyBulletPool.h"
#include <cmath>

// -------------------------------------------------
// 単発
// -------------------------------------------------

// 発射した瞬間のプレイヤー位置へ向かって直進する弾を1発撃つ
void SingleShotAttackState::Fire(EnemyBulletPool& bulletPool,const Vector3& spawnPosition,const Vector3& baseDirection) const{
	bulletPool.Fire(spawnPosition,baseDirection,kBulletSpeed,false);
}

// 単発の次は3方向拡散を撃つ
std::unique_ptr<EnemyAttackState> SingleShotAttackState::CreateNext() const{
	return std::make_unique<SpreadShotAttackState>();
}

// -------------------------------------------------
// 3方向拡散
// -------------------------------------------------

// プレイヤー方向を中心に、Y軸回転で左右へ角度をつけた弾を同時発射する
// 中央の弾を基準(添字kBulletCount / 2)として、そこからのずれ分だけ角度をつける
void SpreadShotAttackState::Fire(EnemyBulletPool& bulletPool,const Vector3& spawnPosition,const Vector3& baseDirection) const{
	const int centerIndex = kBulletCount / 2;
	for(int i = 0; i < kBulletCount; ++i){
		float angle = static_cast<float>(i - centerIndex) * kSpreadAngle;

		// Y軸回転でXZ平面上の向きだけを回す(上下の角度は中央の弾と同じにする)
		float cosAngle = std::cos(angle);
		float sinAngle = std::sin(angle);
		Vector3 direction = {
			baseDirection.x * cosAngle + baseDirection.z * sinAngle,
			baseDirection.y,
			-baseDirection.x * sinAngle + baseDirection.z * cosAngle
		};

		bulletPool.Fire(spawnPosition,direction,kBulletSpeed,false);
	}
}

// 3方向拡散の次は追尾弾を撃つ
std::unique_ptr<EnemyAttackState> SpreadShotAttackState::CreateNext() const{
	return std::make_unique<HomingShotAttackState>();
}

// -------------------------------------------------
// 追尾弾
// -------------------------------------------------

// 発射後もプレイヤーを追い続ける弾を1発だけ撃つ
void HomingShotAttackState::Fire(EnemyBulletPool& bulletPool,const Vector3& spawnPosition,const Vector3& baseDirection) const{
	bulletPool.Fire(spawnPosition,baseDirection,kHomingBulletSpeed,true);
}

// 追尾弾の次は先頭の単発へ戻る
std::unique_ptr<EnemyAttackState> HomingShotAttackState::CreateNext() const{
	return std::make_unique<SingleShotAttackState>();
}
