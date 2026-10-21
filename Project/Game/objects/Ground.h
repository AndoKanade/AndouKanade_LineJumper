#pragma once

#include "MyMath.h"
#include <memory>
#include <string>
#include <vector>

// 前方宣言
class Obj3D;
class Obj3dCommon;
class CloudWave;

/// <summary>
/// 雲海の地面
/// 起伏のある雲の板モデルと、その上に雲と同じ形で重ねた半透明の霧の層を、格子状に並べて表現する。
/// 格子の向きと位置の基準はレール開始地点に合わせ、タイルはカメラの真下を中心に並べ直すことで、
/// レールをどこまで進んでも、カメラから kDrawRadius 以内には必ずタイルがあるようにする。
/// 霧は下ほど濃く上ほど薄い層を何枚も重ね、層ごとに流れ方を変えることで厚みのあるもやに見せる。
/// 向き・大きさは生成時に決め打ちし、毎フレームは並べ直し・行列の更新・雲の起伏のうねり・霧のUVスクロールを行う。
/// </summary>
class Ground{
public:
	Ground();
	~Ground();

	/// <summary>
	/// 雲と霧のタイルを生成する
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	/// <param name="startPosition">格子の位置の基準(レール開始地点。この位置にタイルの中心が来るように格子をそろえる)</param>
	/// <param name="forward">格子の向きの基準(レール開始地点での向き)</param>
	/// <param name="paramGroup">霧の濃さの調整項目を登録するGlobalVariablesのグループ名</param>
	void Initialize(Obj3dCommon* objCommon,const Vector3& startPosition,const Vector3& forward,const std::string& paramGroup);

	/// <summary>
	/// 更新処理(カメラの真下を中心にタイルを並べ直す・カメラ切り替えへの追従・行列の更新・雲の起伏のうねり・霧のUVスクロール)
	/// </summary>
	/// <param name="deltaTime">1フレームの経過時間(秒)</param>
	void Update(float deltaTime);

	// 雲の描画処理(他のオブジェクトより先に描く)
	void Draw();

	// 霧の描画処理(半透明なので、他の3Dオブジェクトをすべて描いた後に描く)
	void DrawFog();

	// 地面を敷いている高さを取得(落下リスタートの判定に使う)
	float GetHeight() const{ return kGroundHeight; }

private:
	/// <summary>
	/// 板モデルのタイルを、格子1つ分の枚数だけ生成する(雲と霧で共通。並べる位置は PlaceTiles() で決める)
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	/// <param name="modelPath">タイルに使うモデルのパス</param>
	/// <param name="height">タイルを敷くY座標</param>
	/// <param name="thicknessScale">板の厚み方向(モデルのZ方向)の拡大率</param>
	/// <param name="tiles">生成したタイルの格納先</param>
	void CreateTiles(Obj3dCommon* objCommon,const std::string& modelPath,float height,float thicknessScale,std::vector<std::unique_ptr<Obj3D>>& tiles);

	/// <summary>
	/// 指定した格子のマスを中心に、タイルを前後左右へ kTileCountRadius 枚ずつ並べる(高さは生成時のまま変えない)
	/// </summary>
	/// <param name="tiles">並べるタイル</param>
	/// <param name="centerDepthIndex">中心のマスの、基準の位置から進行方向へ何マス目か</param>
	/// <param name="centerWidthIndex">中心のマスの、基準の位置から右方向へ何マス目か</param>
	void PlaceTiles(std::vector<std::unique_ptr<Obj3D>>& tiles,int centerDepthIndex,int centerWidthIndex);

	// カメラの真下のマスが変わったら、雲と霧のタイルをそのマスを中心に並べ直す
	void RecenterTiles(const Vector3& cameraPosition);

	// タイルをアクティブなカメラに追従させて行列を更新する
	void UpdateTiles(std::vector<std::unique_ptr<Obj3D>>& tiles);

	// 起伏のある雲のタイル
	std::vector<std::unique_ptr<Obj3D>> cloudTiles_;
	// 雲と霧の起伏をうねらせる(全タイルで同じモデルを使うため1つで足りる)
	std::unique_ptr<CloudWave> cloudWave_;

	// 雲の上に重ねる半透明の霧の層1枚分
	struct FogLayer{
		// 層のタイル
		std::vector<std::unique_ptr<Obj3D>> tiles;
		// 霧のテクスチャをずらしている量(UV座標)
		Vector2 uvOffset = {0.0f, 0.0f};
	};
	// 霧の層(下の層から順に並べ、描画も下から行う)
	std::vector<FogLayer> fogLayers_;

	// 霧の濃さの調整項目を登録したGlobalVariablesのグループ名
	std::string paramGroup_;

	// 格子の基準の位置(レール開始地点。この位置がマス(0, 0)の中心になる)
	Vector3 gridOrigin_ = {0.0f, 0.0f, 0.0f};
	// 格子の進行方向・右方向(どちらも水平な単位ベクトル。タイルはこの2方向へ並べる)
	Vector3 gridForward_ = {0.0f, 0.0f, 1.0f};
	Vector3 gridRight_ = {1.0f, 0.0f, 0.0f};
	// 今タイルを並べている中心のマス(カメラの真下のマスが変わったときだけ並べ直すために覚えておく)
	int centerDepthIndex_ = 0;
	int centerWidthIndex_ = 0;
	// 一度でもタイルを並べたか(最初の Update() では必ず並べる)
	bool hasPlacedTiles_ = false;

	// 板モデル1枚の1辺の長さ(cloudSurface.obj・fogSurface.objは-1〜1の2x2なのでこの値になる)
	static constexpr float kTilePlaneSize = 2.0f;
	// 板モデルに掛ける表示スケール(1タイルの1辺はkTilePlaneSize倍された長さになる)
	static constexpr float kTileScale = 10.0f;
	// タイル1枚の1辺の長さ(この間隔で並べると隙間なく敷き詰められる)
	static constexpr float kTileStep = kTilePlaneSize * kTileScale;
	// 必ずタイルを敷いておく範囲(カメラからの水平距離)
	// カメラの遠平面(Camera の farClip の初期値 100)より先は描画されないため、同じ距離まで敷けば見える範囲に抜けが出ない
	static constexpr float kDrawRadius = 100.0f;
	// カメラの真下のマスから前後左右へ並べるタイルの枚数
	// カメラは中心のマスの中(中心から半マス以内)にいるため、端までの距離は最低でも「枚数 × 1辺の長さ」になる
	static constexpr int kTileCountRadius = 5;
	static_assert(static_cast<float>(kTileCountRadius) * kTileStep >= kDrawRadius,"タイルを並べる枚数が、必ず敷いておく範囲に足りていない");
	// 1辺に並べるタイルの枚数(中心のマスと、その前後(左右)に kTileCountRadius 枚ずつ)
	static constexpr int kTileCountPerSide = kTileCountRadius * 2 + 1;
	// 地面を敷くY座標(レールの起伏に関係なく一定の高さにする)
	static constexpr float kGroundHeight = -3.0f;
	// 板モデルは+Z向きなので、X軸を-90度回して法線を上向き(+Y)にする
	static constexpr float kRotateX = -3.14159265f * 0.5f;

	// 雲の起伏の高さ(cloudSurface.objの起伏は-1〜1なので、山と谷の差はこの値の2倍になる)
	static constexpr float kCloudWaveHeight = 0.6f;
	// 雲の鏡面反射の鋭さ(雲はほぼ光らないため、大きくしてハイライトをほとんど出さない)
	static constexpr float kCloudShininess = 1000.0f;

	// 霧は半透明で描くため、完全に透明なピクセルだけ捨てる
	static constexpr float kFogAlphaCutoff = 0.0f;

	// 霧の濃さの倍率の初期値(各層の不透明度にまとめて掛ける。保存済みJSONがあればそちらが優先される)
	static constexpr float kDefaultFogDensity = 1.0f;
	// 霧の不透明度として使える範囲(倍率を掛けた結果がこの範囲を超えないように制限する)
	static constexpr float kFogAlphaMin = 0.0f;
	static constexpr float kFogAlphaMax = 1.0f;
};
