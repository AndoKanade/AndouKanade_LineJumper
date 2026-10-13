#include "StartPlatform.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include <cmath>

namespace{
	// 台に使うモデル(-1〜1の立方体。スケールに台の大きさの半分を掛けて使う)
	const std::string kPlatformModelPath = "StartPlatform/startPlatform.obj";

	// 台の色(レールのピンクや雲の白と見分けやすい灰色)
	constexpr Vector4 kPlatformColor = {0.55f, 0.6f, 0.68f, 1.0f};
}

StartPlatform::StartPlatform() = default;
StartPlatform::~StartPlatform() = default;

// 初期化処理
void StartPlatform::Initialize(Obj3dCommon* objCommon){
	ModelManager::GetInstance()->LoadModel(kPlatformModelPath);
	obj_ = std::make_unique<Obj3D>();
	obj_->Initialize(objCommon);
	obj_->SetModel(kPlatformModelPath);

	// 色は変わらないため、生成時に一度設定するだけでよい
	if(Model::Material* material = obj_->GetMaterial()){
		material->color = kPlatformColor;
	}
}

// レールの始点に合わせて台を置く
// 上面の高さはレールの始点にそろえ、前の端がレールの始点よりkRailOverlapLengthだけ先に出るよう、進行方向の手前側に置く
void StartPlatform::Place(const Vector3& railStartPosition,const Vector3& railStartForward){
	// 台は水平に置くため、進行方向は上下の成分を除いた向きにする
	forwardXZ_ = Normalize(Vector3{railStartForward.x, 0.0f, railStartForward.z});
	rightXZ_ = {forwardXZ_.z, 0.0f, -forwardXZ_.x};

	// 上面の中央(プレイヤーの開始位置)は、前の端から台の長さの半分だけ手前になる
	spawnPosition_ = railStartPosition - forwardXZ_ * (kHalfLength - kRailOverlapLength);

	if(!obj_){
		return;
	}

	// モデルの中心は上面から厚みの半分だけ下になる
	obj_->SetTranslate({spawnPosition_.x, spawnPosition_.y - kHalfHeight, spawnPosition_.z});
	// モデルの+Z方向を台の前方向へ向ける
	obj_->SetRotate({0.0f, std::atan2(forwardXZ_.x,forwardXZ_.z), 0.0f});
	obj_->SetScale({kHalfWidth, kHalfHeight, kHalfLength});
	// Play中は台が動かないため、ワールド行列はここで求めておく(カメラとの合成はDraw()の中で毎回行われる)
	obj_->Update();
}

// 描画処理
void StartPlatform::Draw(){
	if(!obj_){
		return;
	}

	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		obj_->SetCamera(activeCamera);
	}
	obj_->Draw();
}

// 水平方向(XZ平面)で見て、指定した座標が台の上面の範囲に入っているか
// 台の中心からの差を台の前方向・右方向の成分に分け、それぞれが大きさの半分以内かで判定する
bool StartPlatform::IsInsideTop(const Vector3& position) const{
	Vector3 diff = {position.x - spawnPosition_.x, 0.0f, position.z - spawnPosition_.z};
	return std::abs(Dot(diff,forwardXZ_)) <= kHalfLength && std::abs(Dot(diff,rightXZ_)) <= kHalfWidth;
}
