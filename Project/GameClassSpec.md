# Game フォルダ クラス仕様書

最終更新: 2026-10-19

`Game/` 以下の各クラスについて、「何ができるか」「外から何を呼べるか」「中でどう動いているか」をまとめる。
エンジン側（`Engine/`）のクラスは対象外（Game のために追加したものだけ、使う側の項目で触れる）。

---

## 目次

1. [全体構成](#1-全体構成)
2. [systems（シーン管理）](#2-systemsシーン管理)
3. [scenes（シーン）](#3-scenesシーン)
4. [Title（ステージ開始演出）](#4-titleステージ開始演出)
5. [objects（ゲームオブジェクト）](#5-objectsゲームオブジェクト)
6. [Camera（カメラ）](#6-cameraカメラ)
7. [UI](#7-ui)
8. [Editor（配置エディター）](#8-editor配置エディター)
9. [保存ファイル一覧](#9-保存ファイル一覧)
10. [操作キー一覧](#10-操作キー一覧)

---

## 1. 全体構成

```
Game/
├── systems/   シーンの生成・切り替えの仕組み
├── scenes/    GameScene（本編）、ClearScene、GameOverScene
├── Title/     GameScene 内で動くステージ開始演出（タイトル → 回り込み → スタート台で準備 → 出発ゲート → レールへ乗り込む）
│   └── Departure/  スタート台からレールへ移るときの出発の演出（1 種類 1 クラス）
├── objects/   Player / PlayerBulletManager / TargetManager / Enemy / EnemyManager / Ground / Sun / StartPlatform / InkEffectManager / ComboCounter / StageResult
├── Camera/    RailCamera（プレイ用カメラ）/ DebugTopCamera（俯瞰デバッグカメラ）/ ScreenImpact（ヒットストップ・画面の揺れ）/ CameraPose（演出のカメラの補間）
├── UI/        Reticle（画面中央のレティクル）/ StageHUD（体力・的の数・連鎖数）/ SpeedLines（スピード線）/ HelpOverlay（H キーで開く操作説明）
└── Editor/    RailEditor / TargetEditor / EnemyEditor（Editモード中の配置ツール）/ EditHistory・EditorDocumentManager（Undo/Redo と保存の管理）/ EditorPicking（Scene ビューでのマウス選択）
```

### シーンの流れ

```
起動 ─→ GAME ─(レール終端に到達)─→ CLEAR ─(SPACE)─→ GAME
          │
          └─(体力0)─→ GAMEOVER ─(SPACE)─→ GAME
```

- タイトルは独立したシーンではなく、GameScene の中の開始演出（`StartSequence`）として表示する。
- GAME へ切り替わるたびに GameScene が作り直され、Play モードならタイトルから始まる。
- ESC キーでいつでもゲームを終了する（`Framework::Update()`）。

### Edit モードと Play モード

エンジン側の `EditorContext` が持つモード。起動時は Edit モードで、画面上部の Play / Stop ボタンで切り替える。

| モード | GameScene の動き |
| --- | --- |
| Edit | ゲームは静止する。各エディターのパネルが表示され、レール・的・敵を配置できる |
| Play | 入った瞬間に `ResetPlayState()` で初期化し（プレイヤーはスタート台の上に立つ）、開始演出をタイトルから始める。編集パネルは隠れる |

---

## 2. systems（シーン管理）

### BaseScene（`systems/BaseScene.h`）

全シーンの基底クラス（抽象クラス）。

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*, Input*, SpriteCommon*)` | 純粋仮想。シーンの初期化 |
| `Finalize()` | 純粋仮想。シーンの終了処理 |
| `Update()` | 純粋仮想。毎フレームの更新 |
| `Draw()` | 純粋仮想。毎フレームの描画 |
| `SetSceneManager(SceneManager*)` | シーン遷移を依頼するための SceneManager を受け取る |

- 派生クラスは `sceneManager_`（protected）を使って `ChangeScene()` を呼べる。

### AbstractSceneFactory（`systems/AbstractSceneFactory.h`）

シーン生成のインターフェース。`CreateScene(const std::string&)` を純粋仮想で持つ。

### SceneFactory（`systems/SceneFactory.h/.cpp`）

シーン名の文字列から実際のシーンを生成する。

| シーン名 | 生成されるクラス |
| --- | --- |
| `"GAME"` | GameScene |
| `"CLEAR"` | ClearScene |
| `"GAMEOVER"` | GameOverScene |
| 上記以外 | `nullptr` |

### SceneManager（`systems/SceneManager.h/.cpp`）

シングルトン。現在のシーンを 1 つだけ保持し、更新・描画・切り替えを行う。

| 関数 | 内容 |
| --- | --- |
| `GetInstance()` | インスタンスの取得 |
| `SetFactory(AbstractSceneFactory*)` | シーン生成に使う工場を登録する |
| `SetCommonPtr(Obj3dCommon*, Input*, SpriteCommon*)` | 各シーンに渡す共通リソースを登録する |
| `ChangeScene(const std::string&, SceneTransition::Style)` | 指定した演出（省略時は `Fade`）で暗転を始め、画面を覆い終わったら次のシーンを予約する。実際の切り替えはその直後の `Update()` の冒頭。最初のシーンだけは暗転せずに切り替える。暗転中に呼ばれたら無視する |
| `StartTransition(std::function<void()>)` | シーンを切り替えずに暗転だけを行い、画面を覆い終わった瞬間に渡された処理を1回呼ぶ（落下リスタート用） |
| `IsTransitioning()` | 暗転中なら true。GameScene はこれを見て入力・ゲームの進行を止める |
| `Update()` | 予約があれば「旧シーンの Finalize → 新シーンの生成 → SetSceneManager → Initialize」を行ってから、現在のシーンを更新する |
| `Draw()` | 現在のシーンを描画する |
| `Finalize()` | 現在のシーンを終了・解放する |

- 切り替えのたびにシーンは破棄・再生成されるため、シーンをまたいで状態は残らない。
- `Update()` では暗転を進めてから、シーンの切り替え予約の処理とシーンの更新を行う。`Draw()` ではシーンの描画の後に暗転を重ねる。

### SceneTransition（`systems/SceneTransition.h/.cpp`）

画面のつながりが切れる瞬間（シーンの切り替え・落下リスタート）を隠す暗転。シーンを切り替えても途中で消えないよう SceneManager が持つ。
タイトル → プレイはカメラワークでつながっているため使わない。

| 段階 | 内容 |
| --- | --- |
| `None` | 何もしていない |
| `Covering` | 演出ごとの時間をかけて画面を覆う |
| `Covered` | 覆いきった画面を1フレーム描画した後、`Start()` で渡された処理を1回呼ぶ |
| `Revealing` | 演出ごとの時間をかけて画面を戻す |

| 演出（`Style`） | 使う場面 | 覆う / 戻す時間 | 内容 |
| --- | --- | --- | --- |
| `Fade` | 上記以外のシーンの切り替え | 0.4 / 0.4 秒 | 黒の濃さを `Easing::EaseInOutCubic` で変える |
| `RailStripes` / `FallAndLift` / `RetryCutIn` | 落下リスタート（ImGui の「Fall Transition」で選ぶ） | — | 落下リスタート用の案 |
| `InkBurst` | ゲーム → クリア（初期値） | 0.9 / 0.7 秒 | 自分の黄色いインクのしみ 13 個が、画面の中心から外へ順に弾けて広がり（`EaseOutBack`）、最後に隙間を黄色で塗って覆う。戻すときは塗りを消し、しみが少し持ち上がってから下へ垂れ落ちる（`EaseInBack`） |
| `InkDrip` | ゲーム → ゲームオーバー | 1.1 / 0.7 秒 | 画面を少し暗くしながら、上の縁を暗いインクで埋め、太さの違う 16 本のインクが順番ばらばらに加速しながら垂れてきて覆う。垂れた先にはピンクの縁取りが見える。戻すときは、覆ったインクがひとかたまりのまま下へ沈んでいく |

| `RailPaint` | ゲーム → クリア（案） | 0.9 / 0.6 秒 | 斜めのピンクのレールの線 8 本が左端から右端まで走り、そのあとを黄色いインクが両端の丸い帯になって塗り進んで覆う。戻すときは塗った帯が右へ抜けていく |
| `GoalStamp` | ゲーム → クリア（案） | 1.4 / 0.6 秒 | 画面を少し暗くし、「GOAL!」の文字を 3 倍の大きさから押しつけて左右に揺らす。押した衝撃で周りにしみが 5 つ飛び、中央から黄色いインクが広がって覆う。戻すときは、広がったインクが縮みながら下へ落ちる |
| `CloudRise` | ゲーム → クリア（案） | 1.0 / 0.7 秒 | 7 段 × 9 個の雲のかたまり（白と薄い水色の円）が、下の段から順にふくらみながら湧き上がって画面を包む。戻すときは画面の中央から左右に割れる |

- 演出の画像（インクのしみ・白い円・「GOAL!」）は `tools/generate_transition_assets.py` で生成する（`resource/UI/transition/`）。しみの位置・大きさ・傾きは `SceneTransition.cpp` の `kInkSplatLayouts` / `kStampSplashLayouts` で決めている。
- クリアへ切り替える演出は、Edit モードの「GameScene Debug」パネルの「Clear Transition」で選び、「Preview Clear Transition」でその場で見た目を確認できる（落下リスタートの「Fall Transition」と同じ使い方）。
- 進め方（`Update()`）と見た目（`Draw()`）を分けているため、後から見た目だけを別の演出に差し替えられる。

---

## 3. scenes（シーン）

### GameScene（`scenes/GameScene.h/.cpp`）

本編。レールに沿ってカメラとプレイヤーが進み、的と敵を撃つ。
プレイヤー・カメラ・弾・的・敵などの処理はそれぞれのクラスに任せ、GameScene は「呼び出す順番」「Edit / Play モードの切り替え」「開始演出とのつなぎ」「勝敗判定」「デバッグ表示」を担当する。

#### 持っているもの

| メンバ | 内容 |
| --- | --- |
| `skybox_` | 背景のスカイボックス |
| `railEditor_` | レールのデータと編集ツール（RailEditor） |
| `ground_` | 簡易的な地面（Ground） |
| `startPlatform_` | 最初のレールの手前に置くスタート台と出発ゲート（StartPlatform） |
| `sun_` | 平行光源を太陽の高さ・方角から設定する（Sun） |
| `player_` | プレイヤー（Player） |
| `railCamera_` | レール追従カメラ（RailCamera） |
| `debugTopCamera_` | 俯瞰デバッグカメラ（DebugTopCamera） |
| `bulletManager_` | プレイヤーの弾（PlayerBulletManager） |
| `targetManager_` | 的（TargetManager） |
| `enemyManager_` | 雑魚敵（EnemyManager） |
| `startSequence_` | ステージ開始演出（StartSequence）。タイトルロゴもこの中で表示する |
| `reticle_` | 画面中央のレティクル（Reticle） |
| `inkEffect_` | インクのしぶき・的の破片・閃光の演出（InkEffectManager） |
| `comboCounter_` | 連鎖数（ComboCounter） |
| `stageHUD_` | 体力・壊した的の数・連鎖数の表示（StageHUD） |
| `speedLines_` | スピード線（SpeedLines） |
| `helpOverlay_` | H キーで開く操作説明（HelpOverlay） |
| `screenImpact_` | ヒットストップと画面の揺れ（ScreenImpact） |
| `departureDirector_` | 出発の演出の切り替えと進行（DepartureDirector） |

#### 初期化（Initialize）

1. カメラ `"default"`（プレイ用）を作る。
2. スカイボックス、RailEditor、Ground を生成する。地面はレールの開始地点を基準に並べる。
3. GlobalVariables にグループ `GameScene` を作る。調整項目は各クラスが自分の Initialize で登録する。
4. Player、RailCamera、DebugTopCamera（カメラ `"debug_top"` を作る）、PlayerBulletManager を生成する。
5. TargetManager / EnemyManager を生成する。保存データが無い初回起動時だけ自動配置する。
6. Reticle、撃破演出用パーティクルのグループ、StartSequence を生成する。StartSequence には出発ゲートを開け閉めするためにスタート台を渡す。
7. DepartureDirector を生成する。弾・しぶきが台に着いたらインクを塗るよう、PlayerBulletManager と InkEffectManager に着地の処理（`SetLandingHandler()`）を渡す。

#### 毎フレームの更新（Update）の流れ

1. スカイボックス・パーティクル・地面・スタート台の出発ゲート・レティクル・各エディターを更新する。的・敵のエディターの編集結果は、TargetManager / EnemyManager の `UpdateEditor()` で的・敵のリストに反映する。
   - ScreenImpact を進め、ヒットストップ中はスタート台とインクの演出の時間を止める。
   - 遮断棒が跳ね上がった瞬間（`StartPlatform::IsBarBurstTriggered()`）に、ゲートからインクを飛び散らせ、ヒットストップ・画面の揺れ・放射ブラーをかける。出発台を使う演出なら、この瞬間に `DepartureDirector::Prepare()` で使う演出を決める。
2. `UpdateGameplay()` で次の処理を順番に行う。
   1. Play に入った瞬間は、Rail Editor で編集中のレールと選択中の制御点を覚えてから、`ResetPlayState()` と `StartPlaySequence()`（`startSequence_->Start()`）を呼ぶ。ImGui の「Skip Title」が ON なら、続けて `Player::BoardRailStart()` でレールの始点に乗せ、タイトル・スタート台を飛ばしてすぐプレイを始める。「Start From Selected Point」が ON で制御点を選んでいれば、同じくタイトル・スタート台を飛ばし、その制御点の位置（`RailEditor::GetTOfControlPoint()`）から始める（落下リスタートと R キーのやり直しも同じ位置から）。Edit に戻った瞬間は `startSequence_->Stop()` を呼び、覚えておいたレールと制御点を選び直す（`RailEditor::SelectPoint()`）。
   2. Play 中は `startSequence_->Update()` にプレイヤーの座標を渡して呼び、`isGameplayActive`（Play 中かつ開始演出が終わってプレイ可能）と `isPlayerMovable`（開始演出のうちスタート台の上を動ける段階）を求める。ゲートを開け始めた瞬間に始める出発の演出（CountdownFlip）は、ここで `Prepare()` と `Begin()` を呼んで始める。
   3. マウスカーソルの表示を切り替える（`UpdateCursorVisibility()`）。
   4. `Player::UpdateRailProgress()` でレールを進め、`Player::UpdateBasePose()` で基準位置・基準向きを求める。
   5. `RailCamera::Update()` でカメラをプレイヤーの後ろ上に置き、照準の入力を反映する（スタート台の上を動ける間も照準を動かせる）。
   6. クリア判定、状態確認用のデバッグウィンドウ（`ShowStatusWindow()`）。
   7. `Player::UpdateTransform()` でプレイヤーをレールの上に立たせ、照準の方向へ向ける。出発の演出中は `DepartureDirector::Update()` でプレイヤーの表示を上書きする。カメラは「タイトル・回り込み（StartSequence）→ 出発の演出 → ゲートを映すカメラ」の優先順で上書きし、最後に画面の揺れを足す。
   8. 敵の更新、敵弾とプレイヤーの当たり判定、ゲームオーバー判定。
   9. `Player::UpdateMovement()` でジャンプ・WASD 移動・着地判定を行う。地面まで落ちたら `ResetPlayState()` を呼ぶ。`isPlayerMovable` の間も同じく動かす。出発台を使う演出なら、出発台の手前の線を越えたら演出を始める。歩いてレールへ乗ったとき・出発の演出がレールへ乗せ終えたときは `StartSequence::NotifyBoarded()` を呼び、`PlayBoardedEffects()` で足元から輪・インクのしぶき・インクの跡を出す。
   10. 射撃（手元からインクのしぶきも出す。スタート台の上では的・敵に当たらない試し撃ち）、弾の移動、的・敵と弾の当たり判定、壊れた的ごとの演出と連鎖数の加算、弾の削除と行列の更新、レティクルの色の更新。
   11. 演出・スピード線・HUD の更新。レールを進んでいる間は、通った区間を塗り、足元から後ろへしぶきを飛ばし、スピード線を出す。
3. デバッグ用のキー操作（F1 / F4 / F5）と、Edit モード中の ImGui パネル（`ShowEditorPanels()`）を処理する。

- 4〜10 のうち入力・移動・当たり判定・勝敗判定は、`isGameplayActive` が true のときだけ動く。Edit 中・開始演出中は表示の更新だけ行う。

#### ゲームの仕様

| 項目 | 内容 |
| --- | --- |
| 射撃 | 左クリックで、プレイヤーの手元（`Player::GetMuzzlePosition()`）から、カメラの前方 30.0 の点（レティクルの先）へ向けて弾を発射する |
| 試し撃ち | スタート台の上（`isPlayerMovable` の間）でも撃てる。この弾は的・敵に当たらず、台に着いたところにインクを塗る |
| 出発の演出 | ImGui の「Departure」で選んだ演出で、スタート台からレールへ移る（Title の Departure を参照） |
| 落下リスタート | どのレールにも乗れずに地面の高さまで落ちたら暗転（`SceneManager::StartTransition()`）を始め、画面を覆っている間に `ResetPlayState()` を呼ぶ。開始演出は挟まない |
| クリア | オンレール中にレールの終端に着いたら CLEAR へ移る。移る前に壊した的の数・総数・最大連鎖数を StageResult に書き込む（的を全部壊すことはクリアの条件にしない） |
| ゲームオーバー | プレイヤーの体力が 0 になったら GAMEOVER へ移る |
| 当たり判定 | すべてモデルの頂点から求めた境界球（`Model::BoundingSphere`）同士の重なりで判定する。プレイヤー・敵本体はワールド行列で変換した球（`Obj3D::GetWorldBoundingSphere()`）、弾・的は現在座標と表示スケールから求めた球を使う |

- レール移動・ジャンプ・体力などプレイヤーの仕様は Player、カメラと照準の仕様は RailCamera、弾の仕様は PlayerBulletManager、的の仕様は TargetManager を参照。

#### 主な関数

| 関数 | 内容 |
| --- | --- |
| `ResetPlayState()` | Player・RailCamera・TargetManager・EnemyManager を初期状態に戻し、弾を消し、アクティブレールを 0 番にする。演出・画面の揺れ・出発の演出も止める |
| `MakeDepartureContext()` | 出発の演出が動かす・参照するもの（DepartureContext）をまとめる |
| `PlayBoardedEffects(position)` | レールに乗って「GO!」を出す瞬間の演出（輪・しぶき・インクの跡） |
| `UpdateGameplay()` | レール進行・カメラ・プレイヤー・敵・弾・的・勝敗判定を、決まった順番で呼び出す |
| `UpdateCursorVisibility(isPlayMode)` | Edit 中はカーソルを常に表示し、Play 中は TAB で表示を切り替える |
| `ShowStatusWindow()` | 「Rail Branch Debug」ウィンドウを表示する（ImGui がある構成のみ） |
| `ShowEditorPanels()` | 「GameScene Debug」「Hierarchy」「Inspector」パネルを表示する（ImGui がある構成のみ） |

#### GlobalVariables の調整項目（グループ `GameScene`）

`resource/GlobalVariables/GameScene.json` に保存され、実行中の ImGui 編集と外部ファイルの書き換えが反映される。
グループは GameScene が作り、各項目は担当するクラスが登録・取得する。

| キー | 初期値 | 登録するクラス | 内容 |
| --- | --- | --- | --- |
| `railSpeed` | 6.0 | Player | レール全体の進行速度 |
| `cameraHeightOffset` | 0.8 | RailCamera | カメラの回転の中心をレールより上に置く量 |
| `cameraDistance` | 3.0 | RailCamera | カメラをプレイヤーの後ろへ離す距離 |
| `cameraFollowSharpness` | 8.0 | RailCamera | レールの向きへカメラが追従する速さ（大きいほど遅れが小さい） |
| `mouseSensitivity` | 0.0004 | RailCamera | マウス照準の感度 |
| `aimPitchLimit` | 0.5 | RailCamera | 照準の上下の可動範囲（ラジアン。左右は制限しない） |
| `aimHitAngle` | 0.09 | TargetManager | レティクルが的を捉えたとみなす角度（ラジアン） |
| `fogDensity` | 1.0 | Ground | 雲の上に重ねる霧の濃さの倍率 |
| `sunElevationDegree` / `sunAzimuthDegree` | 25 / 50 | Sun | 太陽の高さ・方角（度。方角はレール開始地点の進行方向から右回り） |
| `sunIntensity` / `sunColor` | 1.5 / (1, 0.92, 0.8) | Sun | 光の明るさ・色 |
| `fallTransitionStyle` / `clearTransitionStyle` | RailStripes / InkBurst | GameScene | 落下リスタート・クリアへの切り替えの演出（ImGui で選ぶ） |
| `departureStyle` | CountdownFlip | GameScene | 出発の演出（ImGui の「Departure」で選ぶ。DepartureStyle の番号） |

#### ImGui 表示

| ウィンドウ | 表示するモード | 内容 |
| --- | --- | --- |
| Rail Branch Debug | 常時 | レールの本数、アクティブレール、進行度、オンレール状態、敵の体力（EnemyManager が表示）、プレイヤーの体力など |
| GameScene Debug | Edit のみ | タイトルを飛ばす設定、選んだ制御点から始める設定（Start From Selected Point）、レール・的・敵の編集の Undo / Redo / Save All と未保存のエディター名の表示、落下リスタート・クリアへの切り替えの演出の選択と Preview、出発の演出の選択（Departure。次にゲートを開けたときから使われる）、カメラ座標・回転の編集、俯瞰カメラの切り替え（DebugTopCamera が表示）、ライティング |
| Hierarchy | Edit のみ | 的の一覧。クリックで選択できる（TargetManager が表示） |
| Inspector | Edit のみ | 選択中の的の座標・生存フラグの編集（TargetManager が表示） |

### ClearScene（`scenes/ClearScene.h/.cpp`）

レールの終端に到達したときの画面。

- 背景に `resource/circle.png` を緑系の色で表示する。
- 画面の中央に「GAME CLEAR!」と「SPACE でタイトルへ」を描いた画像（`resource/UI/clearTitle.png`、仮素材）を表示する。ImGui を使わないため Release 構成でも表示される。
- 「STAGE CLEAR!」と操作説明を ImGui で表示する（専用のフォント描画が無いため）。
- SPACE で GAME へ戻る（タイトルから始まる）。

### GameOverScene（`scenes/GameOverScene.h/.cpp`）

プレイヤーの体力が 0 になったときの画面。ClearScene と同じ構成。

- 背景に `resource/circle.png` を赤系の色で表示する。
- 画面の中央に「GAME OVER」と「SPACE でタイトルへ」を描いた画像（`resource/UI/gameOverTitle.png`、仮素材）を表示する。ImGui を使わないため Release 構成でも表示される。
- 「GAME OVER」と操作説明を ImGui で表示する。
- SPACE で GAME へ戻る（タイトルから始まる）。

---

## 4. Title（ステージ開始演出）

GameScene の中で「タイトル → カメラの回り込み → スタート台で準備 → 出発ゲートを開ける → レールへ乗り込む → プレイ」を進めるクラス群。
GameScene が直接触るのは StartSequence と DepartureDirector で、残りのクラスはその内部で使う（スタート台 StartPlatform は GameScene が持ち、StartSequence にポインタを渡す）。

出発までの流れは、スプラトゥーン2 オクト・エキスパンションの「駅のホームを歩いて改札を通ると、ステージで GO! が出る」雰囲気を参考にした。改札・駅・ポイントの支払いなどの要素はそのまま使わず、「台の上を歩ける → 目印の上で出発の操作 → 3・2・1 でゲートが開く → レールへ乗り込む → GO!」という流れだけを取り入れた。
スタート台は、最終的に試し撃ち場（チュートリアル的な空間）にする予定。

### StartSequence（`Title/StartSequence.h/.cpp`）

開始演出の段階を管理する。

#### 段階（Phase）

| 段階 | 内容 | 次の段階へ進む条件 |
| --- | --- | --- |
| `None` | 何もしない（Edit モード中） | `Start()` が呼ばれる |
| `Title` | プレイヤーを正面から映し、カメラを左右にゆっくり揺らす。タイトルロゴ（3D モデル）と PRESS SPACE を表示する | SPACE を押す |
| `CameraTurn` | カメラがプレイヤーの背後へ回り込む。タイトル表示は回り込みの進行に合わせて薄くなる | 回り込みが終わる |
| `Standby` | スタート台の上を動ける（出発ゲートは閉じている）。ゲートの前の目印に立つと「SPACE 出発する」を表示する | 目印の上で SPACE を押す |
| `GateOpen` | ゲートのカウントダウン（3・2・1）でランプを1段ずつ点け、0 で遮断棒を上げる。ゲートを映すカメラ（GateCameraDirector）で寄る。プレイヤーは動けない | ゲートが開ききる。ゲートを開け始めた瞬間に始める出発の演出なら、すぐ `Departure` へ |
| `Boarding` | また動けるようにする。開いたゲートをくぐってレールの始点へ歩くと乗る（出発台を使う演出なら、出発台の手前の線を越えると `Departure` へ） | `NotifyBoarded()` / `BeginDeparture()` が呼ばれる |
| `Departure` | 出発の演出でレールへ向かっている（プレイヤーは動けない） | `NotifyBoarded()` が呼ばれる |
| `Playing` | プレイ中。「GO!」を出す | なし |

#### 関数

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*, SpriteCommon*, Input*, StartPlatform*)` | TitleLogo・TitleUI・StageIntroUI を生成し、出発ゲートを開け閉めするスタート台を受け取る |
| `Start(skipTitle)` | `Title` から始める。カメラワーク・タイトル表示・出発までの表示・出発ゲートも初期状態に戻す。`skipTitle` が true ならゲートは閉じたまますぐ `Playing` にする（カウントダウン・ゲートが開く演出は出さない。プレイヤーは GameScene 側でレールの始点に乗せる） |
| `Stop()` | `None` に戻す |
| `Update(float deltaTime, playerPosition)` | 段階ごとの処理と、次の段階への切り替えを行う。プレイヤーの座標はゲートの前にいるかの判定に使う |
| `NotifyBoarded()` | `Boarding` / `Departure` 中にプレイヤーがレールに乗ったら呼ぶ。「GO!」を出して `Playing` にする |
| `BeginDeparture()` | 出発の演出を始めたら呼ぶ。`Boarding` / `GateOpen` なら `Departure` にする（`GateOpen` からのときはゲートを映すカメラを止める） |
| `IsGateOpenTriggered()` | このフレームで目印の上で SPACE を押してゲートを開け始めたか |
| `IsOpeningGate()` / `IsDeparting()` | `GateOpen` / `Departure` か |
| `IsGateCameraActive()` / `CalculateGateCamera(gameplayCamera)` | ゲートを映すカメラが動いているか / その位置と向き |
| `UpdateLogo(pivot, gameplayRot)` | タイトルロゴをプレイヤーの位置・向きに合わせて配置する |
| `Draw3D()` | `Title` と `CameraTurn` の間だけタイトルロゴを描画する |
| `Draw2D()` | `Title` と `CameraTurn` の間だけ PRESS SPACE と「ESC ゲーム終了」を描画する。Edit 中以外は出発までの表示（StageIntroUI）も描画する。カウントダウンの数はスタート台から受け取って StageIntroUI に渡す |
| `IsPlayable()` | `Playing` なら true。GameScene はこれを見てゲーム処理を動かす |
| `IsControllingCamera()` | `Title` か `CameraTurn` なら true。GameScene はこれを見てカメラを上書きし、HUD を隠す |
| `IsPlayerMovable()` | `Standby` か `Boarding` なら true。GameScene はこれを見てスタート台の上でプレイヤーを動かす |
| `IsBoarding()` | `Boarding` なら true。GameScene はこれを見て、レールに乗った瞬間に `NotifyBoarded()` を呼ぶ |
| `CalculateCamera(pivot, gameplayPos, gameplayRot, outPos, outRot)` | 演出中のカメラ座標・向きを求める（TitleCameraDirector に任せる） |

- スタート・出発とジャンプはどちらも SPACE だが、押した瞬間に次の段階へ移ってプレイヤーを動かさないため、ジャンプしない。
- タイトルからレールに乗り込むまでは、敵・勝敗判定は動かない（射撃は試し撃ちとして、スタート台の上を動ける間だけ使える）。落下リスタートでは開始演出を挟まず、ゲートが開いたままの台の上から再開する。
- ゲートを映す画の位置は、調整項目（グループ `Title` の `gateCameraPosition` / `gateCameraTarget`。ゲートの中央の根元を原点に x: 右、y: 上、z: 前）で変えられる。

### GateCameraDirector（`Title/GateCameraDirector.h/.cpp`）

ゲートを開けるときのカメラワーク。ゲートを開け始めたら 0.5 秒でプレイ用カメラからゲートの斜め前の画へ寄せ、インクが柱を塗り上がる様子と遮断棒が跳ね上がる瞬間を見せる。開ききったら 0.3 秒その画で止めてから、0.6 秒でプレイ用カメラへ戻す。2 つの画を割合で混ぜるため、戻りきるとプレイ用カメラと一致する。

| 関数 | 内容 |
| --- | --- |
| `Reset()` / `BeginShot()` / `BeginReturn()` | 止める / ゲートへ寄せ始める / プレイ用カメラへ戻し始める |
| `Update(float)` / `IsActive()` | 進める / 動いている最中か |
| `Calculate(shotPosition, shotTarget, gameplayCamera)` | 演出中のカメラの位置と向き |

### Departure（`Title/Departure/`）

スタート台からステージのレールへ移るときの出発の演出。1 種類 1 クラス（`BaseDeparture` を継承）で作り、`DepartureDirector` が切り替える。使う演出は ImGui の「GameScene Debug」→「Departure」で選ぶ（調整項目 `departureStyle` に保存。初期値は CountdownFlip）。

| 種類（`DepartureStyle`） | 始まるとき | 内容 |
| --- | --- | --- |
| `Walk` | — | 演出なし。開いたゲートをくぐって自分で歩いてレールに乗る |
| `SuperJump` | 出発台の手前の線を越えたとき | 出発台で溜め、高さ 18 まで打ち上がって空からコースを見下ろし、レールの始点へ降りる。着地でヒットストップ・揺れ・放射ブラー |
| `InkDive` | 同上 | ゲートが開くと出発台にインク溜まりを塗る。頭から飛び込んで沈むと画面をインク（`InkBurst`）で覆い、晴れるとレールの始点から飛び出す |
| `Catapult` | 同上 | レールの始点で 3 回に分けて溜め、速さ 3 倍で飛び出す。視野角を一瞬広げ、放射ブラーをかける |
| `CountdownFlip` | ゲートを開け始めたとき | ゲートの 3・2・1 の合図（ランプが点く瞬間）に合わせて、合図のたびに身を沈めて溜め、カメラを背中へ寄せる。0（遮断棒が跳ね上がる瞬間）で前へ跳びながら前方宙返りを 1 回してレールの始点へ飛び乗り、「GO!」と同時に速さ 3 倍・視野角を広げる・放射ブラー・ヒットストップ |

| クラス | 内容 |
| --- | --- |
| `BaseDeparture` | 演出の基底クラス。`Begin()` / `Update()` / `IsFinished()` / `GetCameraPose()` を実装する。`Prepare()`（ゲートが開いた瞬間の準備）、`StartsWithGate()`（ゲートを開け始めた瞬間に始めるか）、`Reset()` は必要なときだけ上書きする。レールに乗せ終えたら `NotifyBoarded()` を呼ぶ |
| `DepartureContext` | 演出が動かす・参照するもの（Player・RailEditor・StartPlatform・InkEffectManager・ScreenImpact・SceneManager・カメラ）。GameScene が `MakeDepartureContext()` で作る |
| `DepartureDirector` | 種類ごとの演出を 1 つずつ持ち、`Prepare(style, context)` で選んで `Begin()` で始め、`Update()` で進める。`ConsumeBoarded()` でレールに乗せ終えたことを GameScene に知らせる。`UsesDeparturePad()` / `StartsWithGate(style)` で始め方を返す |

- 新しい演出を足すときは、`BaseDeparture` を継承したクラスを作り、`DepartureStyle`（`Count` の前）と `DepartureDirector::kStyleNames`、`DepartureDirector::Initialize()` に 1 行ずつ足す。
- 時間・高さ・カメラの位置は、各演出のヘッダーの定数で調整する。

### TitleCameraDirector（`Title/TitleCameraDirector.h/.cpp`）

タイトル中と回り込み中のカメラワークを計算する。

#### 計算方法

- プレイ用カメラ（レール追従カメラ）の位置を、プレイヤーを中心に Y 軸まわりに回転させて求める。向きのヨーにも同じ角度を足すため、常にプレイ時と同じ構図でプレイヤーを映す。
- タイトル中は回転角を 180°（正面）にし、`sin` で左右に揺らす。カメラとプレイヤーの距離は 0.6 倍にして、少し寄った画にする。
- 回り込み中は、開始した瞬間の角度から 0°（プレイ用カメラ）まで、距離の倍率を 0.6 から 1.0 まで、`Easing::EaseInOutCubic` で補間する。
- 回り込みが終わった時点で、プレイ用カメラと完全に一致する。

#### 関数

| 関数 | 内容 |
| --- | --- |
| `Reset()` | タイトル開始時の状態に戻す |
| `UpdateIdle(float)` | タイトル中の揺れを進める |
| `BeginTurn()` | 現在の揺れの角度から回り込みを始める |
| `UpdateTurn(float)` | 回り込みを進める |
| `IsTurnFinished()` | 回り込みが終わったら true |
| `GetTurnProgress()` | 回り込みの進行度（0〜1、イージング前） |
| `Calculate(...)` | 演出中のカメラ座標・向きを求める |

#### 調整用の定数

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kTurnDuration` | 2.0 | 回り込みにかける時間（秒） |
| `kTitleDistanceScale` | 0.6 | タイトル中のカメラ距離の倍率 |
| `kGameplayDistanceScale` | 1.0 | 回り込み終了時の距離の倍率 |
| `kSwayAmplitude` | 0.08 | タイトル中の揺れ幅（ラジアン、約 5°） |
| `kSwaySpeed` | 0.8 | 揺れの速さ |

### TitleLogo（`Title/TitleLogo.h/.cpp`）

タイトルロゴの 3D モデル（`resource/Title/title.obj`、立体文字）を表示する。

- タイトル中のカメラから見て、プレイヤーの奥の上空に、文字の正面をカメラへ向けて置く。
- プレイ用カメラ（プレイヤーの約 6 後ろ）よりさらに後ろに置くため、カメラが回り込み終わると自然に画面外へ外れる。
- 上下の傾きは使わず、常に直立させる。
- モデルの原点は文字の左下なので、文字列の中心が指定位置に来るようにずらして置く。
- SPACE を押すと、回り込むカメラにかぶらないよう消える。消え方は `logoHideMode` で選び、消え終わるまでの時間は消え方ごとに決まっている（下記 LogoHidePattern）。

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*)` | モデルの読み込みと、調整項目の登録 |
| `Update(pivot, gameplayRotate)` | プレイヤーの位置とプレイ用カメラのヨーを基準に配置する |
| `Draw()` | 描画。消え終わった後は描画しない |
| `Reset()` | 表示状態に戻す |
| `BeginHide()` | 縮めて消す演出を始める |
| `UpdateHide(float)` | 縮めて消す演出を進める |

#### 調整項目（GlobalVariables、グループ `Title`）

Edit モードの Global Variables パネル、または `resource/GlobalVariables/Title.json` の書き換えで変更できる（JSON の書き換えは Play 中も反映される）。

| キー | 初期値 | 内容 |
| --- | --- | --- |
| `logoOffset` | (0, 4, 10) | 文字列の中心を置く位置。プレイヤーから見て x: 右、y: 上、z: 後ろ |
| `logoScale` | 1.5 | 表示スケール |
| `logoHideMode` | 3 | スタート後の消え方の番号（LogoHidePattern の表を参照）。範囲外の番号は JumpAway になる。SPACE を押した時点の値を使う |

### LogoHidePattern（`Title/LogoHidePattern.h/.cpp`）

タイトルロゴの消え方の種類と、その計算。TitleLogo から使う。

- `CalculateLogoHideEffect(type, progress)`：進行度（0〜1）から、ロゴの大きさの倍率・位置のずれ・追加の回転・α 値を求める。
- `GetLogoHideDuration(type)`：消え方ごとの、消え終わるまでの時間。動きの多い消え方ほど長い。
- `ToLogoHideType(int, defaultType)`：設定値の番号を種類に変換する。範囲外の番号は `defaultType`（TitleLogo では JumpAway）になる。

| 番号 | 種類 | 消え方 | 時間 |
| --- | --- | --- | --- |
| 1 | Squash | 縦に潰れて横線になり、横にも縮んで消える | 0.5 秒 |
| 2 | PopOut | 一瞬ふくらんでから、勢いよく縮む | 0.5 秒 |
| 3 | JumpAway | しゃがんで溜めてから、放物線を描いて奥へジャンプしていく（初期値） | 1.1 秒 |

### TitleUI（`Title/TitleUI.h/.cpp`）

タイトル中の 2D 表示。PRESS SPACE は専用の素材が無いため、既存のテクスチャを仮に使う。

| 表示 | テクスチャ | 位置・大きさ | 動き |
| --- | --- | --- | --- |
| PRESS SPACE | `resource/Title/pressSpace.png`（仮） | 画面上から 80% の高さ、360 × 60 | 濃さ 0.2〜1 で点滅＋フェード |
| ESC ゲーム終了 | `resource/UI/quitHint.png`（`tools/generate_help_texture.py` で生成） | 右上の角から 32 内側、340 × 64 | 濃さ 0.85＋フェード |

| 関数 | 内容 |
| --- | --- |
| `Initialize(SpriteCommon*)` | テクスチャの読み込みとスプライトの生成 |
| `Reset()` | 点滅の位相と表示の濃さを初期状態に戻す |
| `Update(float deltaTime, float visibility)` | 点滅を進め、`visibility`（0〜1）を全体の濃さとして掛ける |
| `Draw()` | 描画。濃さが 0 以下なら何もしない |

### StageIntroUI（`Title/StageIntroUI.h/.cpp`）

出発までの 2D 表示。スプライトで描くため、Release 構成でも表示される。
画像は `tools/generate_start_stage_assets.py` で生成する（文言・色はスクリプト先頭の定数で変えられる）。

| 表示 | テクスチャ | 位置・大きさ | 動き |
| --- | --- | --- | --- |
| 操作案内（SPACE 出発する） | `resource/UI/startPrompt.png` | 画面上から 78% の高さ、384 × 96 | ゲートの前に立つとふわっと出て、小さく上下に揺れる |
| カウントダウン（3・2・1） | `resource/UI/count1.png` 〜 `count3.png` | 画面上から 42% の高さ、240 × 240 | 数が変わった瞬間に 2 倍から 0.15 秒で飛び出すように収まり、0.25 秒表示した後、縮みながら 0.15 秒で消える |
| GO! | `resource/UI/go.png` | 画面上から 42% の高さ、480 × 240 | 1.8 倍から 0.25 秒で飛び出すように収まり、0.65 秒表示した後、広がりながら 0.3 秒で消える |

| 関数 | 内容 |
| --- | --- |
| `Initialize(SpriteCommon*)` | テクスチャの読み込みとスプライトの生成 |
| `Reset()` | 何も表示していない状態に戻す |
| `SetPromptVisible(bool)` | 操作案内を出すかどうか |
| `BeginGo()` | GO! を出す |
| `SetCountdown(int)` | ゲートのカウントダウンの数を伝える（毎フレーム呼ぶ。数が変わった瞬間にその数字を出す） |
| `Update(float)` / `Draw()` | 表示の動きを進める / 描画（見えていないものは描かない） |

### Easing（`Engine/Base/Easing.h`）

※ `Engine/` にあるが、Title のために追加したので記載する。

| 関数 | 内容 |
| --- | --- |
| `Easing::EaseInOutCubic(float t)` | ゆっくり動き出し、中盤で加速し、最後にゆっくり止まる 3 次関数 |
| `Easing::EaseInCubic(float t)` | ゆっくり動き出して、だんだん加速する |
| `Easing::EaseOutCubic(float t)` | 勢いよく動き出して、だんだん減速して止まる |
| `Easing::EaseInBack(float t)` | 一度逆方向へ少し戻ってから、加速して進む |
| `Easing::EaseOutBack(float t)` | 勢いよく進んで目標を少し行き過ぎてから、戻って止まる |

---

## 5. objects（ゲームオブジェクト）

### Enemy（`objects/Enemy.h/.cpp`）

雑魚敵。決まった方向に往復し、プレイヤーを見つけると撃ってくる。

#### 動き

| 項目 | 内容 |
| --- | --- |
| 見た目 | `EnemyBot/enemyBot.obj`（4本脚・一つ目・頭に砲台のロボット、赤色）、スケール 0.4 |
| 往復移動 | 基準座標を中心に、往復方向へ片側 5.0 まで、速度 3.0 で往復する |
| 検知 | プレイヤーとの距離が 25.0 以内なら検知中とし、プレイヤーの方を向く。検知していない間は進行方向を向く |
| 攻撃 | 検知中だけ撃つ。1 回撃つごとに、単発 → 3 方向拡散 → 追尾弾 の順で切り替える |
| 体力 | 最大値は配置エディターで敵ごとに設定する（下限 1）。0 になると撃破され、描画されなくなる |
| 撃破後 | 発射済みの弾は消えずに飛び続ける |

#### 攻撃の種類

| 種類 | 内容 | 弾速 | 発射間隔 |
| --- | --- | --- | --- |
| 単発（Single） | 発射した瞬間のプレイヤー方向へ直進する | 18.0 | 1.2 秒 |
| 3 方向拡散（Spread3） | プレイヤー方向を中心に、左右へ約 12° ずつ開いた 3 発を同時に撃つ | 18.0 | 2.0 秒 |
| 追尾弾（Homing） | 発射後も毎フレーム、プレイヤー方向へ少しずつ向きを変える | 12.0 | 2.5 秒 |

- 弾（赤色）は初期化時に 16 発まとめて生成し、使い回す。寿命は 4 秒。
- 弾は敵の中心より 0.5 上から発射する。

#### 関数

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*, basePosition, patrolDirection, maxHp)` | モデルと弾 16 発の生成、初期状態への設定 |
| `Update(playerPosition, deltaTime)` | 弾の更新、往復移動、検知、発射、向きの更新。`deltaTime` が 0 なら動かずに表示だけ更新する |
| `Draw()` | 本体（生存中のみ）と、発射中の弾を描画する |
| `Reset()` | 位置・向き・体力・弾・攻撃の順番を初期状態に戻す |
| `TakeDamage(int)` | 体力を減らす。この攻撃で撃破されたら true を返す |
| `CheckHitToPlayer(playerSphere)` | 敵弾とプレイヤーの当たり判定。弾の境界球とプレイヤーの境界球が重なったら弾を消し、その数を返す |
| `SetBasePosition` / `SetPatrolDirection` / `SetMaxHp` | 配置エディターの編集を反映する |
| `GetPosition` / `IsAlive` / `GetHp` / `GetMaxHp` | 状態の取得 |
| `GetHitSphere()` | 本体のモデルから求めたワールド座標系の境界球を取得する（当たり判定用） |
| `IsDetectingPlayer` / `GetActiveBulletCount` | デバッグ表示用の取得 |

### Player（`objects/Player.h/.cpp`）

プレイヤー。スタート台の上では WASD・ジャンプで自由に動き、レールに乗ると自動で進む（レール上では WASD で動けず、射撃に専念させる）。体力を持つ。

#### 仕様

| 項目 | 内容 |
| --- | --- |
| 見た目 | `Player/player.obj`（インクタンクを背負いブラスターを構えたキャラクター、黄色）、スケール 0.3 |
| レール移動 | `railT_`（0〜1）を「制御点ごとの Speed × `railSpeed` × 経過時間」で進める。終端（1.0）で止まる。跳ねている間・飛び移っている間は 0.3 倍の速さで進む |
| 表示位置 | 基準位置（オンレール中はレール上の点、オフレール中は自由移動の座標）にそのまま立たせる |
| 体の向き | カメラの左右の向き（照準の方向）。レールが傾いていても体は直立させる |
| 弾を撃ち出す位置 | 表示位置から 0.35 上（胸の高さ）。`GetMuzzlePosition()` で取得する |
| レール上のジャンプ | SPACE でレールに沿って進みながらその場で跳ねる（初速 6.0、重力 9.8）。A / D を押しながら SPACE で、押した方向の並走レール（水平 8.0・高さ 3.0 以内）へ 0.4 秒かけて山なりに飛び移る |
| スタート台の上の移動 | WASD で、カメラ基準の水平方向へ 5.0 で動く。SPACE でジャンプ |
| レールに乗る | 台と重なったレールの始点へ歩く（高さの差 0.5 以内）か、落下中に「水平距離が `kOnRailHorizontalThreshold`（1.0）以内」かつ「前フレームより下へレールの高さを跨いだ」ら乗る |
| 体力 | 最大 3。敵弾が当たると 1 減り、1 秒間は無敵になる。被弾位置に火花パーティクルを出す |

#### 関数

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*, paramGroup)` | モデルの生成と、調整項目 `railSpeed` の登録 |
| `Reset()` | レール先頭・オンレール・体力満タンの状態に戻す |
| `UpdateRailProgress(railEditor, isGameplayActive, deltaTime)` | プレイ可能かつオンレール中、かつ終端未到達のときだけレールを進める |
| `UpdateBasePose(railEditor)` | オンレール / オフレールに応じて、カメラ・プレイヤーの共通の基準位置と基準向きを求める |
| `UpdateTransform(cameraRotation, hopHeight)` | 表示位置と体の向きを求め、モデルの行列を更新する。タイトル中の跳ね（`hopHeight`）は見た目だけに反映する |
| `UpdateInvincible(deltaTime)` | 無敵時間を進める |
| `TakeDamage()` | 無敵中でなければ体力を 1 減らし、無敵時間を始める |
| `StartOnPlatform(startPosition, startRotation)` | スタート台の上にオフレールの状態で立たせる（`Reset()` の後に呼ぶ） |
| `BoardRailStart(startT)` | レールの進行度 startT（省略時は 0 = 始点）にオンレールの状態で乗せる（出発の演出・途中からの Play 用。体力は残す） |
| `SetDirectedPose(position, rotate, scaleRate)` | 出発の演出中に、表示を演出が決めた位置・向き・大きさにする |
| `StartRailBoost(rate, duration)` | レールを進む速さを `rate` 倍にし、`duration` 秒かけて元に戻す |
| `UpdateMovement(input, railEditor, startPlatform, cameraForward, cameraRight, groundHeight, deltaTime)` | ジャンプ・WASD 移動・着地判定。スタート台の上に立たせ続け、出発ゲートが閉じている間はゲートより先へ進めない。地面まで落ちてリスタートが必要になったら true を返す |
| `GetHitSphere()` | モデルから求めたワールド座標系の境界球を取得する（当たり判定用） |
| `GetPosition` / `GetBasePosition` / `GetBaseRotation` | 表示位置・基準位置・基準向きの取得 |
| `GetMuzzlePosition()` | 弾を撃ち出す位置の取得 |
| `HasReachedGoal` / `IsDead` | クリア・ゲームオーバーの判定用 |
| `IsGroundedOnRail()` | オンレールで足がレールに着いているか（跳ねている・飛び移っている途中は false。足元のしぶきに使う） |
| `GetRailT` / `IsOnRail` / `GetFreeVelocityY` / `GetHp` / `GetMaxHp` / `GetInvincibleTimer` | デバッグ表示用の取得 |

### PlayerBulletManager（`objects/PlayerBulletManager.h/.cpp`）

プレイヤーの弾の管理。

| 項目 | 内容 |
| --- | --- |
| 見た目 | `Sphere/sphere.obj`（インクの黄色・ライティングなし）。飛ぶ向きへ伸ばし、太さ 0.15×0.6・長さ 0.15×5.0 の筋にする（当たり判定はスケール 0.15 の球のまま） |
| 弾道 | 速度 40、重力 9.8、寿命 2 秒 |
| ダメージ | 敵 1 体に 1 発あたり 1 |
| 生成 | 発射のたびに `make_unique<Obj3D>` で生成している（使い回しは未対応） |
| 試し撃ち | `canHit` を false にして撃った弾は、的・敵に当たらない |
| 着地 | 着地の処理（`SetLandingHandler()`）を渡しておくと、弾が面に着いたときに半径 0.5 のインクを塗り、弾を消す（GameScene はスタート台に塗り、しぶきを出す） |

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*)` | 弾のモデルを読み込む |
| `Fire(position, direction, canHit)` | 弾を 1 発発射する |
| `SetLandingHandler(handler)` | 弾が面に着いたかを調べてインクを塗る処理を渡す |
| `Move(deltaTime)` | 移動と生存時間の更新 |
| `CheckHit(sphere)` | 指定した境界球に重なる弾を 1 発探し、見つかればその弾を消して true を返す |
| `RemoveDeadBullets()` | 命中・寿命切れの弾をリストから削除する |
| `UpdateTransforms()` | 描画用の行列を更新する |
| `Draw()` / `Clear()` | 描画 / すべての弾を消す |
| `GetDamage()` | 弾 1 発のダメージ量を取得する |

### TargetManager（`objects/TargetManager.h/.cpp`）

的の管理。TargetEditor の配置データを的のリストに反映し、照準判定・弾との当たり判定・描画を行う。

| 項目 | 内容 |
| --- | --- |
| 見た目 | `Bullseye/bullseye.obj`（同心円の模様の円盤）、スケール 0.4（狙えているときは 0.6）。模様の面を常にカメラへ向ける |
| 照準判定 | 「カメラ → 的」の向きとカメラの前方ベクトルのなす角が `aimHitAngle` 以内なら狙えている。的が大きくなり、レティクルの中心ドットが赤くなる |
| 命中 | 弾と的の境界球が重なったら撃破し、火花パーティクルを出す。壊れた位置は `GetDestroyedPositions()` で受け取れる（GameScene が破片・しぶきの演出と連鎖数の加算に使う） |
| 初回の自動配置 | 保存データが無いとき、レール上の 16 か所に左右交互・上下・奥行きをずらして配置する |

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*, railEditor, paramGroup)` | 調整項目 `aimHitAngle` の登録、TargetEditor の生成、初回の自動配置 |
| `UpdateEditor()` | TargetEditor を更新し、個数・座標を的のリストに反映する。個数が変わったときだけ生成・削除する |
| `Update(cameraPosition, cameraForward, bullets)` | 照準判定・弾との当たり判定・行列の更新。いずれかの的を狙えていれば true を返す |
| `Reset()` / `Draw()` | 全ての的を復活させる / 生存している的を描画する |
| `GetDestroyedPositions()` | 直前の `Update()` で壊れた的の座標の一覧を取得する |
| `GetTotalCount()` / `GetDestroyedCount()` | 的の総数 / 壊された的の数を取得する |
| `ShowHierarchy()` / `ShowInspector()` | Hierarchy / Inspector パネルの中身を表示する（ImGui がある構成のみ） |

### EnemyManager（`objects/EnemyManager.h/.cpp`）

雑魚敵の管理。EnemyEditor の配置データを敵のリストに反映し、更新・当たり判定・描画をまとめて行う。

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*, railEditor)` | EnemyEditor の生成。保存データが無いときは、レールの中間地点（t = 0.5）の少し上（1.0）に 1 体置く |
| `UpdateEditor()` | EnemyEditor を更新し、座標・往復方向・体力を敵のリストに反映する。体数が変わったときだけ生成・削除する |
| `Update(playerPosition, deltaTime)` | 全ての敵を更新する |
| `CheckHitToPlayer(playerSphere)` | 全ての敵の弾とプレイヤーの当たり判定。命中した弾の合計数を返す |
| `CheckHitByBullets(bullets)` | プレイヤーの弾と敵本体の当たり判定。当たった敵の体力を減らし、火花パーティクルを出す |
| `Reset()` / `Draw()` | 全ての敵を初期状態に戻す / 描画する |
| `ShowDebugInfo()` | 生存数・弾の数・体ごとの体力を表示する（ImGui がある構成のみ） |

### Ground（`objects/Ground.h/.cpp`）

雲海の地面。継ぎ目なく並べられる雲海のテクスチャを貼った `CloudSea/cloudSea.obj` を X 軸に -90° 回して水平にし、レールの開始地点を基準に 45 枚（奥 8・手前 1・横 5）並べる。1 枚の 1 辺は 20、高さは -3.0 で一定。雲の色をテクスチャのまま見せるため、ライティングは無効にしている。

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*, startPosition, forward)` | 地面タイルを生成する |
| `Update()` / `Draw()` | 行列の更新 / 描画 |
| `GetHeight()` | 地面の高さを取得する（落下リスタートの判定に使う） |

### StartPlatform（`objects/StartPlatform.h/.cpp`）

最初のレールの始点の手前に置くスタート台と、台の上に立てる出発ゲート。プレイヤーは台の上から始まり、ゲートを開けてからレールへ乗る。台・柱・梁はインクで塗れる。

| 部品 | モデル | 内容 |
| --- | --- | --- |
| 台 | `StartPlatform/startPlatformPaint.obj` | 幅 6.0・長さ 11.0・厚み 1.0 の灰色の箱。上面がレールの始点の高さで、前の端がレールの始点より 1.5 先まで重なる。塗りテクスチャは 1024 × 1024 |
| ゲートの柱・梁 | `gatePillarPaint.obj` / `gateBeamPaint.obj` | 上面の中央から 1.4 前に、台の左右の端に高さ 1.4 の柱を立て、上に梁を渡す（台の幅いっぱいをふさぐ）。塗りテクスチャは柱 256、梁 1024 |
| ランプ | `StartPlatform/startPlatform.obj`（-1〜1 の立方体） | 左右の柱の手前側の面に 3 段ずつ。消えているときは暗い青、点くと明るい緑（光源の影響を受けない） |
| 遮断棒 | 同上 | 柱の間をふさぐ黄色の棒。開くと高さ 0.45 から 1.25 まで上がる |
| 足元の目印 | `StartPlatform/ring.obj`（輪の画像を貼った板） | ゲートの 0.9 手前に置く黄色の輪（半径 0.5）。点滅させ、ゲートを開け始めたら消す |
| 出発台の目印 | 同上 | ゲートの 1.6 先に置く黄色の輪（半径 0.7）。出発台を使う演出のときだけ、ゲートが開いてから演出を始めるまで点滅させる |
| 出発の輪 | 同上 | レールに乗り込んだ瞬間に足元から広げる黄色の輪。0.6 秒で半径 0.3 から 2.4 まで広がりながら消える |

- プレイヤーの開始位置は、上面の中央から 3.0 後ろ。
- ゲートを開け始めると、目印から左右の柱へ床の上をインクが走り（0.45 秒）、そこから 3・2・1 のカウントダウン（0.6 秒間隔）の合図のたびに、インクが柱を一段ずつ一気に塗り上がってランプを 1 段点ける。1 の後に柱の上まで塗り上がって梁を塗り、0 の合図で遮断棒が 0.45 秒で跳ね上がる（開け始めてから遮断棒まで 2.5 秒）。
- 塗りは `PaintCanvas`（Engine）の UV 空間ペイントで、塗りたい点をワールド座標の球として予約し、`FlushInk()` で 1 フレーム 1 回テクスチャに描き込む。インクの縁はノイズでぎざぎざにし、法線を塗りの高さから求めて盛り上がって見せる。
- ゲートが閉じている間は、ゲートの 0.25 手前の線より先へ押し戻す（台の外やジャンプ中でも）。この線はレールに乗れる距離（`Player::kOnRailHorizontalThreshold`）に入らない位置にあり、`static_assert` で確かめている。
- 台は Play に入るたび（`ResetPlayState()`）に置き直すため、Edit 中にレールを動かしても反映される。

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*)` | 台・ゲート・目印・輪のモデルの生成 |
| `Place(railStartPosition, railStartForward)` | レールの始点に合わせて台とゲートを置く |
| `ResetGate()` / `BeginOpenGate()` / `IsGateOpened()` | ゲートを閉じる / 開け始める / 開ききったか |
| `GetCountdownNumber()` / `GetCountdownCount()` | 今出しているカウントダウンの数（出していないときは 0） / 最初に出す数（3） |
| `IsBarBurstTriggered()` / `HasBarStarted()` | このフレームで遮断棒が跳ね上がり始めたか / 跳ね上がり始めたか（0 になったか） |
| `GetGateBurstPosition()` / `GateLocalToWorld(local)` | 遮断棒からインクを飛び散らせる位置 / ゲートの根元を原点にした座標をワールド座標に変換 |
| `SetDeparturePadVisible(bool)` / `GetDeparturePadPosition()` / `IsPastDepartureLine(position)` | 出発台の目印を出すか / 出発台の中央 / 出発台の手前の線を越えたか |
| `EmitLaunchRing(position)` | 足元から出発の輪を広げる |
| `PaintInk(position, radius)` / `TryLandInk(from, to, radius, landed)` | 台の上面にインクを塗る / 落ちてくる弾・しぶきが上面に着いたら塗る |
| `ClearInk()` / `FlushInk()` | 塗ったインクを消す / 予約したインクをテクスチャに描き込む |
| `Update(float)` | ランプ・遮断棒・目印・輪の動きを進める |
| `Draw()` / `DrawEffects()` | 台とゲートを描画 / 半透明の目印と輪を描画（3D オブジェクトの最後のほうで呼ぶ） |
| `IsInsideTop(position)` / `GetTopHeight()` | 上面の範囲に入っているか / 上面の高さ（Player が台の上に立たせるのに使う） |
| `IsInGateArea(position)` | ゲートが閉じていて、目印の上（中心から 0.6 以内、上面との高さの差 0.1 以内）に立っているか |
| `ClampBehindClosedGate(position)` | ゲートが閉じている間は、ゲートより先へ進めないよう押し戻した座標を返す |
| `GetSpawnPosition()` | プレイヤーの開始位置 |

### InkEffectManager（`objects/InkEffectManager.h/.cpp`）

インクのしぶき・的の破片・撃破時の閃光の演出。起動時にしぶき 160・破片 48・閃光 8 個の Obj3D を生成して使い回し、空きが無いときは一番古いものを上書きする。

| 種類 | モデル | 動き |
| --- | --- | --- |
| しぶき | `Sphere/sphere.obj`（インクの黄色） | 重力 12 で落ち、飛ぶ向きへ伸びる（最大 3 倍）。寿命の後半で縮んで消える。大きさは撃ったとき 0.02〜0.04、レール走行中 0.025〜0.05、出発の合図 0.035〜0.065 |
| 破片 | `InkShard/shard.obj`（的の緑・オレンジ・白・黒） | 重力 14 で落ちながら回転する。寿命の後半で縮んで消える |
| 閃光 | `Sphere/sphere.obj`（白・ライティングなし） | 0.12 秒で 4 倍に広がりながら透明になる |

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*)` | モデルの読み込みと、使い回す Obj3D の生成 |
| `Reset()` | 出ている演出をすべて消す |
| `EmitTargetBreak(position)` | 的を壊したときの演出（破片 10・しぶき 14・閃光 1） |
| `EmitInkSplash(position, direction, count)` | 指定した向きへ飛び散るしぶき（弾を撃ったときの手元に使う） |
| `EmitRailTrail(position, backward)` | レールを進んでいる間、足元から後ろへ飛ぶしぶき（1 回で 1 粒） |
| `EmitLaunchBurst(position, count)` | 出発の合図に、しぶきを高く打ち上げる（落ちたところにインクの跡が残る） |
| `SetLandingHandler(handler)` | しぶきが面に着いたかを調べてインクを塗る処理を渡す（塗る半径はしぶきの大きさの 6 倍） |
| `Update(deltaTime)` / `Draw()` | 移動・回転・縮小・寿命の更新 / 描画 |

### ComboCounter（`objects/ComboCounter.h/.cpp`）

的を続けて壊したときの連鎖数。壊すたびに 1 増え、2 秒以内に次の的を壊さないと 0 に戻る。先に進む条件にはしない。

| 関数 | 内容 |
| --- | --- |
| `Reset()` | 連鎖数・最大連鎖数を 0 に戻す |
| `AddHit()` | 連鎖数を 1 増やし、途切れるまでの時間を最初からにする |
| `Update(deltaTime)` | 途切れるまでの時間と、増えたときに弾む表示の時間（0.15 秒）を進める |
| `GetCombo` / `GetMaxCombo` / `GetRemainingRate` / `GetPopRate` | 今の連鎖数 / 最大連鎖数 / 途切れるまでの残りの割合 / 弾む表示の残りの割合 |

### StageResult（`objects/StageResult.h`）

クリア画面へ渡すプレイの結果（壊した的の数・的の総数・最大連鎖数）。シーンは切り替えのたびに作り直されるため、GameScene がクリア直前に書き込み、ClearScene が読み出す。

---

## 6. Camera（カメラ）

### RailCamera（`Camera/RailCamera.h/.cpp`）

レール追従カメラ（三人称視点のプレイ用カメラ）。計算した位置・向きをカメラ `"default"` に反映する。

| 項目 | 内容 |
| --- | --- |
| 位置 | プレイヤーの基準位置から `cameraHeightOffset` だけ上の点を中心に、カメラの向きの後ろ側へ `cameraDistance` だけ離した位置。照準を動かすとプレイヤーの周りを回り込む |
| 向き | 基準向きへ `cameraFollowSharpness` で少し遅れて追従させた向き＋マウスの照準オフセット。リセット直後は遅れなしで合わせる |
| 照準 | マウス移動量 × `mouseSensitivity`。左右は `aimYawLimit`、上下は `aimPitchLimit` の範囲に制限する |

| 関数 | 内容 |
| --- | --- |
| `Initialize(paramGroup)` | 調整項目（`cameraHeightOffset` / `cameraDistance` / `cameraFollowSharpness` / `mouseSensitivity` / `aimYawLimit` / `aimPitchLimit`）の登録 |
| `Reset()` | 照準オフセットを 0 に戻し、次の更新で追従の遅れなしにレールの向きへ合わせる |
| `Update(basePosition, baseRotation, isInputEnabled, input, deltaTime)` | 位置・向き・前方ベクトル・右方向ベクトルを計算し、カメラに反映する |
| `GetPosition` / `GetRotation` / `GetForward` / `GetRight` | 計算結果の取得 |

### ScreenImpact（`Camera/ScreenImpact.h/.cpp`）

衝撃の瞬間のヒットストップと画面の揺れ。GameScene が持ち、出発ゲート・出発の演出から使う。

| 関数 | 内容 |
| --- | --- |
| `Trigger(hitStopDuration, shakeAmplitude, shakeDuration)` | ヒットストップと揺れを始める（前の衝撃は上書きする） |
| `Update(float)` / `Reset()` | 進める / 止める |
| `IsHitStopping()` / `IsShaking()` | ヒットストップ中か / 揺れている最中か |
| `GetShakeOffset(right, up)` | カメラに足す揺れのずれ（時間とともに弱まる） |

### CameraPose（`Camera/CameraPose.h`）

カメラの位置と向きの組と、演出のカメラワークで使う計算（`CameraPoseMath`）。

| 関数 | 内容 |
| --- | --- |
| `LookAt(position, target)` / `LookRotation(from, to)` | 注目する点へ向けたカメラ / その向き |
| `Lerp(from, to, t)` / `LerpAngle(from, to, t)` | 2 つの画を混ぜる（角度は近い回り方で補間する） |
| `WorldToScreenUv(position, viewProjection)` | ワールド座標を画面の UV に変換する（放射ブラーの中心に使う） |

- 放射ブラーは `Application::TriggerRadialBlur(center, width, duration)`（Engine）でかけ、時間とともに弱めて元の表示に戻す。

### DebugTopCamera（`Camera/DebugTopCamera.h/.cpp`）

レール全体を真上から見下ろす俯瞰デバッグカメラ（カメラ `"debug_top"`）。

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*)` | カメラを生成し、高さ 30 から真下を向かせる |
| `HandleToggleKey(input, railEditor)` | F1 で切り替える。ON にした瞬間に、制御点全体が映る高さ（半径 × 2.2 ＋ 10）に合わせる |
| `UpdateMove(input)` | 有効な間、WASD で平面移動、QE で高さを移動する（速度 10） |
| `ShowToggleCheckbox(railEditor)` | ImGui の切り替え用チェックボックス（ImGui がある構成のみ） |
| `FocusOnPoint(position)` | 俯瞰カメラを ON にし、今の向きのまま position の手前 8.0 の位置へ寄せる（Rail Editor の Focus 用） |
| `IsEnabled()` | 有効かどうかを取得する |

---

## 7. UI

### Reticle（`UI/Reticle.h/.cpp`）

画面中央固定のレティクル。外枠（`Reticle/reticleOutline.png`）と中心ドット（`Reticle/reticle.png`）の 2 枚構成で、どちらも 48 × 48。

| 関数 | 内容 |
| --- | --- |
| `Initialize(SpriteCommon*)` | スプライトを生成し、画面中央に置く |
| `Update()` | 行列の更新 |
| `SetAiming(bool)` | 狙えているときは中心ドットを赤、それ以外は白にする |
| `Draw()` | スプライト共通の描画前処理を行ってから描画する |

### StageHUD（`UI/StageHUD.h/.cpp`）

プレイ中の画面表示。ImGui を使わずスプライトで描くため、Release 構成でも表示される。Play 中のタイトル・カメラの回り込み以外で表示する。

| 表示 | 位置 | 内容 |
| --- | --- | --- |
| 体力 | 左上 | `UI/lifeIcon.png` を最大体力の数だけ並べ、減った分は暗く半透明にする |
| 壊した的の数 | 右上 | 下地（`UI/panel.png`）の上に標的のアイコンと「壊した数/総数」 |
| 連鎖数 | 壊した的の数の下 | オレンジの吹き出し（`UI/comboBubble.png`）に「+N」。増えた瞬間に弾み、途切れる直前に薄くなる |

- 数字は `UI/digits.png`（0〜9, +, / を 64 × 80 で横一列に並べたもの）から 1 文字ずつ切り出して並べる。

| 関数 | 内容 |
| --- | --- |
| `Initialize(SpriteCommon*)` | テクスチャの読み込みとスプライトの生成 |
| `Update(hp, maxHp, destroyedCount, totalCount, combo)` | 表示内容の更新 |
| `Draw()` | スプライト共通の描画前処理を行ってから描画する |

### SpeedLines（`UI/SpeedLines.h/.cpp`）

画面の中心から外側へ流れる白いスピード線（`UI/speedLine.png`、28 本）。レールを進んでいる間だけ濃くなり、乗っていないときはゆっくり消える。画面の中心付近（半径 280 ピクセル）は自機と照準が見えるよう空けておく。

| 関数 | 内容 |
| --- | --- |
| `Initialize(SpriteCommon*)` | テクスチャの読み込みとスプライトの生成 |
| `Reset()` | 線を置き直し、表示の濃さを 0 に戻す |
| `Update(isActive, deltaTime)` / `Draw()` | 線の移動と表示の濃さの更新 / 描画 |

### HelpOverlay（`UI/HelpOverlay.h/.cpp`）

H キーで開閉する操作説明。操作方法と画面の流れ（どうすれば各画面へ進むか）を 1 枚の画像（`UI/help.png`、1440 × 800）で画面中央に等倍表示し、後ろのゲーム画面を黒の半透明で暗くする。閉じている間は左下に「H あそびかた」の案内（`UI/helpHint.png`）を出す。ImGui を使わずスプライトで描くため、Release 構成でも表示される。

- 画像は `tools/generate_help_texture.py` で生成する（Project フォルダで `python tools/generate_help_texture.py`、Pillow が必要）。操作やキーを変えたときは、スクリプトの `CONTROLS` / `FLOWS` を書き換えて実行し直す。
- GameScene では Play モード中だけ開ける（Edit モードで ImGui の文字入力に H を打っても開かないようにするため）。開いている間は `Update()` の冒頭で戻り、ゲームの進行と入力をすべて止める。
- ClearScene / GameOverScene でも開ける。開いている間は SPACE でタイトルへ戻らない。

| 関数 | 内容 |
| --- | --- |
| `Initialize(SpriteCommon*)` | テクスチャの読み込みとスプライトの生成 |
| `Update(input, isEnabled)` | H キーで開閉する。`isEnabled` が false の間は閉じて、案内も出さない |
| `IsOpen()` | 説明を開いているか |
| `Draw()` | スプライト共通の描画前処理を行ってから描画する |

---

## 8. Editor（配置エディター）

3 つとも Edit モード中だけ動く。Play モード中は `Update()` の冒頭で何もせずに戻る。

### RailEditor（`Editor/RailEditor.h/.cpp`）

レールのデータを持ち、編集もできる。GameScene はこのクラスからレール上の位置・向きを受け取る。

#### レールの仕組み

- レールは複数本持てる。ゲームで使うのは「アクティブなレール」1 本。
- 1 本のレールは制御点（ControlPoint）の配列で、各制御点は次の値を持つ。
  - `position`：座標
  - `rotation`：Z だけをロール（バンク）として使う
  - `speed`：その区間の速度倍率
  - 分岐設定：保存はされるが、今の実行時には読まれない
- 位置は Catmull-Rom スプラインで補間する。
- 向き（ピッチ・ヨー）は進行方向から逆算し、ロールは制御点間を線形補間する。ロールと速度は位置と同じ区間分けで補間し、制御点に設定した値がその制御点を通る位置で効く。
- Edit モードの描画では、アクティブなレールを黄色、それ以外を黒で表示する。
- Play モードの描画では、編集用の目印（制御点の目印・曲線の点列・着地範囲）は出さず、レール本体を管（`RailTube/railTube.obj` を Catmull-Rom の 1 区間あたり 24 本つないだもの、半径 0.05）で描く。Edit 中の曲線の点列も 1 区間あたり 20 点で、どちらもレールを延長して足りなくなった分だけ生成する。インクで塗った区間（`PaintActiveRail(t)` で広げ、`ClearPaint()` で消す）は黄色、それ以外はピンクにする。

#### ImGui パネル「Rail Editor」

| 操作 | 内容 |
| --- | --- |
| レール一覧 | クリックでアクティブなレールを切り替える |
| New Rail | レールを 1 本追加し、アクティブにする |
| Add Control Point | アクティブなレールの末尾に、末尾 2 点と同じ向き・間隔で伸ばした位置へ制御点を追加し、選択する |
| Save / Load | 全レールを JSON に保存する / JSON から読み込む |
| Show Control Point Models / Show Rail Curve | 制御点の目印（`RailMarker/railMarker.obj`、上下に尖った宝石の形） / 曲線の表示切り替え |
| Show Landing Range | 着地できる範囲（レールの左右、緑の点列）の表示切り替え |
| Drag Points In Scene / Keep Height | ON の間、Scene ビュー上で制御点を左ドラッグして動かせる（初期は OFF。レイから 0.5 以内の制御点を全レールから探し、別のレールの点ならそのレールをアクティブにする）。Keep Height が ON（初期値）なら水平な面の上で動かし高さを変えない。OFF なら画面に平行な面の上で動かす。目印を隠しているレールの制御点は掴まない |
| Length / Time | アクティブなレールの全長と、走り切るまでの秒数の目安（制御点の Speed × 調整項目 railSpeed で走ったとき。発射直後の加速・空中で遅くなる分は含まない） |
| 制御点一覧 | クリックで選択し、Position / Rotation / Speed / 分岐設定を編集する。Reset で初期値に戻す。Delete で削除する（最後の 1 点は削除できない）。Insert After で次の制御点との中間に制御点を挿入する（末尾なら延長）。Focus またはダブルクリックで、俯瞰カメラをその制御点へ寄せる。From Start に始点からその制御点までの長さと秒数を出す。一覧は高さ 150 の枠の中だけでスクロールする。選択中の制御点の目印は水色で、他の制御点より大きく（0.2 → 0.35）描く |
| 未保存の表示 | 保存していない変更があるとき、Save の下に「未保存」と出す（TargetEditor・EnemyEditor も同じ） |

#### 主な関数（GameScene から使うもの）

| 関数 | 内容 |
| --- | --- |
| `GetPositionOnRail(t)` | 進行度 t の座標 |
| `GetRotationOnRail(t)` | 進行度 t の向き（ピッチ・ヨー・ロール） |
| `GetForwardOnRail(t)` | 進行度 t の進行方向（正規化済み） |
| `GetSpeedOnRail(t)` | 進行度 t の速度倍率（制御点の Speed を補間したもの） |
| `FindNearestRail(worldPos)` | 全レールから最も近いレール・進行度・距離・最近傍点を求める。粗いサンプリング 200 点のあと、三分探索で 20 回絞り込む |
| `SwitchActiveRail(index)` | アクティブなレールを切り替える |
| `GetControlPointsCenter()` / `GetControlPointsRadius()` | 俯瞰カメラを自動で合わせるための、制御点全体の中心と半径 |
| `SetLandingRangeRadius(radius)` | 着地範囲の表示に使う許容距離を受け取る |
| `ToggleShowControlPointModels()` / `ToggleShowCurve()` | キー操作用の表示切り替え |
| `SaveToJson()` / `LoadFromJson()` | 保存・読み込み。起動時は自動で読み込む |
| `GetTOfControlPoint(index)` / `GetSelectedPointIndex()` / `SelectPoint(rail, point)` | 途中からの Play 用。制御点を通る位置の進行度（両端の制御点は曲線の始点・終点に丸める）/ 選択中の制御点 / Play から戻ったときの選び直し |
| `ConsumeFocusRequest(outPos)` / `SetBaseRailSpeed(speed)` | Focus の操作があれば選択中の制御点の位置を返す / 走り切るまでの秒数の計算に使う railSpeed を受け取る |

### TargetEditor（`Editor/TargetEditor.h/.cpp`）

的の配置データを持ち、Scene ビュー上で編集できる。

#### ImGui パネル「Target Editor」

| 操作 | 内容 |
| --- | --- |
| Placement Mode | ON の間だけ、マウスでのドラッグ移動を受け付ける |
| Add Target | カメラの前方 15.0 の位置に的を追加し、選択する |
| Delete Target | 選択中の的を削除する |
| Save / Load | `resource/target/target.json` への保存・読み込み |
| マウスドラッグ | Scene ビュー上で的を左ドラッグして動かす。レイから 1.0 以内の的を掴み、画面に平行な面の上で動かす |

- 選択中の的の座標は、GameScene の Inspector パネルからも編集できる（`GetTargetPositionRef()`）。
- 起動時に保存データを自動で読み込む。

### EnemyEditor（`Editor/EnemyEditor.h/.cpp`）

敵の配置データを持ち、Scene ビュー上で編集できる。操作感は TargetEditor と同じ。

#### 1 体分のデータ（EnemyPoint）

| 項目 | 内容 |
| --- | --- |
| `position` | 往復移動の中心座標 |
| `patrolDirection` | 往復する方向。長さがほぼ 0 になったら (1, 0, 0) に戻す |
| `maxHp` | 体力の最大値。初期値 3、下限 1 |

#### ImGui パネル「Enemy Editor」

| 操作 | 内容 |
| --- | --- |
| Placement Mode | ON の間だけ、マウスでのドラッグ移動を受け付ける |
| Add Enemy | カメラの前方 15.0 の位置に敵を追加し、選択する |
| Delete Enemy | 選択中の敵を削除する |
| Save / Load | `resource/enemy/enemy.json` への保存・読み込み |
| 敵一覧 | クリックで選択する |
| 選択中の敵 | Position / Patrol Dir / Max HP をボタン UI で編集する |
| マウスドラッグ | TargetEditor と同じ |

- 古い保存データに `patrolDirection` や `maxHp` が無くても、初期値で補って読み込める。

### EditHistory / EditorDocumentManager（`Editor/EditHistory.h`、`Editor/EditorDocumentManager.h`）

レール・的・敵のエディターの Undo / Redo と、保存していない変更の管理（ヘッダーオンリー）。

- 各エディターは `IEditorDocument`（`UndoEdit()` / `RedoEdit()` / `SaveToJson()` / `IsModified()` / `GetDocumentName()`）を実装し、初期化時に `EditorDocumentManager` に登録、破棄時に登録を外す。
- `EditHistory<T>` は確定した状態を履歴に積む（1 エディターあたり最大 100 件）。状態ごとに番号を振り、保存したときの番号と比べて未保存かどうかを判定する。Undo で保存したときの状態まで戻れば未保存の表示も消える。
- 各エディターは毎フレームの最後に、ImGui の操作中・ドラッグ中でなければ今の状態と最後に確定した状態を比べ、変わっていれば 1 回の編集として確定する。Inspector など他の場所からの編集もこれで拾う。Load も 1 回の編集として積むため、Ctrl+Z で読み込む前に戻せる。
- `EditorDocumentManager` は「どのエディターを編集したか」の順番を覚え、Ctrl+Z ではレール・的・敵をまたいで編集した順に戻す。

### EditorPicking（`Editor/EditorPicking.h`）

Scene ビュー上のマウス位置から画面奥へ伸びるレイを求める処理と、レイと平面の交点を求める処理（ヘッダーオンリー）。今は RailEditor が使う（TargetEditor・EnemyEditor は同じ処理をそれぞれのクラスに持っている）。

---

## 9. 保存ファイル一覧

| ファイル | 保存するクラス | 内容 | 反映のしかた |
| --- | --- | --- | --- |
| `resource/rail/rail.json`、`rail1.json`、`rail2.json` … | RailEditor | レールごとの制御点 | 起動時に自動で読み込む。以降は Save / Load ボタン |
| `resource/target/target.json` | TargetEditor | 的の座標 | 同上 |
| `resource/enemy/enemy.json` | EnemyEditor | 敵の座標・往復方向・最大 HP | 同上 |
| `resource/GlobalVariables/GameScene.json` | GlobalVariables（エンジン） | GameScene の調整項目 | 実行中の変更が自動で反映される |
| `resource/GlobalVariables/Title.json` | GlobalVariables（エンジン） | タイトルロゴの位置・大きさ・消え方、BGM・効果音の音量、ゲートを映す画の位置 | 同上 |

---

## 10. 操作キー一覧

### タイトル中（Play モード）

| キー | 内容 |
| --- | --- |
| SPACE | スタート（カメラの回り込みを始める） |
| ESC | ゲームを終了する（どの画面でも使える） |

### スタート台の上（Play モード）

| キー | 内容 |
| --- | --- |
| WASD | 台の上を動く |
| マウス移動 | 周りを見回す |
| 左クリック | 試し撃ち（的・敵には当たらず、台にインクを塗る） |
| SPACE | ジャンプ。出発ゲートの前の目印の上で押すと、ゲートが開く |

### プレイ中

| キー | 内容 |
| --- | --- |
| マウス移動 | 照準 |
| 左クリック | 射撃 |
| SPACE | レールに沿って進みながらその場で跳ねる |
| A / D ＋ SPACE | 押した方向の並走レールへ飛び移る |
| TAB | マウスカーソルの表示切り替え |

### デバッグ（モード問わず）

| キー | 内容 |
| --- | --- |
| F2 | Edit / Play の切り替え |
| R | Play 中にタイトルから最初からやり直す（確認用の一時的な処理） |
| F1 | 俯瞰デバッグカメラの切り替え。ON にした瞬間に、制御点全体が映るよう自動で合わせる |
| WASD / Q E | 俯瞰カメラ中の平面移動 / 高さの移動 |
| F4 | 制御点の目印の表示切り替え |
| F5 | レール曲線の表示切り替え |
| Ctrl+Z / Ctrl+Y（Ctrl+Shift+Z） | Edit 中にレール・的・敵の編集を元に戻す / やり直す |
| Ctrl+S | Edit 中に、保存していない変更があるレール・的・敵をすべて保存する |

### クリア画面・ゲームオーバー画面

| キー | 内容 |
| --- | --- |
| SPACE | GAME へ戻る（タイトルから始まる） |
