#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <memory>

class ITransitionEffect;
class Sprite;
class SpriteCommon;

/// <summary>
/// 画面のつながりが切れる瞬間(シーンの切り替え・落下リスタートなど)を隠すための暗転
/// 画面を覆う → 覆い終わったら指定された処理を呼ぶ → 画面を戻す、の順に進める。
/// シーンを切り替えても消えないよう SceneManager が持ち、すべてのシーンの上に描画する。
/// 進め方(Update)と見た目(Draw)を分けているため、後から見た目だけを別の演出に差し替えられる。
/// 演出ごとの見た目は systems/transition フォルダのクラス(ITransitionEffect を継承したもの)に分けてあり、
/// このクラスは暗転の進め方と、今の演出の呼び出しだけを持つ。
/// </summary>
class SceneTransition{
public:
	// 画面を覆う演出の種類
	enum class Style{
		Fade,        // 黒で徐々に覆う(シーンの切り替えで使う基本の暗転)
		RailStripes, // レールを模した斜めの帯が左右から走って覆い、始点の方向(左)へ抜けていく
		FallAndLift, // 帯が上から落ちてきて覆い、下に沈んでから上へ持ち上げるように抜けていく
		RetryCutIn,  // 画面を暗くして斜めの帯と「RETRY」の文字を差し込み、黒の帯を広げて覆う
		InkBurst,    // 自分の黄色いインクが画面の中心から外へ次々に弾けて塗りつぶし、下へ垂れ落ちて抜けていく(クリア)
		InkDrip,     // 暗いインクが画面の上から不揃いに垂れてきて覆い、そのまま下へ沈んで抜けていく(ゲームオーバー)
		RailPaint,   // 斜めのレールの線が走り、そのあとを黄色いインクが塗り進んで覆い、塗った帯が右へ抜けていく(クリアの案)
		GoalStamp,   // 「GOAL!」が上から押されて揺れ、しみが飛んだ後に中央から黄色いインクが広がって覆う(クリアの案)
		CloudRise,   // 下から雲が湧き上がって画面を包み、左右に割れて抜けていく(クリアの案)
	};

	SceneTransition();
	~SceneTransition();

	// 画面全体を覆うスプライトを生成する
	void Initialize(SpriteCommon* spriteCommon);

	// スプライトを解放する(SceneManager はプログラム終了まで残るため、DirectX の終了前に呼ぶ)
	void Finalize();

	// 暗転を始める。画面を覆い終わったら onCovered を1回だけ呼ぶ
	// すでに暗転中なら何もせず false を返す(同じ遷移が二重に始まらないようにするため)
	bool Start(std::function<void()> onCovered,Style style = Style::Fade);

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

	// 演出の種類の数(Style の最後の種類の次の番号。Style に種類を足したら、最後の種類に合わせて直す)
	static constexpr size_t kStyleCount = static_cast<size_t>(Style::CloudRise) + 1;

	// 今の演出の見た目を取得する
	ITransitionEffect* GetCurrentEffect() const;

	// 今の段階
	Phase phase_ = Phase::None;
	// 今の演出の種類
	Style style_ = Style::Fade;
	// 画面の覆い具合(0: 何も覆っていない、1: 完全に覆っている)
	float coverRate_ = 0.0f;
	// 画面を覆い終わったときに呼ぶ処理
	std::function<void()> onCovered_;

	SpriteCommon* spriteCommon_ = nullptr;
	// 画面全体を覆う黒のスプライト(Fade の暗転と、RetryCutIn で画面を暗くするのに使う)
	// 別の色を掛けて使う演出(InkBurst・InkDrip・GoalStamp・CloudRise)もあるため、各演出の Initialize() にも渡す
	std::unique_ptr<Sprite> fadeSprite_;
	// 演出ごとの見た目(Style の番号順に1つずつ持つ)
	std::array<std::unique_ptr<ITransitionEffect>,kStyleCount> effects_;
};
