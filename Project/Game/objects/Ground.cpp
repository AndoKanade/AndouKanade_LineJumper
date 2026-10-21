#include "Ground.h"
#include "CloudWave.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "GlobalVariables.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace{
	// 雲に使用するモデルのパス(継ぎ目なく並べられる起伏をつけた板に雲海のテクスチャを貼ったもの)
	const std::string kCloudModelPath = "CloudSea/cloudSurface.obj";
	// 霧に使用するモデルのパス(雲と同じ格子の板に、継ぎ目なく並べられるもやのテクスチャを貼ったもの)
	const std::string kFogModelPath = "CloudSea/fogSurface.obj";

	// 起伏に陰影をつけるため、雲はライティングを有効にする
	constexpr int32_t kEnableLighting = 1;
	// 霧はテクスチャの色そのままに見せたいため、ライティングを無効にする
	constexpr int32_t kDisableLighting = 0;

	// 進行方向に直角な横方向を求めるための上方向
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};

	// UVのずらし量を0〜1の範囲に収めるための周期(テクスチャは1ごとに繰り返す)
	constexpr float kUvPeriod = 1.0f;
	// UVは2次元なので、UV変換行列の奥行き方向は拡大しない
	constexpr float kUvDepthScale = 1.0f;
	// 霧の模様は回転させない
	constexpr Vector3 kUvNoRotation = {0.0f, 0.0f, 0.0f};

	// 霧の層の色(白いもや。不透明度は層ごとに決める)
	constexpr Vector3 kFogColor = {1.0f, 1.0f, 1.0f};

	// 霧の層1枚分の設定
	struct FogLayerSetting{
		float heightOffset;     // 雲の表面から浮かせる高さ(雲と同じ形でうねるので、どこでも雲から同じだけ離れる)
		float alpha;            // 不透明度(テクスチャの透明度に掛け合わせる)
		float uvScale;          // テクスチャの繰り返し回数(タイルの境目で模様がつながるよう整数にする)
		Vector2 uvStartOffset;  // 模様の初期のずらし量(層ごとに模様が重ならないようにする)
		Vector2 scrollSpeed;    // 流れる速さ(1秒あたりにずらすUV量)
	};

	// 視錐台の6つの面のうちの1つ(normal・distanceの順に ax + by + cz + d の a,b,c,d にあたる。normalは正規化済み)
	struct FrustumPlane{
		Vector3 normal;
		float distance;
	};
	// 視錐台の面の数(左・右・下・上・近・遠)
	constexpr size_t kFrustumPlaneCount = 6;

	// ビュープロジェクション行列から、視錐台の6つの面を求める
	// 行ベクトル×行列の並びなので、クリップ座標の各成分は行列の各列との内積になる
	// DirectXのクリップ空間(-w<=x<=w, -w<=y<=w, 0<=z<=w)の各条件が、そのまま1つの面になる
	std::array<FrustumPlane,kFrustumPlaneCount> ExtractFrustumPlanes(const Matrix4x4& viewProjection){
		// 行列の列c番目を(x, y, z, w)の係数として取り出す
		auto column = [&viewProjection](int c){
			return std::array<float,4>{viewProjection.m[0][c], viewProjection.m[1][c], viewProjection.m[2][c], viewProjection.m[3][c]};
		};
		const std::array<float,4> columnX = column(0);
		const std::array<float,4> columnY = column(1);
		const std::array<float,4> columnZ = column(2);
		const std::array<float,4> columnW = column(3);

		// 2つの列を足し引きして面の係数にする(sign = 1で足す、-1で引く)
		auto combine = [](const std::array<float,4>& a,const std::array<float,4>& b,float sign){
			return std::array<float,4>{a[0] + b[0] * sign, a[1] + b[1] * sign, a[2] + b[2] * sign, a[3] + b[3] * sign};
		};
		const std::array<std::array<float,4>,kFrustumPlaneCount> coefficients = {{
			combine(columnW,columnX,1.0f),  // 左 (x >= -w)
			combine(columnW,columnX,-1.0f), // 右 (x <= w)
			combine(columnW,columnY,1.0f),  // 下 (y >= -w)
			combine(columnW,columnY,-1.0f), // 上 (y <= w)
			columnZ,                        // 近 (z >= 0)
			combine(columnW,columnZ,-1.0f), // 遠 (z <= w)
		}};

		// 法線の長さで割り、面からの距離をワールド座標の長さで比べられるようにする
		std::array<FrustumPlane,kFrustumPlaneCount> planes{};
		for(size_t i = 0; i < kFrustumPlaneCount; ++i){
			const std::array<float,4>& c = coefficients[i];
			const float length = std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
			const float invLength = (length > 0.0f)?1.0f / length:0.0f;
			planes[i] = {{c[0] * invLength, c[1] * invLength, c[2] * invLength}, c[3] * invLength};
		}
		return planes;
	}

	// 霧の層(下から順)
	// 下ほど濃く上ほど薄くして厚みのあるもやに見せ、層ごとに模様の細かさ・流れる向き・速さを変えて立体的に動かす
	// 4層を重ねた合計の濃さが高すぎると下の雲の起伏が見えなくなるため、各層の不透明度は低めにして雲が透けて見えるようにする
	constexpr std::array<FogLayerSetting,4> kFogLayers = {{
		{0.15f, 0.25f, 1.0f, {0.00f, 0.00f}, {0.03f, 0.05f}},
		{0.40f, 0.17f, 2.0f, {0.37f, 0.21f}, {-0.04f, 0.03f}},
		{0.70f, 0.11f, 1.0f, {0.62f, 0.84f}, {0.05f, -0.02f}},
		{1.05f, 0.06f, 2.0f, {0.15f, 0.53f}, {-0.02f, -0.04f}},
	}};

	// 1つのマスを囲む球の半径(雲と霧の層をまとめて囲み、この球が視錐台の外にあるマスは描かない)
	// 横の広がりはタイルの対角線の半分、縦は雲の起伏と一番上の霧の層の浮かせ量の分だけ余裕を持たせる
	float CalculateTileCullingRadius(float tileStep,float cloudWaveHeight){
		// 正方形の対角線の半分は、1辺の長さ×(√2 / 2)
		constexpr float kHalfDiagonalRate = 0.70710678f;
		float maxFogHeightOffset = 0.0f;
		for(const FogLayerSetting& setting : kFogLayers){
			maxFogHeightOffset = (std::max)(maxFogHeightOffset,setting.heightOffset);
		}
		const float horizontalRadius = tileStep * kHalfDiagonalRate;
		const float verticalRadius = cloudWaveHeight + maxFogHeightOffset;
		return std::sqrt(horizontalRadius * horizontalRadius + verticalRadius * verticalRadius);
	}
}

Ground::Ground() = default;
Ground::~Ground() = default;

// 雲と霧のタイルの生成
// 雲は起伏の高さを板の厚み方向の拡大率で調整し、霧の層は雲と同じ拡大率にして雲の形に沿わせる
void Ground::Initialize(Obj3dCommon* objCommon,const Vector3& startPosition,const Vector3& forward,const std::string& paramGroup){
	if(!objCommon){
		return;
	}

	// 霧の濃さの倍率を調整項目に登録する(第3引数はデフォルト値。保存済みJSONがあればそちらが優先される)
	// ImGuiの "Global Variables" ウィンドウから実行中に変更でき、次のUpdate()で全層の不透明度に反映される
	paramGroup_ = paramGroup;
	GlobalVariables::GetInstance()->AddItem(paramGroup_,"fogDensity",kDefaultFogDensity);

	// 格子の基準の位置と向きを決める
	// 右方向は進行方向と上方向の外積で水平に求め、進行方向も右方向と上方向から求め直して水平にする
	// (レール開始地点が坂になっていても、タイルを水平な間隔で隙間なく並べられるようにする)
	gridOrigin_ = startPosition;
	gridRight_ = Normalize(Cross(kWorldUp,forward));
	gridForward_ = Cross(gridRight_,kWorldUp);

	// 視錐台の外のマスを描かないようにするための、1つのマスを囲む球の半径と、マスごとの見えるかどうか
	tileCullingRadius_ = CalculateTileCullingRadius(kTileStep,kCloudWaveHeight);
	isTileVisible_.assign(static_cast<size_t>(kTileCountPerSide * kTileCountPerSide),true);

	ModelManager::GetInstance()->LoadModel(kCloudModelPath);
	ModelManager::GetInstance()->LoadModel(kFogModelPath);

	// 雲と霧の起伏を時間とともにうねらせる準備をする
	// 霧も雲と同じ形で動かし、雲の山が霧を突き抜けて境目がくっきり出ないようにする
	cloudWave_ = std::make_unique<CloudWave>();
	cloudWave_->Initialize({ModelManager::GetInstance()->FindModel(kCloudModelPath),ModelManager::GetInstance()->FindModel(kFogModelPath)});

	// 雲のタイルを生成する
	CreateTiles(objCommon,kCloudModelPath,kGroundHeight,kCloudWaveHeight,cloudTiles_);
	for(auto& tile : cloudTiles_){
		if(Model::Material* material = tile->GetMaterial()){
			// 起伏に光の当たり方の差を出して立体感をつける
			material->enableLighting = kEnableLighting;
			// 雲がつやつやして見えないよう、鏡面反射をほとんど出さない
			material->shininess = kCloudShininess;
		}
	}

	// 霧の層を下から順に生成する
	fogLayers_.resize(kFogLayers.size());
	for(size_t layerIndex = 0; layerIndex < kFogLayers.size(); ++layerIndex){
		const FogLayerSetting& setting = kFogLayers[layerIndex];
		FogLayer& layer = fogLayers_[layerIndex];
		layer.uvOffset = setting.uvStartOffset;

		CreateTiles(objCommon,kFogModelPath,kGroundHeight + setting.heightOffset,kCloudWaveHeight,layer.tiles);
		for(auto& tile : layer.tiles){
			if(Model::Material* material = tile->GetMaterial()){
				material->enableLighting = kDisableLighting;
				material->color = {kFogColor.x, kFogColor.y, kFogColor.z, setting.alpha};
				// ふちのぼかしを残すため、アルファテストで半透明部分を捨てないようにする
				material->alphaCutoff = kFogAlphaCutoff;
			}
		}
	}
}

// 板モデルのタイルを、格子1つ分の枚数だけ生成する
// 板モデルはXY平面の板なのでX軸を-90度回して水平にする
// 水平方向の位置は PlaceTiles() でカメラの真下を中心に決めるため、ここでは高さだけを決めておく
void Ground::CreateTiles(Obj3dCommon* objCommon,const std::string& modelPath,float height,float thicknessScale,std::vector<std::unique_ptr<Obj3D>>& tiles){
	// 生成数は固定なので、再確保が起きないよう先に確保しておく
	const int tileCount = kTileCountPerSide * kTileCountPerSide;
	tiles.reserve(static_cast<size_t>(tileCount));

	for(int i = 0; i < tileCount; ++i){
		auto tile = std::make_unique<Obj3D>();
		tile->Initialize(objCommon);
		tile->SetModel(modelPath);
		// 高さはレールの起伏に関係なく一定にして、常に足元より下に敷く
		tile->SetTranslate({gridOrigin_.x, height, gridOrigin_.z});
		tile->SetRotate({kRotateX, 0.0f, 0.0f});
		// Z方向は板の厚み(起伏の高さ)に相当するため、横の広がりとは別の拡大率にする
		tile->SetScale({kTileScale, kTileScale, thicknessScale});

		tiles.push_back(std::move(tile));
	}
}

// 指定した格子のマスを中心に、タイルを前後左右へ kTileCountRadius 枚ずつ並べる
// タイルは必ず格子のマスの中心に置くため、並べ直しても雲の起伏と霧の模様の位置は変わらず、動いて見えない
void Ground::PlaceTiles(std::vector<std::unique_ptr<Obj3D>>& tiles,int centerDepthIndex,int centerWidthIndex){
	size_t tileIndex = 0;
	for(int depthOffset = -kTileCountRadius; depthOffset <= kTileCountRadius; ++depthOffset){
		for(int widthOffset = -kTileCountRadius; widthOffset <= kTileCountRadius; ++widthOffset){
			Obj3D* tile = tiles[tileIndex].get();
			++tileIndex;

			Vector3 tilePosition = gridOrigin_
				+ gridForward_ * (static_cast<float>(centerDepthIndex + depthOffset) * kTileStep)
				+ gridRight_ * (static_cast<float>(centerWidthIndex + widthOffset) * kTileStep);
			// 高さは雲・霧の層ごとに生成時に決めた値のまま変えない
			tilePosition.y = tile->GetTranslate().y;
			tile->SetTranslate(tilePosition);
			// タイルは並べ直したとき以外は動かないため、行列(逆行列の計算を含む)はここでだけ求める
			tile->Update();
		}
	}
}

// カメラの真下のマスが変わったら、雲と霧のタイルをそのマスを中心に並べ直す
// マスが変わらない間は並べ直さず、毎フレーム全タイルの位置を計算し直さないようにする
void Ground::RecenterTiles(const Vector3& cameraPosition){
	// カメラの位置を、格子の進行方向・右方向に何マス目かに直す(マスの中心からの距離が半マス以内になるよう四捨五入する)
	const Vector3 fromOrigin = cameraPosition - gridOrigin_;
	const int depthIndex = static_cast<int>(std::lround(Dot(fromOrigin,gridForward_) / kTileStep));
	const int widthIndex = static_cast<int>(std::lround(Dot(fromOrigin,gridRight_) / kTileStep));

	if(hasPlacedTiles_ && depthIndex == centerDepthIndex_ && widthIndex == centerWidthIndex_){
		return;
	}
	hasPlacedTiles_ = true;
	centerDepthIndex_ = depthIndex;
	centerWidthIndex_ = widthIndex;

	PlaceTiles(cloudTiles_,depthIndex,widthIndex);
	for(FogLayer& layer : fogLayers_){
		PlaceTiles(layer.tiles,depthIndex,widthIndex);
	}
}

// 更新処理
// 向き・大きさは生成時に決めているため、カメラの真下を中心にした並べ直し、カメラ切り替えへの追従と行列の更新、
// 雲と霧のうねり、霧のUVスクロールだけ毎フレーム行う
void Ground::Update(float deltaTime){
	// 雲と霧の起伏をうねらせる(全タイルが同じモデルを使うので、モデルの頂点を1回書き換えれば全タイルに反映される)
	if(cloudWave_){
		cloudWave_->Update(deltaTime);
	}

	// レールを進んで(または俯瞰カメラで)カメラが動いても、見える範囲に必ずタイルがあるようにする
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		RecenterTiles(activeCamera->GetTranslate());
	}

	UpdateTiles(cloudTiles_);

	// 調整項目から霧の濃さの倍率を取得(ImGui編集/ホットリロードが即反映される)
	const float fogDensity = GlobalVariables::GetInstance()->GetFloatValue(paramGroup_,"fogDensity");

	for(size_t layerIndex = 0; layerIndex < fogLayers_.size(); ++layerIndex){
		const FogLayerSetting& setting = kFogLayers[layerIndex];
		FogLayer& layer = fogLayers_[layerIndex];

		// 霧のテクスチャを少しずつずらして流れているように見せる
		// ずらし量が大きくなり続けると精度が落ちるため、テクスチャの繰り返し周期で0〜1に戻す
		layer.uvOffset.x = std::fmod(layer.uvOffset.x + setting.scrollSpeed.x * deltaTime,kUvPeriod);
		layer.uvOffset.y = std::fmod(layer.uvOffset.y + setting.scrollSpeed.y * deltaTime,kUvPeriod);

		// 層の全タイルで同じ値にすることで、タイルの境目でも模様がつながる
		const Matrix4x4 uvTransform = MakeAffineMatrix(
			{setting.uvScale, setting.uvScale, kUvDepthScale},
			kUvNoRotation,
			{layer.uvOffset.x, layer.uvOffset.y, 0.0f});

		// 層ごとの不透明度に濃さの倍率を掛ける(下ほど濃く上ほど薄い関係は保ったまま、全体の濃さだけを変える)
		const float fogAlpha = std::clamp(setting.alpha * fogDensity,kFogAlphaMin,kFogAlphaMax);

		for(auto& tile : layer.tiles){
			if(Model::Material* material = tile->GetMaterial()){
				material->uvTransform = uvTransform;
				material->color.w = fogAlpha;
			}
		}

		UpdateTiles(layer.tiles);
	}
}

// タイルをアクティブなカメラに追従させる
// 行列はタイルを並べ直したときに PlaceTiles() で求めるため、ここでは更新しない
// (カメラとの合成は Draw() の中で毎回行われるので、カメラが動いても追従する)
void Ground::UpdateTiles(std::vector<std::unique_ptr<Obj3D>>& tiles){
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
	if(!activeCamera){
		return;
	}

	for(auto& tile : tiles){
		tile->SetCamera(activeCamera);
	}
}

// アクティブなカメラに映るマスを求める
// タイルは1枚の頂点が多いため、カメラの後ろや横にあって映らないマスは雲も霧も描かないようにする
// マスを囲む球が視錐台のどれか1つの面より外側にあれば、そのマスは映らない
void Ground::UpdateTileVisibility(){
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
	if(!activeCamera){
		// カメラが無いときは判定できないため、すべて描く
		std::fill(isTileVisible_.begin(),isTileVisible_.end(),true);
		return;
	}

	const std::array<FrustumPlane,kFrustumPlaneCount> planes = ExtractFrustumPlanes(activeCamera->GetViewProjectionMatrix());
	for(size_t tileIndex = 0; tileIndex < isTileVisible_.size() && tileIndex < cloudTiles_.size(); ++tileIndex){
		// 球の中心は雲のタイルの位置(霧の層は真上に重なっているため、半径の縦方向の余裕で囲む)
		const Vector3& center = cloudTiles_[tileIndex]->GetTranslate();
		bool isVisible = true;
		for(const FrustumPlane& plane : planes){
			if(Dot(plane.normal,center) + plane.distance < -tileCullingRadius_){
				isVisible = false;
				break;
			}
		}
		isTileVisible_[tileIndex] = isVisible;
	}
}

// 雲の描画処理
// カメラの更新が終わった描画のときに映るマスを求め、霧の描画(DrawFog())でも同じ結果を使う
void Ground::Draw(){
	UpdateTileVisibility();

	for(size_t tileIndex = 0; tileIndex < cloudTiles_.size(); ++tileIndex){
		if(isTileVisible_[tileIndex]){
			cloudTiles_[tileIndex]->Draw();
		}
	}
}

// 霧の描画処理
// 上から見下ろすカメラでは下の層ほど奥になるため、下の層から順に描いて半透明を正しく重ねる
// 映るかどうかは、直前の Draw() で求めたマスごとの結果を使う
void Ground::DrawFog(){
	for(auto& layer : fogLayers_){
		for(size_t tileIndex = 0; tileIndex < layer.tiles.size(); ++tileIndex){
			if(isTileVisible_[tileIndex]){
				layer.tiles[tileIndex]->Draw();
			}
		}
	}
}
