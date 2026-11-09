#pragma once

#include "MyMath.h"
#include <memory>
#include <string>
#include <vector>

// 前方宣言
class Obj3D;
class Obj3dCommon;
class Model;
class TargetEditor;
class PlayerBulletManager;

/// <summary>
/// 的の管理
/// 配置エディター(TargetEditor)の内容を的のリストに反映し、照準判定・弾との当たり判定・描画を行う。
/// </summary>
class TargetManager{
public:
	TargetManager();
	~TargetManager();

	/// <summary>
	/// 初期化処理
	/// 配置エディターを生成し、全ステージの保存済みの配置を読み込む
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	/// <param name="paramGroup">調整項目を登録するGlobalVariablesのグループ名</param>
	void Initialize(Obj3dCommon* objCommon,const std::string& paramGroup);

	// 配置エディターの更新と、編集結果(追加・削除・ドラッグ移動)の的リストへの反映
	void UpdateEditor();

	// 編集・プレイするステージを切り替え、的のリストを切り替えた先のステージの配置にして、すべて生存状態に戻す
	void SetStageIndex(int stageIndex);

	// 配置エディターで「Focus」が押されていれば、選択中の的の位置を返す(1回読むと消える)
	bool ConsumeFocusRequest(Vector3& outPosition);

	/// <summary>
	/// 照準判定・弾との当たり判定・描画用の行列更新
	/// </summary>
	/// <param name="cameraPosition">カメラの位置</param>
	/// <param name="cameraForward">カメラの前方ベクトル(画面中央のレティクルの方向)</param>
	/// <param name="bullets">当たり判定を行うプレイヤーの弾</param>
	/// <param name="deltaTime">経過時間(秒。鍵の的を脈打たせるのに使う)</param>
	/// <returns>いずれかの的をレティクルで狙えているときtrue</returns>
	bool Update(const Vector3& cameraPosition,const Vector3& cameraForward,PlayerBulletManager& bullets,float deltaTime);

	// 直前の Update() で鍵の的を壊したか(鍵の的の案内・目印を消すのに使う)
	bool HasDestroyedKeyTarget() const{ return hasDestroyedKeyTarget_; }

	/// <summary>
	/// 生きている鍵の的のうち、指定した位置に一番近いものを探す(鍵の的の目印・案内を出すのに使う)
	/// </summary>
	/// <param name="position">探す基準の位置(プレイヤーの位置)</param>
	/// <param name="maxDistance">探す最大の距離</param>
	/// <param name="outPosition">見つかった鍵の的の中心</param>
	/// <returns>見つかったら true</returns>
	bool FindNearestKeyTarget(const Vector3& position,float maxDistance,Vector3& outPosition) const;

	/// <summary>
	/// レティクルで狙えている(レティクルの中心が赤くなる)的のうち、カメラに一番近いものを探す
	/// 弾をその的の中心へ向けて撃ち、レティクルが赤いときは狙った的へ飛ぶようにするのに使う
	/// </summary>
	/// <param name="cameraPosition">カメラの位置</param>
	/// <param name="cameraForward">カメラの前方ベクトル(正規化済み。画面中央のレティクルの方向)</param>
	/// <param name="minDistance">探す的の、カメラからの前方ベクトルに沿った距離の下限(これより手前の的は無視する)</param>
	/// <param name="outPosition">見つかった的の中心の座標</param>
	/// <param name="outDistance">カメラから見つかった的までの、前方ベクトルに沿った距離</param>
	/// <returns>狙えている的があるときtrue</returns>
	bool FindAimedTarget(const Vector3& cameraPosition,const Vector3& cameraForward,float minDistance,Vector3& outPosition,float& outDistance) const;

	// すべての的を生存状態に戻す
	void Reset();

	// 的ごとに生存しているかを取得(チェックポイントを通ったときに、壊した的を覚えておくのに使う)
	std::vector<bool> GetAliveStates() const;
	// 的ごとの生存状態を、GetAliveStates()で取得した状態に戻す(チェックポイントからやり直すときに使う)
	void RestoreAliveStates(const std::vector<bool>& aliveStates);

	/// <summary>
	/// 鍵の的が残っていて出ていないレールに true を入れる(鍵の的が無いレールと、鍵の的をすべて壊したレールはそのまま)
	/// </summary>
	/// <param name="lockedRails">レールごとに、出ていないかを受け取る配列(呼び出し側でレールの本数分を false で用意しておく)</param>
	void FillLockedRails(std::vector<bool>& lockedRails) const;

	/// <summary>
	/// 指定ステージの鍵の的がすべて残っているときに、出ていないレールに true を入れる(今のステージ以外のレールを描くのに使う)
	/// </summary>
	/// <param name="stageIndex">調べるステージの番号</param>
	/// <param name="lockedRails">レールごとに、出ていないかを受け取る配列(呼び出し側でレールの本数分を false で用意しておく)</param>
	void FillStageLockedRails(int stageIndex,std::vector<bool>& lockedRails) const;

	// 指定したレールが、鍵の的を壊すと出るレールか(飛び移り方の案内を出すかの判定に使う)
	bool IsKeyRail(int railIndex) const;

	// 生存している的を描画する
	void Draw();

	// 直前のUpdate()で壊れた的の座標の一覧を取得(撃破演出・連鎖数の加算に使う)
	const std::vector<Vector3>& GetDestroyedPositions() const{ return destroyedPositions_; }
	// 配置されている的の総数を取得
	int GetTotalCount() const{ return static_cast<int>(targets_.size()); }
	// 壊された的の数を取得
	int GetDestroyedCount() const;

private:
	// 的
	struct Target{
		std::unique_ptr<Obj3D> obj;
		Vector3 position;
		bool isAlive = true;
		// 壊すと出るレールのインデックス(-1: 鍵の的ではない)
		int unlockRailIndex = -1;
		// 鍵の的のモデル(鍵の絵の画像)で描いているか
		bool isKeyModel = false;
	};

	// 配置エディターの内容を的のリストに反映する
	void SyncFromEditor();

	Obj3dCommon* objCommon_ = nullptr;

	// 調整項目を登録したGlobalVariablesのグループ名
	std::string paramGroup_;

	// 的の配置エディター(的の座標はこちらが保持し、こちらは毎フレーム同期する)
	std::unique_ptr<TargetEditor> editor_;

	// 的に使う円盤モデル(当たり判定で境界球を求めるのに使う)
	Model* model_ = nullptr;

	// 的のリスト
	std::vector<Target> targets_;

	// 直前のUpdate()で壊れた的の座標(Update()の最初に空にする)
	std::vector<Vector3> destroyedPositions_;
	// 直前の Update() で鍵の的を壊したか
	bool hasDestroyedKeyTarget_ = false;
	// 鍵の的を脈打たせるための経過時間(秒)
	float keyPulseTimer_ = 0.0f;

	// 照準判定の許容角度の初期値(ラジアン、約5度。保存済みJSONがあればそちらが優先される)
	static constexpr float kDefaultAimHitAngle = 0.09f;
	// 狙えているときの表示スケール(少し大きくして視覚的にフィードバックする)
	static constexpr float kAimedScale = 0.6f;
	// 狙えていないときの表示スケール
	static constexpr float kNormalScale = 0.4f;
	// 鍵の的を普通の的より大きくする倍率(見落とされないよう、狙えているときも同じ倍率を掛ける)
	static constexpr float kKeyTargetScaleRate = 1.5f;
	// 鍵の的を脈打たせる大きさの幅(大きさに対する割合)と速さ(1秒あたりの位相の進み、ラジアン)
	static constexpr float kKeyPulseAmplitude = 0.12f;
	static constexpr float kKeyPulseSpeed = 5.0f;
	// 的の模様に掛ける色(白。普通の的も鍵の的も画像の色のまま出す)
	static constexpr Vector4 kNormalTargetColor = {1.0f, 1.0f, 1.0f, 1.0f};
	// Editモードで配置エディターが選択中の的の色(どの的を選んでいるかSceneビューで分かるよう、普通の的・鍵の的と違う黄色にする)
	static constexpr Vector4 kSelectedTargetColor = {1.0f, 0.85f, 0.1f, 1.0f};
	// カメラと的が重なっているとみなす距離(向きが定まらないため照準判定をしない)
	static constexpr float kMinAimDistance = 0.001f;
};
