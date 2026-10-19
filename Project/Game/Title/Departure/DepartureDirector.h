#pragma once

#include "MyMath.h"
#include "Camera/CameraPose.h"
#include "Title/Departure/DepartureContext.h"
#include <array>
#include <iterator>
#include <memory>

// 前方宣言
class BaseDeparture;

/// <summary>
/// 出発の演出の種類(ImGui の "GameScene Debug" の Departure で切り替える)
/// 新しい演出を足すときは、Count の前に追加し、DepartureDirector::kStyleNames と Initialize() の一覧にも足す
/// </summary>
enum class DepartureStyle{
	Walk,      // 演出なし(開いたゲートをくぐって、自分で歩いてレールに乗る)
	SuperJump, // スーパージャンプで打ち上がり、空からレールの始点へ降りる
	InkDive,   // インク溜まりに潜って画面をインクで覆い、レールの始点から飛び出す
	Catapult,  // レールの始点で力を溜め、速さを上げて飛び出す
	CountdownFlip, // ゲートのカウントダウンに合わせて溜め、0 で宙返りしながらレールに飛び乗る
	Count,
};

/// <summary>
/// 出発の演出(スタート台からステージのレールへ移るときの演出)の切り替えと進行
/// 出発ゲートが開いた瞬間に Prepare() で使う演出を決め、出発台の手前の線を越えたら Begin() で始める。
/// ゲートのカウントダウンに合わせる演出(StartsWithGate() が true)は、ゲートを開け始めた瞬間に Prepare() と Begin() を続けて呼ぶ。
/// 演出はプレイヤーの表示とカメラを動かし、レールへ乗せ終えたら ConsumeBoarded() で GameScene に知らせる。
/// </summary>
class DepartureDirector{
public:
	// ImGui の切り替えに表示する名前(DepartureStyle と同じ順番)
	static constexpr const char* kStyleNames[] = {"Walk", "SuperJump", "InkDive", "Catapult", "CountdownFlip"};
	static_assert(std::size(kStyleNames) == static_cast<size_t>(DepartureStyle::Count),"出発の演出の名前と種類の数が合っていない");

	DepartureDirector();
	~DepartureDirector();

	// 初期化(演出を種類ごとに1つずつ生成する)
	void Initialize();

	/// <summary>
	/// 使う演出を決め、出発台の見た目を整える(出発ゲートが開いた瞬間に呼ぶ)
	/// </summary>
	/// <param name="style">使う演出の種類</param>
	/// <param name="context">演出が動かす・参照するもの</param>
	void Prepare(DepartureStyle style,const DepartureContext& context);

	// 出発台を使う演出か(Walk とゲートを開け始めた瞬間に始める演出以外。出発台の目印を出し、線を越えたら演出を始める)
	bool UsesDeparturePad() const;

	/// <summary>
	/// 出発ゲートを開け始めた瞬間に始める演出か(ゲートのカウントダウンに合わせる演出)
	/// </summary>
	/// <param name="style">調べる演出の種類</param>
	bool StartsWithGate(DepartureStyle style) const;

	/// <summary>
	/// Prepare() で決めた演出を始める(出発台の手前の線を越えたときに呼ぶ)
	/// </summary>
	/// <param name="context">演出が動かす・参照するもの</param>
	/// <param name="startCamera">始めた瞬間に映っているカメラの位置と向き</param>
	void Begin(const DepartureContext& context,const CameraPose& startCamera);

	/// <summary>
	/// 演出を進める(演出が終わったら止める)
	/// </summary>
	/// <param name="deltaTime">経過時間(秒。ヒットストップ中は0)</param>
	/// <param name="gameplayCamera">このフレームのプレイ用カメラの位置と向き</param>
	void Update(float deltaTime,const CameraPose& gameplayCamera);

	/// <summary>
	/// このフレームでレールに乗せ終え、「GO!」を出す瞬間を迎えたかを受け取る(受け取ったら false に戻る)
	/// </summary>
	/// <param name="outPosition">乗せた位置</param>
	bool ConsumeBoarded(Vector3& outPosition);

	// 演出を進めている最中か
	bool IsRunning() const{ return runningDeparture_ != nullptr; }

	/// <summary>
	/// 演出のカメラの位置と向きを取得する
	/// </summary>
	/// <param name="outPose">演出のカメラの位置と向き</param>
	/// <returns>カメラを演出側で動かしている最中なら true</returns>
	bool GetCameraPose(CameraPose& outPose) const;

	// 進めている演出を止める(使う演出の選択は残す。落下リスタート後も出発台から演出を始められるようにする)
	void Reset();

private:
	// 種類ごとの演出(Walk は演出が無いため空のまま)
	std::array<std::unique_ptr<BaseDeparture>,static_cast<size_t>(DepartureStyle::Count)> departures_;
	// Prepare() で決めた演出(Walk なら nullptr)
	BaseDeparture* preparedDeparture_ = nullptr;
	// 進めている最中の演出
	BaseDeparture* runningDeparture_ = nullptr;

	// レールに乗せ終えた知らせをまだ受け取られていないか、と乗せた位置
	bool isBoardedPending_ = false;
	Vector3 boardedPosition_ = {0.0f, 0.0f, 0.0f};
};
