#pragma once

#include <functional>
#include <memory>

class Sprite;
class SpriteCommon;

/// <summary>
/// 画面のつながりが切れる瞬間(シーンの切り替え・落下リスタートなど)を隠すための暗転
/// 画面を覆う → 覆い終わったら指定された処理を呼ぶ → 画面を戻す、の順に進める。
/// シーンを切り替えても消えないよう SceneManager が持ち、すべてのシーンの上に描画する。
/// 進め方(Update)と見た目(Draw)を分けているため、後から見た目だけを別の演出に差し替えられる。
/// </summary>
class SceneTransition{
public:
	SceneTransition();
	~SceneTransition();

	// 画面全体を覆うスプライトを生成する
	void Initialize(SpriteCommon* spriteCommon);

	// スプライトを解放する(SceneManager はプログラム終了まで残るため、DirectX の終了前に呼ぶ)
	void Finalize();

	// 暗転を始める。画面を覆い終わったら onCovered を1回だけ呼ぶ
	// すでに暗転中なら何もせず false を返す(同じ遷移が二重に始まらないようにするため)
	bool Start(std::function<void()> onCovered);

	// 暗転を進める
	void Update(float deltaTime);

	// 画面を覆うスプライトを描画する(すべてのシーンの描画の後に呼ぶ)
	void Draw();

	// 暗転中かどうか(覆っている途中・覆い終わり・戻している途中のいずれか)
	bool IsActive() const{ return phase_ != Phase::None; }

private:
	enum class Phase{
		None,      // 何もしていない
		Covering,  // 画面を覆っている途中
		Covered,   // 覆い終わった(真っ暗な画面を1フレーム描画してから onCovered を呼ぶ)
		Revealing, // 画面を戻している途中
	};

	// 今の段階
	Phase phase_ = Phase::None;
	// 画面の覆い具合(0: 何も覆っていない、1: 完全に覆っている)
	float coverRate_ = 0.0f;
	// 画面を覆い終わったときに呼ぶ処理
	std::function<void()> onCovered_;

	SpriteCommon* spriteCommon_ = nullptr;
	// 画面全体を覆う黒のスプライト
	std::unique_ptr<Sprite> fadeSprite_;

	// 画面を覆いきるまでの時間(秒)
	static constexpr float kCoverDuration = 0.4f;
	// 画面を戻しきるまでの時間(秒)
	static constexpr float kRevealDuration = 0.4f;
	// 覆い具合の上限と下限
	static constexpr float kFullyCovered = 1.0f;
	static constexpr float kNotCovered = 0.0f;
};
