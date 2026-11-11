#pragma once

class Sprite;
class SpriteCommon;

/// <summary>
/// 画面を覆う演出(SceneTransition::Style)1種類分の見た目の基底クラス
/// 暗転の進め方(覆う → 覆い終わり → 戻す)は SceneTransition が持ち、このクラスを継承したクラスは
/// 「今の覆い具合に合わせてスプライトをどう置くか」と「どの順に重ねて描くか」だけを決める。
///
/// 新しい演出を足すときは、このクラスを継承して Initialize() / Finalize() / Update() / Draw() と演出の時間を作り、
/// SceneTransition の Style と演出の一覧(SceneTransition のコンストラクタ)に登録する。
/// </summary>
class ITransitionEffect{
public:
	virtual ~ITransitionEffect() = default;

	/// <summary>
	/// テクスチャの読み込みとスプライトの生成
	/// </summary>
	/// <param name="spriteCommon">スプライト共通設定</param>
	/// <param name="fadeSprite">画面全体を覆うスプライト(SceneTransition が持つ。色を掛けて、画面を暗くする・隙間を塗るのに使う)</param>
	virtual void Initialize(SpriteCommon* spriteCommon,Sprite* fadeSprite) = 0;

	// スプライトを解放する
	virtual void Finalize() = 0;

	// 演出ごとの画面を覆いきるまでの時間(秒)
	virtual float GetCoverDuration() const = 0;
	// 演出ごとの画面を戻しきるまでの時間(秒)
	virtual float GetRevealDuration() const = 0;

	/// <summary>
	/// 演出ごとに、今の覆い具合に合わせてスプライトの位置・大きさ・色を決める
	/// </summary>
	/// <param name="coverRate">画面の覆い具合(0: 何も覆っていない、1: 完全に覆っている)</param>
	/// <param name="isRevealing">画面を戻している途中か(false なら、覆っている途中か覆い終わったところ)</param>
	virtual void Update(float coverRate,bool isRevealing) = 0;

	// スプライトを描画する(スプライト共通の描画前処理は SceneTransition が済ませている)
	virtual void Draw() = 0;
};
