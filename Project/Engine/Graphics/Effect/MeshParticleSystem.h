#pragma once

#include "MyMath.h"
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// 前方宣言
class Camera;
class DXCommon;
class Model;
class Obj3dCommon;

/// <summary>
/// GPU で動かすモデルのパーティクル
/// 種類(しぶき・破片など)ごとに決まった数の入れ物を持ち、空きが無いときは一番古いものを上書きする。
/// 出したときの状態は CPU で決め、その後の動き(重力・回転・寿命・大きさと色の変わり方)と描画に使う行列はコンピュートシェーダーで求める。
/// 描画は種類ごとに1回の命令でまとめて行い、見た目は Obj3D と同じライティング・半透明の合成で描く。
/// 決まった高さを上から下へ横切った粒は CPU へ知らせ、CPU で面に着いたと判断したら次のフレームに消す。
/// </summary>
class MeshParticleSystem{
public:
	// 同時に使える種類の数の上限(MeshParticle.hlsli の kMaxKinds と合わせる)
	static constexpr uint32_t kMaxKinds = 5;

	/// <summary>
	/// 種類ごとの設定
	/// </summary>
	struct KindDesc{
		// 描くモデル(ModelManager に登録するときのパス。resource/ からの相対パス)
		std::string modelPath;
		// モデルの大きさを揃える倍率(モデルを差し替えたときに、元のモデルと同じ大きさに見えるよう合わせる)
		float modelScale = 1.0f;
		// モデルの頂点に足す量(モデルの座標系)。モデルの原点が形の中心にないときに、回転の中心を形の中心へ合わせる
		Vector3 modelOffset = {0.0f, 0.0f, 0.0f};
		// 同時に出せる数
		uint32_t capacity = 0;
		// 下向きの重力加速度(1秒あたりの下方向への速度変化量)
		float gravity = 0.0f;
		// 光の当たり方を計算するか(false なら色をそのまま出す)
		bool enableLighting = true;
		// 飛ぶ向きにモデルの +Z を向けて伸ばすか・速さ1あたりに伸ばす割合・伸ばす上限・伸ばし始める速さ
		bool stretchesAlongVelocity = false;
		float stretchPerSpeed = 0.0f;
		float maxStretch = 1.0f;
		float minStretchSpeed = 0.0f;
		// 寿命のこの割合を過ぎたら、小さくなりながら消える(1 なら縮まない)
		float shrinkStartRate = 1.0f;
		// 広がりながら透明になって消えるか・寿命が尽きる瞬間の大きさ(出たときの何倍か)
		bool expandsAndFades = false;
		float expandScale = 1.0f;
		// 空気抵抗の強さ(大きいほど早く止まる。0 なら減速しない)
		float drag = 0.0f;
		// 寿命に合わせて、出たときの色(SpawnDesc::color)から endColor へ変えるか・寿命が尽きる瞬間の色
		bool changesColor = false;
		Vector4 endColor = {1.0f, 1.0f, 1.0f, 0.0f};
	};

	/// <summary>
	/// 1つ出すときの設定
	/// </summary>
	struct SpawnDesc{
		Vector3 position = {0.0f, 0.0f, 0.0f};
		Vector3 velocity = {0.0f, 0.0f, 0.0f};
		// 出たときの回転(ラジアン)と、1秒あたりの回転量
		Vector3 rotate = {0.0f, 0.0f, 0.0f};
		Vector3 angularVelocity = {0.0f, 0.0f, 0.0f};
		Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
		// 大きさ(全軸共通)
		float baseScale = 1.0f;
		// 寿命(秒)
		float lifeTime = 1.0f;
		// SetCrossing() で決めた高さを上から下へ横切ったら知らせるか
		bool reportsCrossing = false;
	};

	/// <summary>
	/// 決まった高さを上から下へ横切った粒を受け取る処理の型
	/// 移動前の座標・移動後の座標・大きさを受け取り、true を返すとその粒を消す
	/// </summary>
	using CrossingHandler = std::function<bool(const Vector3& from,const Vector3& to,float baseScale)>;

	MeshParticleSystem();
	~MeshParticleSystem();

	/// <summary>
	/// 初期化処理(モデルの読み込み・GPU のバッファとパイプラインの生成)
	/// </summary>
	/// <param name="objCommon">ライト・カメラ・遠くを溶け込ませる距離を Obj3D と共通で使うため</param>
	/// <param name="kinds">種類ごとの設定(並んだ順に 0, 1, 2... の番号で Spawn() に渡す)</param>
	void Initialize(Obj3dCommon* objCommon,const std::vector<KindDesc>& kinds);

	/// <summary>
	/// 1つ出す(実際に GPU へ書き込むのは Draw() のとき)
	/// </summary>
	/// <param name="kindIndex">Initialize() で渡した種類の番号</param>
	/// <param name="desc">出したときの状態</param>
	void Spawn(uint32_t kindIndex,const SpawnDesc& desc);

	// 出ている粒をすべて消す
	void Clear();

	/// <summary>
	/// 横切ったかを調べる高さと、横切った粒を受け取る処理を設定する
	/// 調べるのは水平な面1枚だけ(GPU で調べるため)。処理を設定しなければ調べない
	/// </summary>
	void SetCrossing(float height,CrossingHandler handler);

	/// <summary>
	/// 前のフレームに横切った粒を受け取る処理へ渡し、このフレームに進める時間を受け取る
	/// 呼ばなかったフレームは、粒を止めたままにする
	/// </summary>
	void Update(float deltaTime);

	/// <summary>
	/// GPU で粒を進め、種類ごとにまとめて描く
	/// </summary>
	void Draw(const Camera& camera);

private:
	// GPU へ渡す構造体(MeshParticle.hlsli の同じ名前の構造体と並びを合わせる)
	struct GpuParticle;
	struct GpuSpawnRequest;
	struct GpuInstance;
	struct GpuCrossingReport;
	struct GpuKindParams;
	struct GpuFrameConstants;
	struct GpuViewConstants;
	struct GpuDrawConstants;

	// 1種類分の入れ物の範囲と描き方
	struct Kind{
		Model* model = nullptr;
		// 全種類を1つの配列に並べたときの、この種類の先頭の番号と数
		uint32_t firstSlot = 0;
		uint32_t capacity = 0;
		// 次に使う入れ物(この種類の中での番号。一番古いものから順に上書きする)
		uint32_t nextSlot = 0;
		bool enableLighting = true;
	};

	// 指定の入れ物へ書き込む要求を積む(同じフレームに同じ入れ物へ2回出したときは、後の要求で上書きする)
	void RequestWrite(uint32_t slot,const GpuParticle& particle);
	// 指定の入れ物を空きにする要求を積む
	void RequestKill(uint32_t slot);

	// バッファ・パイプラインの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(size_t sizeInBytes);
	void CreateBuffers();
	void CreateComputePipelines();
	void CreateGraphicsPipeline();

	Obj3dCommon* objCommon_ = nullptr;
	DXCommon* dxCommon_ = nullptr;

	std::vector<Kind> kinds_;
	// 入れ物の総数(全種類の数の合計)
	uint32_t particleCount_ = 0;

	// 入れ物ごとの使い回した回数(知らせが届いたときに、別の粒へ入れ替わっていないかを確かめる)
	std::vector<uint32_t> generations_;
	// このフレームに積んだ要求の番号(入れ物ごと。まだ積んでいなければ kNoRequest)
	std::vector<uint32_t> requestIndexOfSlot_;
	// このフレームに要求を積んだ入れ物の番号(Draw() の後に requestIndexOfSlot_ を戻すのに使う)
	std::vector<uint32_t> requestedSlots_;

	// GPU のバッファ
	// 粒の状態・描画用の行列と色・横切った粒の知らせとその数(GPU だけが読み書きする)
	Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> reportBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> reportCountBuffer_;
	// 知らせの数を毎フレーム 0 に戻すための、0 を入れたバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> zeroCountBuffer_;
	// 知らせを CPU で読むためのバッファ(先頭に数、その後ろに知らせを並べる)
	Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer_;
	const uint8_t* readbackData_ = nullptr;
	// 出す要求(CPU から書き込む)
	Microsoft::WRL::ComPtr<ID3D12Resource> spawnRequestBuffer_;
	GpuSpawnRequest* spawnRequests_ = nullptr;
	uint32_t spawnRequestCount_ = 0;
	// 1フレーム分の設定・カメラと描き方の設定
	Microsoft::WRL::ComPtr<ID3D12Resource> frameConstantBuffer_;
	GpuFrameConstants* frameConstants_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> viewConstantBuffer_;
	GpuViewConstants* viewConstants_ = nullptr;

	// パイプライン
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> spawnPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> updatePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> graphicsRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	// Update() で受け取り、次の Draw() で GPU へ渡す時間(渡したら 0 に戻し、Update() を呼ばないフレームは止める)
	float pendingDeltaTime_ = 0.0f;
	// 横切ったかを調べる高さと、横切った粒を受け取る処理
	float crossingHeight_ = 0.0f;
	CrossingHandler crossingHandler_;
	// 前のフレームの知らせを CPU で読むバッファへ写したか(まだ Update() で読んでいない知らせがあるか)
	bool hasPendingReports_ = false;
};
