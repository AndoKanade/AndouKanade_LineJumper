# 実装記録

日付ごとに、実際に実装・完了した内容を記録する。
先頭の「現在の状況」に、できていること・これからやることをまとめる。

---

## 現在の状況（2026-10-24時点）

### できていること

**プレイヤー・レール**
- 最初のレールの手前にあるスタート台の上から始まり、台の上を WASD・ジャンプで自由に動ける。試し撃ちもでき、台に当たったところにインクが塗られる。
- 出発ゲートの前の目印で SPACE を押すと、3・2・1 のカウントダウンでゲートが開き、選んだ出発の演出でレールへ乗って「GO!」で出発する。
- レールに沿って自動で進み、照準を合わせて弾を撃てる。レール上では SPACE でその場で跳ね、A/D＋SPACE で並走レールへ飛び移れる。
- どのレールにも乗れず地面の高さまで落ちたら、スタート台からリスタートする。チェックポイントを通った後なら、チェックポイントからやり直す（ステージ 1 は Rail 4 の螺旋の入り口）。
- 通ったレールの区間を黄色く塗る。
- 鍵の的を壊すと次のレールが出る。出る前のレールは点線で描き、乗れない。壊し損ねると今のレールの終点で落ちる（Rail 1〜5 に仮の鍵の的を 1 つずつ置いている）。
- Play 中のレールの管はレール 1 本ずつ 1 つのモデルにまとめて描き、描画命令はレール 1 本につき最大 3 回で済む。螺旋のような制御点の多いレールを足しても重くならない。

**敵・的**
- 敵はプレイヤーを検知すると、単発 → 3方向拡散 → 追尾弾の順に攻撃を切り替えて撃ってくる。敵弾は16発を使い回している。
- 敵には体力があり、自機の弾が当たるたびに減って0で撃破される。
- 当たり判定はモデルの頂点から求めた境界球同士で行い、見た目の大きさと判定の大きさを合わせている。

**ゲームの流れ・演出**
- タイトル → カメラの回り込み → スタート台で準備 → 出発ゲートを開ける → 出発の演出 → GO! を、シーンを切り替えずに1つのカメラでつないでいる。
- 出発の演出は 5 種類（Walk / SuperJump / InkDive / Catapult / CountdownFlip）から ImGui で選べる。初期値は CountdownFlip（ゲートの 3・2・1 に合わせて溜め、0 で宙返りしてレールに飛び乗る）。
- スタート台・ゲートの柱と梁は UV 空間ペイントでインクを塗れる。ゲートを開けるときはインクが柱を塗り上がり、遮断棒が跳ね上がる瞬間にヒットストップ・画面の揺れ・放射ブラーをかける。
- 自機の体力は3で、被弾後は1秒間の無敵時間がある。残機は3で、落下・体力0で1つ減ってチェックポイント（通っていなければスタート台）からやり直す。残機が0になるとゲームオーバー画面へ移り、コンティニュー（最後に通ったチェックポイントから再開）かタイトルへ戻るかを選べる。ゴールのレール（今は Rail 5）の終点に着くとクリア画面へ移る。ほかのレールの終点ではレールから外れて落ちる。
- HUD（残機・壊した的の数・連鎖数）、体力を表す画面の縁のインク（ポストプロセス）、H キーで開く操作説明、タイトル画面右上の「ESC ゲーム終了」の案内を、スプライトで表示している（Release 構成でも出る）。
- ESC キーでゲームを終了できる。

**開発環境（エディター）**
- Edit / Play モードを切り替えられる（画面上部のボタンか F2）。
- ImGui の「Skip Title」を ON にすると、タイトル・スタート演出を飛ばしてレールの始点からすぐプレイを始める。
- ImGui の「Start From Selected Point」を ON にすると、Rail Editor で選んだ制御点の位置からプレイを始める。
- レール・的・敵をエディターで配置・ドラッグ移動し、JSON に保存・読み込みできる。Rail Editor では螺旋の制御点をまとめて作れる。
- レール・的・敵の編集は Ctrl+Z / Ctrl+Y で戻せる。保存していない変更は「未保存」と出し、Save All（Ctrl+S）でまとめて保存できる。シーンが切り替わっても保存していない変更は消えない。
- GlobalVariables による調整項目は、実行中の ImGui 編集と外部ファイルの書き換えが反映される。
- Debug / Development / Release（x64）のすべてでビルドが通る（2026-10-24 確認）。

### これからやること

**サウンド・素材**
- 効果音を入れる。今鳴っているのはタイトルの BGM・決定音・ゲートのカウントダウン（3・2・1 と GO!）・カタパルトの溜めと発射音。まだ無いのは、遮断棒が跳ね上がる音・宙返りで跳ぶ音・着地の音・プレイ中の BGM・試し撃ちの発射音と着弾音。素材を見つけて `resource/music/se/` に置いたら、鳴らす処理を作る（音量は調整項目 seVolume を使う）。
- `resource/music/se/` の `cancel.mp3`・`move.mp3`・`catapult_preview.mp3` は、今はどこからも鳴らしていない。
- Release 構成で動かしての確認はまだしていない（ビルドが通ることだけ確認済み）。
- 操作説明の画像（`help.png`）に ESC でゲーム終了を載せていない。レール上の操作も今のジャンプの仕様と合っているか見直す。

**スタート演出**
- 0 の瞬間にゲートの遮断棒から飛び散るしぶきが、CountdownFlip の宙返りに少しかぶる。気になるなら、この演出のときだけ減らすか左右に散らす。
- スタート台を試し撃ち場（チュートリアル的な空間）にする。照準の上下の可動範囲（0.5 ラジアン）のままだと、足元の床を撃ちにくい。

**ゲームシステム**
- ボス戦。
- スコア・タイマーなどのゲーム状態。
- ポーズ機能。
- 敵本体や障害物との接触判定（自機の当たり判定は敵弾との判定のみ）。
- 弾・しぶきとステージの面の判定はスタート台の上面だけ。他の壁・床やレールに当たったら塗る、は未対応。
- 地面の当たり判定（描画のみ）。

**UI・演出**
- 敵の体力の表示（ImGui のデバッグ表示にしか出ていない）。
- クリア・ゲームオーバー画面は仮の画像で、壊した的の数・最大連鎖数などの結果は ImGui でしか表示していない。
- PRESS SPACE、出発ゲートの目印、操作案内・GO!・カウントダウンの数字が仮素材のまま。

**敵の設定**
- 攻撃の種類が全ての敵で共通のローテーション。
- 体力以外のパラメータ（移動速度・検知範囲・発射間隔など）が `Enemy.h` の定数固定で、敵ごとの設定や実行中の調整に対応していない。

**コードの品質**
- Edit 中の曲線の点列（1 区間あたり 20 個の球）とインクのしぶき・破片は、1 個ずつ別の Obj3D で描いていて描画命令が多い。重くなったら、レールの管と同じく 1 つのモデルにまとめるか、インスタンシングで描く。
- RailEditor の分岐設定 UI と `rail.json` の分岐先データが、実行時には使われないまま残っている。
- ImGui のデバッグウィンドウ名が `"Rail Branch Debug"` のまま。
- `StartSequence::IsIntroRunning()` がどこからも使われていない。
- Play 中の R キーでタイトルからやり直す処理が、確認用の一時的な処理のまま残っている。
- 放射ブラーをかけた後は、ポストエフェクトの種類を Default に戻す。ImGui で別の種類を選んでいても戻ってしまう。

---

## 2026-09-13

### 目標

- 09/08まで: 敵が自機に向かって弾を撃ってくるようにする
- 09/13まで: 自機と敵の弾との当たり判定を実装する
- 余裕があれば: 自機に体力を実装し、弾が当たると体力を減らす

### 1. 敵の弾の発射

対象ファイル: `Game/objects/Enemy.h` / `Game/objects/Enemy.cpp`

- Enemy クラス内に敵弾（`struct Bullet`）を追加した。
- 弾は `Initialize` で最大数ぶん（`kMaxBulletCount = 16`）まとめて生成し、以降は `isAlive` フラグで使い回す方式にした。発射のたびに `make_unique` するのを避けるため。
- プレイヤーを検知している間だけ、`kShotInterval`（1.2秒）ごとに1発発射する。非検知中は発射せず、待ち時間を戻して次の検知直後に即撃ちされないようにした。
- 弾は発射した瞬間のプレイヤー座標へ向かう等速直線運動（追尾なし・重力なし）。発射位置は敵の中心から `kBulletSpawnUpOffset`（0.5）ぶん上。
- 弾は `kBulletLifeTime`（4秒）で未使用に戻り、再利用される。
- 敵が撃破されても、発射済みの弾は飛び続けるようにした（`UpdateBullets` を本体の生存チェックより先に呼ぶ構成）。
- `Reset()` で弾と発射間隔も初期化されるため、Play開始時に前回の弾を持ち越さない。

追加した定数（すべて `Enemy.h` 内の `static constexpr`）:

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kMaxBulletCount` | 16 | 同時に存在できる弾の最大数 |
| `kShotInterval` | 1.2f | 発射間隔（秒） |
| `kBulletSpeed` | 18.0f | 弾の移動速度（1秒あたり） |
| `kBulletLifeTime` | 4.0f | 弾の生存時間（秒） |
| `kBulletScale` | 0.2f | 弾の表示スケール |
| `kBulletSpawnUpOffset` | 0.5f | 発射位置を敵の中心より上へずらす量 |

### 2. 自機と敵弾の当たり判定

対象ファイル: `Game/objects/Enemy.cpp` / `Game/scenes/GameScene.cpp`

- `Enemy::CheckHitToPlayer(playerPosition, playerHitRadius)` を追加。発射済みの弾とプレイヤーの中心間距離で判定し、命中した弾を未使用に戻して命中数を返す。
- GameScene 側では、雑魚敵の更新直後にこの判定を呼ぶようにした。
- 命中した弾は無敵時間中でもその場で消滅させ、すり抜けて後から当たることがないようにした。

### 3. 自機の体力

対象ファイル: `Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

- `playerHp_`（最大 `kPlayerMaxHp_` = 3）と、被弾後の無敵時間 `playerInvincibleTimer_` を追加した。
- 被弾すると体力を1減らし、`kPlayerInvincibleTime_`（1秒）の無敵時間を設定する。同一フレームで複数の弾が当たっても減少は1回分だけ。
- 被弾位置に火花パーティクル（`ParticleManager::EmitSpark`）を発生させ、当たったことが見た目で分かるようにした。
- Play開始時のリセット処理で、体力と無敵時間も初期状態へ戻すようにした。

追加した定数（`GameScene.h`）:

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kPlayerMaxHp_` | 3 | 体力の最大値 |
| `kPlayerHitRadius_` | 1.0f | 敵弾が命中したとみなす距離 |
| `kPlayerInvincibleTime_` | 1.0f | 被弾後の無敵時間（秒） |

### 4. デバッグ表示

対象ファイル: `Game/scenes/GameScene.cpp`

ImGui の "Rail Branch Debug" ウィンドウに以下を追加した。

- `Enemy Bullets`: 発射中の敵弾の数
- `Player HP`: 現在の体力 / 最大体力
- `Player Invincible`: 無敵時間の残り（秒）

### ビルド確認

`MyGameEngine.sln` を Debug / x64 でビルドし、エラー・警告なしで成功することを確認した。

---

## 2026-09-19

### 目標

- 09/16まで: 体力が0になったときにシーンを切り替えて GAMEOVER シーンを作る
- 09/20まで: 敵の配置をエディターで置けるようにする
- 余裕があれば: 敵にも体力を入れる

### 1. ゲームオーバーシーン

対象ファイル: `Game/scenes/GameOverScene.h` / `Game/scenes/GameOverScene.cpp`（新規）、`Game/systems/SceneFactory.cpp`、`Game/scenes/GameScene.cpp`

- ClearScene と同じ構成で GameOverScene を新規作成した。専用素材が無いため、背景は既存の `resource/circle.png` を赤系の色（`kBackgroundColor` = 1.0, 0.3, 0.3, 0.6）で表示している。
- 文字表示は専用のフォント描画機能が無いため、ClearScene と同様に ImGui で "GAME OVER" と操作説明を表示している。
- SPACE キーでタイトルへ戻る。
- SceneFactory に `"GAMEOVER"` の分岐を追加した。
- GameScene 側では、敵弾の被弾処理の直後に判定を追加した。Playモード中に `playerHp_` が0以下になると `sceneManager_->ChangeScene("GAMEOVER")` を呼ぶ。

前回の残課題だった「体力が0になっても何も起きない」状態が解消された。

### 2. 敵の配置エディター

対象ファイル: `Game/Editor/EnemyEditor.h` / `Game/Editor/EnemyEditor.cpp`（新規）、`Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

TargetEditor と同じ操作感になるよう、同じ構成で新規作成した。

- Editモード中のみ動作する（Playモード中は即 return してドラッグ状態も解除する）。
- `Add Enemy`: 現在のカメラ前方 `kSpawnDistance`（15.0）の位置に敵を1体追加する。追加した敵はそのまま選択状態になる。
- Sceneビュー上で敵を左ドラッグして移動できる。掴んだ瞬間のレイ方向を法線とする平面上を動かすため、画面に平行な移動になる。掴める距離は `kPickRadius`（1.0）。
- `Delete Enemy`: 選択中の敵を削除する（未選択のときは押せない）。
- 敵ごとに `Position` / `Patrol Dir`（往復移動の方向） / `Max HP` をボタンUIで編集できる。往復方向が0ベクトルに近くなったときは既定値 (1, 0, 0) に戻す。
- `Save` / `Load` で `resource/enemy/enemy.json` へ保存・再読込できる。保存形式は `position` / `patrolDirection` / `maxHp`。古い保存データに `patrolDirection` や `maxHp` が無くても既定値で補って読めるようにした。
- 配置された敵の一覧を表示し、クリックで選択できるようにした。

GameScene 側の変更:

- 単体の `std::unique_ptr<Enemy> enemy_` を `std::vector<std::unique_ptr<Enemy>> enemies_` に変更した。
- `SyncEnemiesFromEditor()` を追加した。的（`SyncTargetsFromEditor`）と同じ方式で、**体数が変わったときだけ** Enemy の実体を生成・削除し、毎フレームの生成が発生しないようにした。座標・往復方向・最大HPは毎フレーム上書きするので、エディターでの編集が即座に反映される。
- 保存データが無い初回起動時のみ、従来どおりレール中間地点（`kEnemySpawnRailT_` = 0.5）の少し上へ1体を自動配置する。
- Play開始時のリセットで全ての敵を `Reset()` するようにした。
- 敵弾とプレイヤーの当たり判定は全ての敵に対して行い、命中数を合計してから体力を1減らす。複数の敵が同時に撃っていても、当たった弾はすべて消える。

### 3. 敵の体力

対象ファイル: `Game/objects/Enemy.h` / `Game/objects/Enemy.cpp` / `Game/scenes/GameScene.cpp`

- Enemy に `hp_` / `maxHp_` を追加した。`Reset()` で最大値まで回復する。
- `TakeDamage(damage)` を追加した。撃破済みの敵には効果が無く、体力が0になったときだけ `isAlive_` を false にして true を返す。
- `Initialize()` に `maxHp` 引数を追加し、配置エディターからの編集反映用に `SetBasePosition` / `SetPatrolDirection` / `SetMaxHp` を追加した。`SetMaxHp` は `kMinMaxHp`（1）を下回らないよう制限する（0以下だと生成直後に撃破された状態になってしまうため）。
- `Kill()` は体力も0にして、表示と状態が食い違わないようにした。
- GameScene のプレイヤー弾の判定を、1発で撃破する方式から `kBulletDamageToEnemy_`（1）ぶん体力を減らす方式に変更した。当たるたびに火花パーティクルが出て、体力が0になったときだけ撃破される。

追加した定数:

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `Enemy::kDefaultMaxHp` | 3 | 体力の最大値の初期値 |
| `Enemy::kMinMaxHp` | 1 | 最大HPとして設定できる下限 |
| `EnemyEditor::kDefaultMaxHp` | 3 | Add Enemy で追加したときの最大HP |
| `EnemyEditor::kMinMaxHp` | 1 | エディターで設定できる最大HPの下限 |
| `EnemyEditor::kSpawnDistance` | 15.0f | 新規追加時にカメラ前方へ置く距離 |
| `EnemyEditor::kPickRadius` | 1.0f | マウスで敵を掴めるとみなす距離 |
| `GameScene::kBulletDamageToEnemy_` | 1 | プレイヤーの弾1発が敵に与えるダメージ量 |

### 4. エディタパネルのレイアウト調整

対象ファイル: `Engine/Manager/EditorWidgets.h`、`Game/Editor/TargetEditor.cpp`、`Game/Editor/EnemyEditor.cpp`

Enemy Editor を追加した時点で、下段に置くパネルが4枚（GameScene Debug / Target Editor / Enemy Editor / PostProcess Settings）になり、同じ位置に重なって見えなくなる問題があった。

- `Layout` に `bottomCenterLeft` / `bottomCenterRight` を追加し、`ComputeLayout()` で下段を4等分するようにした。右端のパネルだけは割り算の余りを含めた幅にして、下段に隙間ができないようにしている。
- Target Editor を `bottomCenterLeft`、Enemy Editor を `bottomCenterRight` に割り当てた。GameScene Debug（`bottomLeft`）と PostProcess Settings（`bottomRight`）は割り当て名の変更なしで、それぞれ左端・右端に配置される。

### 5. デバッグ表示

対象ファイル: `Game/scenes/GameScene.cpp`

ImGui の "Rail Branch Debug" ウィンドウの敵の表示を、複数体に対応した内容へ変更した。

- `Enemies`: 配置されている体数と生存数
- `Enemy Bullets`: 全ての敵の発射中の弾の合計数
- `Enemy n HP`: 敵ごとの現在HP / 最大HP と検知状態

生存数と弾の合計は、体ごとの表示と同じループでまとめて数えるようにした。

### その他

- `MyGameEngine.vcxproj` / `MyGameEngine.vcxproj.filters` に新規4ファイル（EnemyEditor.h/.cpp、GameOverScene.h/.cpp）を登録した。
- `CLAUDE.md` のロードマップと「現状の課題」を現状に合わせて更新した（1〜4を完了、解決済み項目を 3.3 へ移動）。

### ビルド確認

`MyGameEngine.vcxproj` を Debug / x64 でビルドし、エラー・警告なしで成功することを確認した。

---

## 2026-09-27

### 目標

- 09/24まで: レールから落ちたときにリスタートできるようにする
- 09/27まで: 敵の攻撃を増やす

### 1. レールから落ちたときのリスタート

対象ファイル: `Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

- Play開始時のリセット処理を `ResetPlayState()` として切り出し、Playに入った瞬間とリスタートで共通して使うようにした。処理内容は従来のリセットと同じ（railT_ を0に戻す、レールを0番へ、的を全て復活、弾をクリア、敵を `Reset()`、体力と無敵時間を初期化）。
- オフレール中の落下判定で、どのレールにも着地できないまま一定の高さより下まで落ちたら `ResetPlayState()` を呼び、レール先頭からやり直すようにした。
- 判定の高さは専用の定数を持たず、後述の地面の高さ `kGroundHeight_` をそのまま使っている。地面の高さを変えれば判定も一緒に動く。

チェックポイント（落ちる直前のレール位置）からの復帰にはしていない。今回はレール先頭からのやり直しで実装した。

### 2. 敵の攻撃の追加（3方向拡散ショット・追尾弾）

対象ファイル: `Game/objects/Enemy.h` / `Game/objects/Enemy.cpp`

- `AttackType`（`Single` / `Spread3` / `Homing`）を追加し、プレイヤーを検知して1回撃つごとに `kAttackOrder` の順番で切り替えるようにした。単発 → 3方向拡散 → 追尾弾 → 単発 … とローテーションする。
- **3方向拡散ショット**: プレイヤー方向を中心に、Y軸回転で左右へ `kSpreadAngle` ずつ角度をつけた3発を同時発射する。上下の角度は中央の弾と同じにしている。
- **追尾弾**: 発射後も毎フレーム進行方向をプレイヤー方向へ補間して向きを変える。補間の割合は `kHomingTurnRate * deltaTime`（1.0を超えないよう上限を設定）で、速さは `Bullet::speed` に保持して変えない。避けられる余地を残すため、通常弾より遅い `kHomingBulletSpeed` にした。
- `Bullet` に `speed` と `isHoming` を追加した。`Reset()` で両方とも初期化し、再利用した弾が前回の設定を引きずらないようにした。
- 発射間隔を攻撃の種類ごとに分け、`GetCurrentShotInterval()` で取得するようにした。弾数の多い拡散、避けにくい追尾は間隔を長めにしている。
- 発射処理は `FireCurrentAttack()`（種類ごとの振り分け）と `FireBullet(spawnPosition, direction, speed, isHoming)`（未使用の弾を1発使う）に分けた。弾は従来どおり `Initialize` で16発まとめて生成して使い回している。

追加した定数（すべて `Enemy.h` 内の `static constexpr`）:

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kAttackOrderCount` | 3 | 攻撃を切り替える順番の要素数 |
| `kAttackOrder` | Single, Spread3, Homing | 攻撃を切り替える順番 |
| `kSpreadBulletCount` | 3 | 3方向拡散ショットで同時に発射する弾数 |
| `kSpreadAngle` | 0.21f | 隣の弾との間につける角度（ラジアン、約12度） |
| `kSpreadShotInterval` | 2.0f | 3方向拡散ショットの発射間隔（秒） |
| `kHomingBulletSpeed` | 12.0f | 追尾弾の移動速度（1秒あたり） |
| `kHomingTurnRate` | 1.5f | 追尾弾が1秒あたりに向きを補正する割合 |
| `kHomingShotInterval` | 2.5f | 追尾弾の発射間隔（秒） |

### 3. 簡易的な地面の描画

対象ファイル: `Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

奥行きが分かりにくかったため、`resource/Plane/plane.obj` を使って地面を表現した。

- `CreateGroundTiles()` を追加し、レールの座標を基準に板モデルを格子状に敷き詰める。レールエディターの初期化後に呼ぶ必要がある。
- plane.obj は XY 平面の 2x2 の板（法線 +Z）なので、X軸を -90 度回して法線を上向きにしている。
- レール開始地点（t=0）の進行方向を「奥」として、奥へ 8 枚・手前へ 1 枚・横へ 5 列を並べる。並べる間隔はタイル1枚の1辺の長さそのままにしてあるため、繋ぎ目に隙間ができない。計45枚で 横100 × 奥180 の広さになり、現在のレール（x = -10.2 〜 40.7）は全域が地面の上に収まる。
- 位置・向き・大きさは生成時に決め打ちし、毎フレームはアクティブカメラの同期と行列更新だけ行う。
- テクスチャは plane.mtl の uvChecker がそのまま出るため、マス目で奥行きが分かる。

追加した定数（`GameScene.h`）:

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kGroundTilePlaneSize_` | 2.0f | 板モデル1枚の1辺の長さ |
| `kGroundTileScale_` | 10.0f | 板モデルに掛ける表示スケール（1タイル20x20になる） |
| `kGroundTileCountForward_` | 8 | 奥（進行方向）へ並べるタイル数 |
| `kGroundTileCountBack_` | 1 | 手前へ並べるタイル数 |
| `kGroundTileCountWidth_` | 5 | 横方向へ並べるタイル数 |
| `kGroundHeight_` | -3.0f | 地面を敷くY座標（落下リスタートの判定にも使う） |
| `kGroundRotateX_` | -π/2 | 板を水平にするためのX軸回転 |

### 4. 着地判定の精密化

対象ファイル: `Game/Editor/RailEditor.h` / `Game/Editor/RailEditor.cpp` / `Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

まだレールに着いていないのに乗ってしまう問題があったため、判定方法を変更した。

- 従来は最近傍点までの3D距離が `kOnRailDistanceThreshold_`（2.0）以内かどうかの1条件だった。球状の判定なのでレールの横や上にいるだけで乗ってしまっていた。
- `NearestRailResult` に最近傍点のワールド座標 `position` を追加した。`FindNearestRail()` 内で既に計算している値をそのまま返すため、呼び出し側での再計算は発生しない。
- 着地条件を2つの AND に変更した。
  1. 水平方向（XZ平面）での最近傍点までの距離が `kOnRailHorizontalThreshold_` 以内（レールの真上にいる）
  2. 前フレームはレールより上、今フレームでレールの高さ以下（落下でレール面を跨いだ瞬間）
- 条件2は跨いだ瞬間のみ成立するため、許容距離を広げても「まだ着いてないのに乗る」現象は起きない。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kOnRailHorizontalThreshold_` | 1.0f | 水平方向の許容距離（`kOnRailDistanceThreshold_` = 2.0f を置き換え） |

### 5. レール間分岐移動（矢印キー操作）の削除

対象ファイル: `Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

レールの乗り換えはジャンプしてプレイヤーを移動させ、着地判定で行う方式にしたため、矢印キーでの分岐操作を削除した。

- `hasPendingBranch_` / `pendingBranchTargetRailIndex_` / `pendingBranchTargetPointIndex_` を削除。
- 制御点の分岐設定を調べる分岐検知処理と、LEFT/RIGHT キーでの乗り移り処理を削除。
- クリア判定を `isRailFinished_` のみに変更した（分岐待ちの条件が無くなったため）。
- ImGui のデバッグ表示から Pending Branch / Required Key の行を削除。
- カメラ右方向ベクトル `cameraRight` の計算は、オフレール中の WASD 移動で使っているため残している。

RailEditor 側の分岐設定 UI（Inspector の Branch Target Rail/Point）と `rail.json` への保存は残しているが、実行時には読まれない状態になった。

### 6. レール・弾の色分け

対象ファイル: `Game/Editor/RailEditor.cpp` / `Game/objects/Enemy.cpp` / `Game/scenes/GameScene.cpp`

どのレールに乗っているか、どちらの弾かを見た目で判別できるようにした。

- レールごとの色分けパレット（白・水色・ピンク・黄緑）と分岐先ハイライト色を廃止し、**今乗っているレールを黄色、それ以外を黒**の2色にした。制御点の球と曲線の両方に適用される。着地してアクティブレールが切り替わると、黄色が乗り移り先へ移る。
- **敵の弾を赤、自機の弾を青**にした。マテリアルは `Obj3D` ごとに持っているため、同じ sphere.obj を使っている的やレールの制御点には影響しない。敵弾は初期化時に16発生成する方式なので、色の設定も生成時の1回だけで済む。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `RailEditor.cpp::kActiveRailColor` | 1.0, 0.9, 0.1, 1.0 | 今乗っているレールの色（黄色） |
| `RailEditor.cpp::kInactiveRailColor` | 0.0, 0.0, 0.0, 1.0 | 乗っていないレールの色（黒） |
| `Enemy.cpp::kEnemyBulletColor` | 1.0, 0.2, 0.2, 1.0 | 敵の弾の色（赤） |
| `GameScene.cpp::kPlayerBulletColor` | 0.2, 0.4, 1.0, 1.0 | 自機の弾の色（青） |

### 7. カメラデバッグマーカーの削除

対象ファイル: `Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

不要になったため、カメラの位置・向きを可視化するマーカー一式を削除した。

- `cameraMarker_` / `cameraFacingMarker_` / `showCameraDebugMarkers_` を削除。
- 生成・位置追従・描画、F3 キーでの表示トグル、ImGui の `Show Main Camera Markers` チェックボックスを削除。
- 向きマーカー専用の調整項目だった GlobalVariables の `noseOffset` も登録・取得ごと削除した（`resource/GlobalVariables/GameScene.json` にキーは残っているが参照されない）。
- 球モデルの読み込みは的や弾でも使うため残している。

### 8. コードの整理

対象ファイル: `Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp` / `Game/objects/Enemy.h` / `Game/objects/Enemy.cpp` / `Game/Editor/RailEditor.h` / `Game/Editor/RailEditor.cpp`

処理内容は変えず、呼び出し元の無いコードと古くなったコメントを削除した。

削除した不要なコード:

| 場所 | 削除したもの | 理由 |
| --- | --- | --- |
| `GameScene.h` | `Application* app_` | どこからも参照されていない |
| `GameScene.cpp` | `#include "SoundManager.h"` / `#include "Logger.h"` | このファイルで未使用 |
| `Enemy.h` / `Enemy.cpp` | `Kill()` | 呼び出し元が無い（撃破は `TakeDamage()` 経由のみ） |
| `RailEditor.h` / `.cpp` | `FindNearestTOnRail()` / `GetDistanceToRail()` | 呼び出し元が無い（着地判定は `FindNearestRail()` を使用） |
| `RailEditor.h` / `.cpp` | `GetTFromControlPointIndex()` / `GetBranchAt()` / `GetControlPointPosition()` / `BranchInfo` | 矢印キーの分岐移動を削除したことで呼び出し元が無くなった |

修正した古いコメント:

- 「矢印キーはレール分岐操作に使用するため」→ 矢印キー操作が無くなったため修正。
- ロードマップの番号を参照していた「(優先度1で実装)」を削除。
- 「フェンスや地面等のオブジェクト」→ このシーンにフェンスは無く、地面タイルは毎フレーム `SetCamera()` を呼ぶため誤りだったので修正。
- 機能削除の経緯を説明していたコメントを削除し、現在の挙動の説明だけを残した。
- `Enemy.h` の `Bullet` 構造体の行末コメントの桁揃えを修正。`kShotInterval` / `kBulletSpeed` を「単発」「単発・3方向拡散用」と明示。

### その他

- `CLAUDE.md` の 4.3 を更新した。「変更箇所にはわかりやすい目印をつけること」を、「変更箇所を示す目印コメント（「ここから追加」「ここまで追加」など）は書かないこと。処理の意図を説明するコメントのみ記述する」に変更した。

### ビルド確認

`MyGameEngine.vcxproj` を Debug / x64 でビルドし、エラー・警告なしで成功することを確認した。

---

## 2026-09-30

### 目標

- タイトルシーンの作り込み（CLAUDE.md ロードマップ 7）
- 参考: スプラトゥーン2 オクト・エキスパンションのステージ開始演出
- モデル・リソースが揃っていないため、カメラワーク・タイトルロゴのモデル・仮スプライト・ImGui 表示で表現する

### 1. タイトルをゲームシーン内の開始演出に変更

対象ファイル: `Game/Title/StartSequence.h/.cpp`（新規）、`Game/scenes/GameScene.h/.cpp`、`Game/systems/SceneFactory.h/.cpp`、`Engine/Framework/Application.cpp`、`Game/scenes/ClearScene.cpp`、`Game/scenes/GameOverScene.cpp`

タイトルからゲームへカメラを途切れさせずにつなぐため、タイトルを独立したシーンではなく GameScene の中の開始演出にした。
`SceneManager::ChangeScene` はシーンを破棄・再生成するため、別シーンのままではカメラを連続して動かせないため。

- `StartSequence` を追加し、`Title → CameraTurn → Countdown → Playing` の段階を管理するようにした。Edit モード中は `None`。
- GameScene からは `Update()` を呼び、`IsPlayable()` でプレイ可能かを受け取るだけにした。
- GameScene に `isGameplayActive`（Play 中かつプレイ可能）を追加した。レール進行・照準・ジャンプ・射撃・敵の動き・当たり判定・勝敗判定は、これが true のときだけ動く。
- Play に入った瞬間は `ResetPlayState()` と `StartSequence::Start()` を呼び、Edit に戻った瞬間は `Stop()` を呼ぶ。
- 落下リスタートは従来どおり `ResetPlayState()` だけを呼ぶため、タイトルを挟まずにすぐ再開する。
- スタートと射撃はどちらも SPACE だが、押した瞬間はまだプレイ可能になっていないため弾は出ない。
- タイトル中・回り込み中はレティクルを表示しない。
- 起動時のシーンと、ClearScene / GameOverScene から戻る先を `"GAME"` に変更した。SceneFactory から `"TITLE"` を外した。
- `TitleScene.h/.cpp` を削除した。数字キーでのポストエフェクト切り替えとグリッチ発動のデバッグ処理も一緒に無くなった。

### 2. カメラの回り込み

対象ファイル: `Game/Title/TitleCameraDirector.h/.cpp`（新規）、`Engine/Base/Easing.h`（新規）

- プレイ用カメラ（レール追従カメラ）の位置を、プレイヤーを中心に Y 軸まわりに回転させて求める。向きのヨーにも同じ角度を足すため、常にプレイ時と同じ構図でプレイヤーを映す。回り込み終了時にプレイ用カメラと完全に一致する。
- タイトル中は回転角を 180°（正面）にし、`sin` で左右にゆっくり揺らす。カメラ距離は 0.6 倍にして少し寄った画にする。
- SPACE を押すと、その瞬間の揺れの角度から 0° まで、距離の倍率を 0.6 から 1.0 まで、`EaseInOutCubic` で 2 秒かけて補間する。
- GameScene では、プレイヤーの位置を求めた後に、演出中だけカメラの位置・向きを上書きする。カメラの行列はシーン更新の後にまとめて更新されるため、このフレームの描画に反映される。
- `Easing.h` を新規作成した（`EaseInOutCubic` / `EaseInCubic` / `EaseOutCubic` / `EaseInBack`）。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kTurnDuration` | 2.0f | 回り込みにかける時間（秒） |
| `kTitleDistanceScale` | 0.6f | タイトル中のカメラ距離の倍率 |
| `kGameplayDistanceScale` | 1.0f | 回り込み終了時の距離の倍率 |
| `kSwayAmplitude` | 0.08f | タイトル中の揺れ幅（ラジアン、約 5°） |
| `kSwaySpeed` | 0.8f | 揺れの速さ |

### 3. タイトルロゴ（3D モデル）

対象ファイル: `Game/Title/TitleLogo.h/.cpp`（新規）、`resource/Title/title.obj`（ユーザー追加）

- 立体文字のモデル（「ラインジャンパー」）を、タイトル中のカメラから見てプレイヤーの奥の上空に置く。文字の正面はタイトル中のカメラへ向ける。
- モデルの原点は文字の左下で、読み込み時の左手系変換で X が反転しているため、文字列の中心が指定位置に来るよう原点をずらして置く（`kModelCenterX` = -3.545、`kModelCenterY` = 0.467）。大きさ・回転を変えても文字列の中心が軸になる。
- 位置・大きさ・消え方は GlobalVariables（グループ `Title`、`resource/GlobalVariables/Title.json`）で調整できる。

| キー | 初期値 | 内容 |
| --- | --- | --- |
| `logoOffset` | (0, 4, 10) | 文字列の中心の位置。プレイヤーから見て x: 右、y: 上、z: 後ろ |
| `logoScale` | 1.5 | 表示スケール |
| `logoHideMode` | 3 | スタート後の消え方の番号 |

### 4. タイトルロゴの消え方

対象ファイル: `Game/Title/LogoHidePattern.h/.cpp`（新規）、`Game/Title/TitleLogo.h/.cpp`

最初はカメラの回り込みが終わってから消えていたため、回り込むカメラにロゴがかぶって邪魔になっていた。SPACE を押した瞬間から消し始めるようにした。

- 消え方の計算は `LogoHidePattern` に分け、進行度から「大きさの倍率・位置のずれ・追加の回転・α 値」を返す形にした。TitleLogo はその結果を反映するだけ。
- 何種類か試した結果、次の 3 種類を残した。消え終わるまでの時間は消え方ごとに決めている。

| 番号 | 種類 | 消え方 | 時間 |
| --- | --- | --- | --- |
| 1 | Squash | 縦に潰れて横線になり、横にも縮んで消える | 0.5 秒 |
| 2 | PopOut | 一瞬ふくらんでから、勢いよく縮む | 0.5 秒 |
| 3 | JumpAway | しゃがんで溜めてから、放物線を描いて奥へジャンプしていく（初期値） | 1.1 秒 |

- 範囲外の番号は JumpAway として扱う。
- 消え方は SPACE を押した時点の設定値を取り込み、演出の途中で設定が変わっても崩れないようにした。

### 5. 3D モデルのアルファブレンド

対象ファイル: `Engine/Graphics/3D/Obj3DCommon.cpp`

- ロゴをフェードで消すために、3D モデル用パイプラインで通常のアルファブレンド（描く色 × α + 背景 × (1 - α)）を有効にした。
- 従来はブレンドが無効で、マテリアルの α 値はシェーダーのアルファテスト（0 のとき discard）にしか効いていなかった。
- α 値が 1 のモデルは従来と同じ見た目になる。
- 半透明のモデルも深度を書き込むため、その後ろに後から描くモデルは隠れる。半透明のものは最後に描くのが基本。

### 6. PRESS SPACE とカウントダウン

対象ファイル: `Game/Title/TitleUI.h/.cpp`（新規）、`Game/Title/StartCountdown.h/.cpp`（新規）

- PRESS SPACE は仮テクスチャ（`resource/gradationLine.png`）のスプライトで、濃さ 0.2〜1 で点滅させる。回り込みの進行に合わせて薄くなる。
- カウントダウンは ImGui で画面中央に「3 → 2 → 1 → GO!」を大きく表示する（1 秒ごと、GO は 0.8 秒表示）。GO と同時にプレイ可能になる。

### 7. Release 構成でゲームが始まらない問題の修正

対象ファイル: `Engine/Manager/EditorContext.h`

- Play ボタンは ImGui にしか無く、Release 構成は `USE_IMGUI` が無効なため、起動したまま Edit モードで止まっていた（今回より前からの問題）。
- ImGui が無い構成では、起動時から Play モードにした。これで起動直後にタイトル演出から始まる。

### その他

- `GameClassSpec.md` を新規作成した。Game フォルダの全クラスの役割・関数・定数・操作キー・保存ファイルをまとめた仕様書。
- `MyGameEngine.vcxproj` / `.filters` に新規ファイルを登録し、TitleScene を外した。`Game/Title` フィルターを追加した。

### ビルド確認

`MyGameEngine.sln` を Debug / Development / Release（すべて x64）でビルドし、エラー・警告なしで成功することを確認した。
タイトル → 回り込み → カウントダウン → プレイの流れと、ロゴの消え方は Development 構成の実機で確認済み。

---

## 2026-10-07

### 目標

- 当たり判定を、手で決めた半径からモデルの形に合わせた判定へ変更する
- GameScene.cpp を役割ごとのクラスへ分割する

### 1. モデルから境界球を求める

対象ファイル: `Engine/Graphics/3D/Model.h/.cpp`、`Engine/Graphics/3D/Obj3D.h/.cpp`

従来は判定の半径を定数で決めていたため、モデルの大きさ・表示スケールと連動せず、見た目と判定の大きさがずれていた。
また、判定の中心がモデルの原点（人型モデルでは足元）になっていた。

- `Model::BoundingSphere`（中心・半径）を追加した。球同士の重なりは `IsHit()` で判定する。平方根を避けるため、距離の2乗と半径の和の2乗で比べる。
- モデルの読み込み時に一度だけ、全頂点を包む境界球を求める（`CalculateBoundingSphere()`）。中心は全頂点を包む箱の中心、半径は中心から最も遠い頂点までの距離。
- `Model::GetBoundingSphere(translate, scale)` で、回転を考慮しない簡易版のワールド座標系の球を取得できる。球のモデル（弾・的）向け。
- `Obj3D::GetWorldBoundingSphere()` を追加した。直前の `Update()` で計算したワールド行列で中心を変換し、半径には各軸で最も大きい拡大率を掛ける。

### 2. 当たり判定の置き換え

対象ファイル: `Game/objects/Enemy.h/.cpp`、`Game/scenes/GameScene.h/.cpp`

- 3つの判定（敵弾 → 自機、自機の弾 → 的、自機の弾 → 敵）を、すべて境界球同士の判定にした。弾の大きさも判定に含まれるようになった。
- 自機と敵本体は、`Update()` 済みのワールド行列から求めた球を使う。
- 弾と的は、描画用オブジェクトの行列が判定より後で更新される（発射直後の敵弾は前回使用時の行列が残っている）ため、現在座標と表示スケールから球を求める。
- 的の判定は狙っているときの大きさにも合わせるため、表示スケールの計算を判定より前に移した。
- `kPlayerHitRadius_`（GameScene）、`kBulletHitRadius_`（GameScene）、`kHitRadius` / `GetHitRadius()`（Enemy）を削除した。代わりに `Enemy::GetHitSphere()` を追加し、`Enemy::CheckHitToPlayer` はプレイヤーの境界球を受け取る形にした。

| 判定 | 変更前 | 変更後（計算上の目安） |
| --- | --- | --- |
| 自機の被弾 | 足元中心・半径 1.0 | 体の中心・半径 約0.3 ＋ 敵弾 0.2 |
| 敵への命中 | 足元中心・半径 1.0 | 体の中心・半径 約0.4 ＋ 自機の弾 0.15 |
| 的への命中 | 0.6 | 0.55（狙っているときは 0.75） |

### 3. GameScene のクラス分割

対象ファイル: `Game/scenes/GameScene.h/.cpp`、`Game/objects/Player.h/.cpp`（新規）、`Game/objects/PlayerBulletManager.h/.cpp`（新規）、`Game/objects/TargetManager.h/.cpp`（新規）、`Game/objects/EnemyManager.h/.cpp`（新規）、`Game/objects/Ground.h/.cpp`（新規）、`Game/Camera/RailCamera.h/.cpp`（新規）、`Game/Camera/DebugTopCamera.h/.cpp`（新規）、`Game/UI/Reticle.h/.cpp`（新規）、`MyGameEngine.vcxproj` / `.filters`

GameScene.cpp が約1000行あり、道中作りでさらに処理が増えるため、役割ごとのクラスへ分けた。処理の内容と呼び出す順番は変えていない。GameScene.cpp は約490行になった。

| クラス | 担当 |
| --- | --- |
| Player | レールの進行、ジャンプ・WASD 移動・着地判定、体力・無敵時間、モデルの表示 |
| PlayerBulletManager | 弾の発射・移動・寿命、境界球との当たり判定 |
| TargetManager | TargetEditor との同期、初回の自動配置、照準判定、弾との当たり判定、Hierarchy / Inspector の中身 |
| EnemyManager | EnemyEditor との同期、初回の自動配置、更新、当たり判定、デバッグ表示 |
| Ground | 地面タイルの生成・描画 |
| RailCamera | プレイ用カメラの位置・照準・前方/右方向ベクトル |
| DebugTopCamera | 俯瞰デバッグカメラ（F1 / ImGui での切り替え、WASDQE 移動） |
| Reticle | 画面中央のレティクル |

- GameScene に残したのは、Edit / Play の切り替え、開始演出とのつなぎ、クリア・ゲームオーバー判定、デバッグウィンドウ、各クラスを呼ぶ順番。
- 調整項目のグループ名は `GameScene` のまま。グループは GameScene が作り、各項目は担当クラスが登録・取得する。保存済みの調整値はそのまま使われる。
- 俯瞰カメラを制御点全体に合わせる処理が F1 と ImGui の 2 か所に重複していたため、`DebugTopCamera::FitToRail()` にまとめた。
- 移したコードのうち数値を直接書いていた箇所（レティクルの大きさ・色、的の表示スケール、俯瞰カメラの高さなど）を、名前付きの定数にした。
- `Game/Camera` と `Game/UI` のフィルターを追加した。

### その他

- `GameClassSpec.md` を新しいクラス構成に合わせて更新した（GameScene の節の書き直し、objects への 5 クラスの追加、Camera・UI の章の追加）。

### ビルド確認

`MyGameEngine.sln` を Development / Release（x64）でビルドし、成功することを確認した（Game 側のコードからの警告なし）。実機での当たりやすさと、クラス分割後の動作は未確認。

### 4. スプラトゥーン風の三人称カメラ

対象ファイル: `Game/Camera/RailCamera.h/.cpp`、`Game/objects/Player.h/.cpp`、`Game/scenes/GameScene.cpp`、`resource/GlobalVariables/GameScene.json`

参考動画（スプラトゥーン3 隠しステージのレール区間）に合わせて、カメラとプレイヤーの位置関係を変えた。

| 項目 | 変更前 | 変更後 |
| --- | --- | --- |
| カメラの位置 | レールの上 1.25 | プレイヤーの後ろ 3.0・上 0.8 を中心に、照準に合わせてプレイヤーの周りを回り込む |
| プレイヤーの位置 | カメラの前方 6.0（照準を動かすとレールから離れて振り回される） | レールの上に立つ |
| プレイヤーの向き | レールの向き | 照準の方向（直立） |
| レールの曲がり | カメラがレールの向きにそのまま合わせる | 少し遅れて追従する（`cameraFollowSharpness`） |
| 弾の発射位置 | カメラ | プレイヤーの手元から、レティクルの先（カメラの前方 30.0）へ向けて撃つ |

- 調整項目 `cameraDistance` / `cameraFollowSharpness` を追加し、`cameraHeightOffset` を 1.25 から 0.8 に変えた。

### ビルド確認（4.）

この変更はビルド・実機での確認をしていない。

### 5. 参考動画に寄せた手触り・見た目の調整

対象ファイル: `Game/objects/InkEffectManager.h/.cpp`（新規）、`Game/objects/ComboCounter.h/.cpp`（新規）、`Game/objects/StageResult.h`（新規）、`Game/UI/StageHUD.h/.cpp`（新規）、`Game/UI/SpeedLines.h/.cpp`（新規）、`Game/objects/TargetManager.h/.cpp`、`Game/objects/PlayerBulletManager.h/.cpp`、`Game/objects/Player.cpp`、`Game/Editor/RailEditor.h/.cpp`、`Game/scenes/GameScene.h/.cpp`、`Game/scenes/ClearScene.cpp`、`MyGameEngine.vcxproj` / `.filters`

追加した素材: `resource/Bullseye/`（標的の円盤）、`resource/RailTube/`（レールの管）、`resource/InkShard/`（的の破片）、`resource/UI/`（数字・吹き出し・体力アイコン・下地・標的アイコン・スピード線）

参考動画の「的を全部壊さないと次のレールが出ない」仕様は、撃ち漏らすと先へ進めず遊びにくいため採用しない。的は壊すほど連鎖数が伸びる、得をするものとして扱う。

| 項目 | 内容 |
| --- | --- |
| 的の見た目 | 白い球から、同心円の模様の円盤（常にカメラの方を向く）に変更 |
| 壊したときの演出 | 的の色の破片・インクのしぶき・白い閃光を飛ばす（InkEffectManager） |
| 連鎖数 | 2 秒以内に続けて壊すと「+N」が増える。右上の吹き出しに表示し、増えた瞬間に弾ませる（ComboCounter・StageHUD） |
| HUD | 左上に体力アイコン、右上に「壊した数/総数」。ImGui を使わないので Release 構成でも表示される（StageHUD） |
| クリア画面 | 壊した的の数と最大連鎖数を表示（StageResult） |
| スピード感 | レールを進んでいる間、画面の中心から外へ流れるスピード線と、足元から後ろへ飛ぶインクのしぶきを出す（SpeedLines・InkEffectManager） |
| レール | Play 中は点列ではなく管で描き、通った区間を黄色、まだの区間をピンクにする（RailEditor） |
| 弾 | 青い球から、飛ぶ向きへ伸ばした黄色い筋に変更。撃つと手元からしぶきが飛ぶ |
| 配色 | 自機をインクと同じ黄色に変更 |

- 演出用の 3D オブジェクトは起動時にまとめて生成して使い回す（毎回の生成によるメモリ確保を避ける）。
- エンジンの GPU パーティクルは加算合成で大きさも変えられず、インクのしぶきに向かないため、Obj3D で作った。

### ビルド確認（5.）

この変更は Windows でのビルド・実機での確認をしていない。DirectX 周りを仮のヘッダーで置き換えて clang で構文チェックだけ行い、変更したファイルにエラーが無いことを確認した。

---

## 2026-10-09

### 1. 背景を天球にした

対象ファイル: `resource/Skybox/celestialSphere.dds`（新規）、`tools/generate_celestial_sphere.py`（新規）、`Game/scenes/GameScene.cpp`、`Engine/Graphics/3D/Obj3D.cpp`

- これまでの背景は空を何も描いておらず、画面のクリア色（青）がそのまま見えていた。既存の Skybox は更新だけされていて描画されていなかった。
- 参考映像に合わせ、霧で白んだ水色の空に、霧にかすむ遠景の建造物のシルエット（柱・張り出し・頂上のふた・浮いたかけらを奥・中・手前の3層で重ねる）を描いた天球のキューブマップ画像を生成し、空港の画像（`rostock_laage_airport_4k.dds`）の代わりに Skybox で描画するようにした。3D モデルの環境マップも同じ画像に切り替えた。
- 画像は `tools/generate_celestial_sphere.py` で生成する（Project フォルダで `python tools/generate_celestial_sphere.py`、numpy が必要）。色・建造物の数や高さ・太陽の位置はスクリプト先頭の定数で変えられる。1 面 1024 ピクセルの非圧縮 sRGB で、ミップマップは読み込み時にエンジンが生成する。
- 太陽の方向の光のにじみは、Sun の初期値（高さ 25 度・方角 50 度、レール開始地点の進行方向 +X 基準）で画像に焼き込んでいる。Sun の調整項目を変えても空の明るい場所は動かない。
- 建造物は背景に描き込んでいるため、カメラが移動しても動かない。近くをすれ違う建造物が必要になったら、モデルで配置する。
- Skybox の行列更新は、カメラ更新後の描画時に行うように移した（空が 1 フレーム遅れて回るのを防ぐ）。

### ビルド確認（1.）

この変更は Windows でのビルド・実機での確認をしていない。生成した DDS をキューブマップとして読み直して描き、面のつなぎ目や向きに問題が無いことだけ確認した。

---

## 2026-10-13

### 1. プレイヤー・雑魚敵・レールの制御点のモデルを自作のものに差し替えた

対象ファイル: `resource/Player/`（新規）、`resource/EnemyBot/`（新規）、`resource/RailMarker/`（新規）、`tools/generate_character_models.py`（新規）、`Game/objects/Player.cpp/.h`、`Game/objects/Enemy.cpp`、`Game/Editor/RailEditor.cpp/.h`

- これまでは配布モデル（プレイヤー: `human/walk.gltf`、雑魚敵: `human/sneakWalk.gltf`、制御点: `Sphere/sphere.obj`）を使っていた。円柱・楕円体・箱を組み合わせたローポリモデルを生成して置き換えた。
  - プレイヤー: インクタンクを背負い、両手でブラスターを構えたキャラクター
  - 雑魚敵: 4本脚で歩く、一つ目で頭に砲台を載せた丸いロボット（人型のプレイヤーと形で区別できるようにした）
  - 制御点: 上下に尖った宝石の形（曲線の点列・着地範囲の点は球のまま）
- モデルは `tools/generate_character_models.py` で生成する（Project フォルダで `python tools/generate_character_models.py`、numpy と Pillow が必要）。
- 向きと大きさは差し替え前の人型モデルに合わせた（原点は足元、正面は +Z、身長 1.66）。表示スケール・弾の発射位置の定数は変えていない。
- 体の色はこれまでどおりマテリアルの色で付ける。テクスチャは白・灰・黒の明暗だけを持つパレット画像で、目やバイザーなどの黒い部分は体の色を変えても黒のまま残る。
- 当たり判定の境界球の半径（モデル単位）は、プレイヤーが 0.85 → 0.92、雑魚敵が 0.85 → 1.16 になった。雑魚敵は脚を広げた分だけ見た目と判定が大きくなっている。
- 差し替え前のモデル（`resource/human/`）はどこからも使っていないが、ファイルは残している。

### ビルド確認（1.）

この変更は Windows でのビルド・実機での確認をしていない。生成した OBJ を読み直して簡易的に描画し、形・向き・パレットの塗り分けに問題が無いことだけ確認した。

### 2. Play 中にレールの始点と終点の目印を出す

対象ファイル: `Game/Editor/RailEditor.cpp`

- Play 中はレールの管だけを描いていたため、レールの端が分かりにくかった。各レールの始点と終点に、制御点の目印（宝石の形）を出すようにした。間の制御点の目印は出さない。
- Catmull-Rom の曲線は 2 番目の制御点から最後の 1 つ前の制御点までを通る。0 番目と最後の制御点は曲線の向きを決めるだけでレールに含まれないため、目印は 2 番目と最後の 1 つ前の制御点に置く。
- 目印の行列は管と同じく、Play に入ってから最初の描画で 1 回だけ求める。色は Edit 中と同じく、今乗っているレールを黄色、それ以外を黒にする。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `RailEditor.cpp::kRailStartPointIndex` | 1 | レールの始点になる制御点のインデックス（終点は最後から同じ数だけ手前） |

### 3. スタート台

対象ファイル: `Game/objects/StartPlatform.h/.cpp`（新規）、`resource/StartPlatform/`（新規）、`Game/objects/Player.h/.cpp`、`Game/scenes/GameScene.h/.cpp`、`MyGameEngine.vcxproj` / `.filters`

これまではレールの始点に乗った状態から始まっていた。台の上を自由に動いてからレールに乗る流れにした。

- 最初のレールの始点の手前に、灰色の箱（スタート台）を置く。上面の高さはレールの始点にそろえ、向きはレールの始点の進行方向（上下の成分を除く）に合わせる。
- 台の前の端をレールの始点より `kRailOverlapLength` だけ先まで重ね、前へ歩くだけでレールに乗れるようにした。
- 台は Play に入るたび（`ResetPlayState()`）に置き直すため、Edit 中にレールを動かしても反映される。Edit 中も台は表示される。
- プレイヤーは `Player::StartOnPlatform()` で台の上面の中央にオフレールの状態で置かれる。カメラの基準向きはレールの始点の向き。
- 台の上では WASD で動け、SPACE でジャンプできる。重力で沈んだ分は毎フレーム上面に戻して立たせ続け、台の端から出るとそのまま落ちる。
- 台の上ではレールと同じ高さを歩くため、落下で高さを跨ぐ着地判定が成立しない。そこで、台の上に立っていて、水平方向でレールの真上（`kOnRailHorizontalThreshold` 以内）かつ高さの差が `kPlatformBoardHeightRange` 以内なら乗る条件を足した。
- 開始位置からレールの始点までは 2.5 離れているため、始まった瞬間にレールへ乗ってしまうことはない。
- 落下リスタートでも `ResetPlayState()` から台の上に戻る。
- 台のモデルは -1〜1 の立方体（`startPlatform.obj`、白テクスチャ）で、大きさはスケール、色はマテリアルの色で付ける。見た目は仮。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `StartPlatform::kHalfWidth` | 2.0f | 台の左右の幅の半分 |
| `StartPlatform::kHalfLength` | 4.0f | 台の前後の長さの半分 |
| `StartPlatform::kHalfHeight` | 0.5f | 台の厚みの半分 |
| `StartPlatform::kRailOverlapLength` | 1.5f | 台の前の端をレールの始点より先へ重ねる長さ |
| `StartPlatform.cpp::kPlatformColor` | 0.55, 0.6, 0.68, 1.0 | 台の色（灰色） |
| `Player::kPlatformBoardHeightRange` | 0.5f | 台の上からレールへ歩いて乗れる高さの差の最大値 |

### ビルド確認（2.・3.）

`MyGameEngine.sln` を Debug / x64 でビルドし、成功することを確認した。実機で、Play 中にレールの始点の目印が管の端に出ること、台の上から始まって W で前へ歩くとレールに乗って進み始めることを確認した。そのほかの動作（台の上でのジャンプ・落下リスタートなど）は、ユーザーが一通りデバッグして問題なさそうなことを確認した。

### 4. スタート台からの出発を参考映像の雰囲気に寄せた

対象ファイル: `Game/objects/StartPlatform.h/.cpp`、`Game/Title/StartSequence.h/.cpp`、`Game/Title/StageIntroUI.h/.cpp`（新規）、`Game/objects/Player.h/.cpp`、`Game/scenes/GameScene.h/.cpp`、`Engine/Base/Easing.h`、`tools/generate_start_stage_assets.py`（新規）、`resource/StartPlatform/ring.*`（新規）、`resource/UI/startPrompt.png` / `stageCard.png` / `go.png` / `missionText.png`（新規）、`MyGameEngine.vcxproj` / `.filters`

3. のスタート台は、台の上から歩いてそのままレールに乗るだけだった。参考映像（スプラトゥーン2 オクト・エキスパンションの、駅のホームを歩いて改札を通るとステージで GO! が出る流れ）の雰囲気を取り入れ、出発までに区切りと演出を入れた。改札・駅・ポイントの支払いなどはそのまま使わず、流れと手触りだけを取り入れている。

| 段階 | 内容 |
| --- | --- |
| タイトル・回り込み | 変更なし。プレイヤーは台の上面の中央より 2.0 後ろに立つ（ゲートまで少し歩かせるため） |
| 準備（Standby） | 台の上を 3. と同じ動き方（WASD・ジャンプ・台から落ちたらリスタート）で動ける。マウスで周りを見回せる。台の前寄りに出発ゲートが立っていて、閉じている間はゲートより先へ進めない |
| ゲートを開ける（GateOpen） | ゲートの前の黄色い目印の上で SPACE を押すと、決定音が鳴り、柱のランプが下の段から順に青から緑へ点いて、黄色の遮断棒が上がる。その間、画面上からステージ情報（番号・ステージ名・目標）が下りてくる。プレイヤーは動けない |
| 乗り込み（Boarding） | ステージ情報を引っ込め、また動けるようになる。開いたゲートをくぐって、3. と同じくレールの始点へ歩くとレールに乗る |
| 出発 | レールに乗った瞬間、足元から黄色い輪が広がり、インクのしぶきが飛んで「GO!」が飛び出す。続けて画面上部に目標の文言を出し、レールを進み始める |

- カウントダウン（`StartCountdown`、ImGui で 3, 2, 1, GO）は、出発ゲートと GO! の表示に置き換えたため削除した。出発までの表示はスプライトで描くため、Release 構成でも表示される。
- タイトルからレールに乗り込むまでは、敵・射撃・勝敗判定を止め、レティクルも出さない。
- ゲートの前で SPACE を押した瞬間は出発の操作として扱い、ジャンプはしない（ジャンプ中に目印の上を通っても出発の操作は受け付けない）。
- 落下リスタートでは開始演出を挟まず、台の上から再開する。ゲートは開いたままなので、そのままレールへ歩いて乗れる（GO! は最初の出発のときだけ出す）。
- ImGui の「Skip Title」を ON にしたときは、ゲートを開けた状態ですぐプレイ中になる。
- ゲートは台の幅いっぱいに立て、閉じている間はゲートの手前の線より先へ押し戻す（台の外へ回り込んだりジャンプで飛び越えたりもできない）。押し戻す線はレールの始点から 1.35 離れていて、レールに乗れる距離（`kOnRailHorizontalThreshold` = 1.0）に入らないことを `static_assert` で確かめている。
- ゲートの柱・梁・ランプ・遮断棒は、台と同じ立方体（`startPlatform.obj`）を大きさを変えて使っている。目印と出発の輪は、輪の画像を貼った板（`ring.obj`）。
- `Easing::EaseOutBack` を追加した（ステージ情報・GO! の飛び出す動きに使う）。

参考映像から取り入れたもの・変えたもの

| 参考映像 | このゲーム |
| --- | --- |
| 駅のホームを自由に歩ける | 雲海に浮かぶスタート台の上を動ける |
| 端末を起動するとステージ情報を表示 | ゲートを開けている間にステージ情報を表示（端末は置かず、操作を 1 回にまとめた） |
| 改札にタッチしてポイントを払うとランプが緑になる | 目印の上で SPACE を押すと、柱のランプが下から順に点いて遮断棒が上がる（支払いは無し） |
| 改札を抜けると別の場所へ切り替わり、出現地点で GO! | 切り替えずに、開いたゲートをくぐってそのままレールに乗り込み、乗った瞬間に GO! |
| ピンクのネオン風の GO! と輪 | 自分のインクの黄色で、太い縁取りの GO! と輪 |

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `StartPlatform::kSpawnBackOffset` | 2.0f | 開始位置の、台の上面の中央から後ろへの距離 |
| `StartPlatform::kGateDistance` | 1.4f | ゲートを立てる位置の、台の上面の中央から前への距離 |
| `StartPlatform::kGateWallMargin` | 0.25f | ゲートが閉じている間に近づける、ゲートからの距離 |
| `StartPlatform::kLampInterval` | 0.5f | ランプが 1 段点くまでの間隔（秒）。3 段 |
| `StartPlatform::kBarOpenDuration` | 0.45f | ランプが点き終わってから遮断棒が上がりきるまでの時間（秒） |
| `StartPlatform::kMarkerDistanceFromGate` | 0.9f | 目印の、ゲートから手前への距離 |
| `StartPlatform::kGateAreaRadius` | 0.6f | 出発の操作を受け付ける、目印の中心からの距離 |

画像とモデルは `tools/generate_start_stage_assets.py` で生成する（Project フォルダで `python tools/generate_start_stage_assets.py`、numpy と Pillow が必要）。文字はリポジトリに入っている `resource/fonts/MochiyPopPOne-Regular.ttf` で描いている。ステージ名（仮: 雲海ライン）・目標の文言（仮: 的を撃ちぬいて ゴールをめざせ!）・色はスクリプト先頭の定数で変えられる。

### ビルド確認（4.）

この変更は Windows でのビルド・実機での確認をしていない。DirectX・Windows のヘッダーを仮のものに置き換えて clang で構文チェックだけ行い、Game フォルダの全ての .cpp にエラーが無いことを確認した。ゲートの大きさ・位置、ステージ情報や GO! の出る時間は、実機で見ながら調整が必要。

### 5. 出発ゲートを開けるときのステージ情報の表示をなくした

対象ファイル: `Game/Title/StageIntroUI.h/.cpp`、`Game/Title/StartSequence.cpp`、`tools/generate_start_stage_assets.py`、`resource/UI/stageCard.png`（削除）

- 4. で入れた、ゲートを開けている間に画面上から下りてくるステージ情報（番号・ステージ名・目標）はいらないため削除した。画像と、生成スクリプトの該当部分も消した。
- ゲートのランプが点いて遮断棒が上がる間はこれまでどおり動けない。操作案内・GO!・GO! の後の目標の文言は残している。

### ビルド確認（5.）

この変更は Windows でのビルド・実機での確認をしていない。4. と同じく clang で構文チェックだけ行い、エラーが無いことを確認した。

### 6. GO! の後の目標の文言をなくした

対象ファイル: `Game/Title/StageIntroUI.h/.cpp`、`tools/generate_start_stage_assets.py`、`resource/UI/missionText.png`（削除）

- GO! が消え始めると同時に画面上部に出していた目標の文言（的を撃ちぬいて ゴールをめざせ!）はいらないため削除した。GO! が消え終わった時点で出発までの表示は終わる。

### 7. クリア・ゲームオーバーへの切り替えの演出

対象ファイル: `Game/systems/SceneTransition.h/.cpp`、`Game/systems/SceneManager.h/.cpp`、`Game/scenes/GameScene.cpp`、`tools/generate_transition_assets.py`（新規）、`resource/UI/transition/`（新規）

これまではどの切り替えも黒のフェードだった。ゲームの「自分のインク（黄色）でレールを塗りながら進む」見た目に合わせ、クリアとゲームオーバーで正反対の動きのインクの演出にした。

| 切り替え | 演出 | 内容 |
| --- | --- | --- |
| ゲーム → クリア | `InkBurst` | 自分の黄色いインクのしみが、画面の中心から外へ次々に弾けて広がる（勢い余って少し大きくなってから収まる）。最後に隙間を黄色で塗って覆う。クリア画面に変わったら塗りを消し、しみが少し持ち上がってから下へ垂れ落ちて抜けていく |
| ゲーム → ゲームオーバー | `InkDrip` | 画面が少しずつ暗くなり、上の縁を暗いインクが埋めた後、太さの違うインクが順番ばらばらに、重さで加速しながら垂れてきて覆う。垂れた先にはピンクの縁取りが見える。ゲームオーバー画面に変わったら、覆ったインクがひとかたまりのまま下へ沈んでいく |

- 弾ける（上向き・明るい）と垂れる（下向き・暗い）で、成功と失敗が一目で分かるようにした。
- `SceneManager::ChangeScene()` に演出の種類を渡せるようにした（省略時はこれまでどおり `Fade`）。クリア画面・ゲームオーバー画面からゲームへ戻るときは `Fade` のまま。
- ImGui の「Fall Transition」の選択肢に `InkBurst` / `InkDrip` を足した。Preview でクリア・ゲームオーバーにならなくても見た目を確認できる。
- しみと垂れるインクの先の丸みの画像は `tools/generate_transition_assets.py` で生成する（Project フォルダで `python tools/generate_transition_assets.py`、numpy と Pillow が必要）。しみは、本体の円から先の丸い腕を突き出してなめらかにつなぎ、周りにしずくを飛ばした形を 3 種類作っている。
- 実装の前に、同じ計算を Python で再現して動きのコマ割りを確認し、戻すときの塗りやインクの動きが角ばって見えないよう調整した。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kInkBurstCoverDuration` / `kInkBurstRevealDuration` | 0.9f / 0.7f | クリアの演出の覆う・戻す時間（秒） |
| `kInkFillCoverStart` | 0.8f | しみの隙間を黄色で塗り始める覆い具合 |
| `kDripCoverDuration` / `kDripRevealDuration` | 1.1f / 0.7f | ゲームオーバーの演出の覆う・戻す時間（秒） |
| `kDripCount` | 16 | 垂れるインクの本数 |
| `kDripWidthScales` | 1.3, 0.9, 1.5, 1.0, 1.2, 0.85, 1.4, 1.1 | 垂れるインクの太さの倍率（隣り合う 2 本の和を 2 以上にして隙間を作らない） |
| `kDripTintAlpha` | 0.45f | 垂れている間に画面を暗くする濃さ |

### ビルド確認（6.・7.）

この変更は Windows でのビルド・実機での確認をしていない。DirectX・Windows のヘッダーを仮のものに置き換えて clang で構文チェックだけ行い、Game フォルダの全ての .cpp にエラーが無いことを確認した。

### 8. クリアへの切り替えの演出を ImGui で選べるようにした

対象ファイル: `Game/systems/SceneTransition.h/.cpp`、`Game/scenes/GameScene.h/.cpp`、`tools/generate_transition_assets.py`、`resource/UI/transition/circle.png` / `goal.png`（新規）、`resource/UI/transition/dripTip.png`（`circle.png` に名前を変えた）

落下リスタートの演出と同じく、クリアへ切り替えるときの演出を見比べて選べるようにした。7. の InkBurst に、新しく 3 つの案を足した。

| 演出 | 内容 |
| --- | --- |
| `InkBurst`（初期値） | 7. のまま。黄色いインクのしみが中心から外へ弾けて塗りつぶし、下へ垂れ落ちて抜ける |
| `RailPaint` | 斜めのピンクのレールの線が走り、そのあとを黄色いインクが筆の先のように塗り進んで覆う。プレイ中に通ったレールが黄色く塗られていく様子を画面全体で見せる。戻すときは塗った帯が右へ抜ける |
| `GoalStamp` | 画面を少し暗くし、「GOAL!」を大きい状態から押しつけて揺らす。押した衝撃でしみが飛び、中央から黄色いインクが広がって覆う。戻すときはインクが縮みながら下へ落ちる |
| `CloudRise` | 下の段から雲のかたまりが湧き上がって画面を包む。雲海の上を進むステージから雲を抜けて次の画面へ出るように見せる。戻すときは雲が左右に割れる |

- Edit モードの「GameScene Debug」パネルに「Clear Transition」の切り替えと「Preview Clear Transition」ボタンを足した。選んだ演出は、実際にクリアしたときの切り替えにも使われる。
- 選んだ演出は GameScene が持っているため、クリア画面からゲームへ戻ってシーンが作り直されると初期値（InkBurst）に戻る（Fall Transition と同じ）。
- 7. で「Fall Transition」に足した InkBurst / InkDrip は、クリアの演出を別に選べるようになったため外し、落下リスタートの 4 つだけに戻した。
- 垂れるインクの先に使っていた白い円の画像は、塗り進む帯の先や雲にも使うため `dripTip.png` から `circle.png` に名前を変えた。「GOAL!」の文字の画像（`goal.png`）は同じスクリプトで生成する。
- 3 つの案は、組み込む前に同じ計算を Python で再現して動きを確認した。

### ビルド確認（8.）

この変更は Windows でのビルド・実機での確認をしていない。DirectX・Windows のヘッダーを仮のものに置き換えて clang で構文チェックだけ行い、Game フォルダの全ての .cpp に ImGui がある構成・無い構成どちらでもエラーが無いことを確認した。

### 9. クリア画面・ゲームオーバー画面に画面の名前の画像を出した

対象ファイル: `Game/scenes/ClearScene.h/.cpp`、`Game/scenes/GameOverScene.h/.cpp`、`tools/generate_result_assets.py`（新規）、`resource/UI/clearTitle.png` / `gameOverTitle.png`（新規）

- どちらの画面も丸の画像と ImGui の文字だけで、今どの画面なのか分かりにくく、Release 構成では文字が出なかった。
- 画面の中央に、画面の名前（クリアは黄色の「GAME CLEAR!」、ゲームオーバーはピンクの「GAME OVER」）と「SPACE でタイトルへ」を描いた画像を表示するようにした。仮素材で、`tools/generate_result_assets.py` で生成する（Project フォルダで `python tools/generate_result_assets.py`、Pillow が必要）。
- これまでの丸の背景と ImGui のウィンドウ（結果の表示）はそのまま残している。

### ビルド確認（9.）

この変更は Windows でのビルド・実機での確認をしていない。clang で構文チェックだけ行い、ImGui がある構成・無い構成どちらでもエラーが無いことを確認した。

---

## 2026-10-14

### 1. H キーで開く操作説明を追加した

対象ファイル: `Game/UI/HelpOverlay.h/.cpp`（新規）、`resource/UI/help.png`・`resource/UI/helpHint.png`（新規）、`tools/generate_help_texture.py`（新規）、`Game/scenes/GameScene.h/.cpp`、`Game/scenes/ClearScene.h/.cpp`、`Game/scenes/GameOverScene.h/.cpp`、`MyGameEngine.vcxproj(.filters)`

- Release 版を初めて遊ぶ人が操作も画面の進め方も分からなかったため、H キーで開閉する操作説明を作った。ImGui を使わずスプライトで描くので Release 構成でも表示される。
- 説明には操作（マウス・左クリック・SPACE・A/D+SPACE・WASD・R・TAB・H）と、画面の流れ（タイトル → スタート台 → ゲートの前で SPACE・レールに乗ってプレイ開始、プレイ → クリア / ゲームオーバー / 落下でスタート台からやり直し、クリア・ゲームオーバー → タイトル）とそれぞれの条件を載せた。
- 閉じている間は画面の左下に「H あそびかた」の案内を出し、説明を開けることが分かるようにした。タイトル・プレイ中・クリア画面・ゲームオーバー画面のどこでも開ける（GameScene では Play モード中のみ）。
- 開いている間はゲームの進行と入力を止める（タイトルの SPACE、クリア・ゲームオーバーの SPACE も受け付けない）。
- 説明の画像は文字を描く機能が無いため、リポジトリのフォント（`resource/fonts/MochiyPopPOne-Regular.ttf`）で `tools/generate_help_texture.py` から生成する。

### ビルド確認（1.）

この変更は Windows でのビルド・実機での確認をしていない。`HelpOverlay.cpp` を仮のヘッダーで g++ の構文チェックにかけ、エラーが無いことだけ確認した。

### 2. レール上のジャンプをスプラトゥーン風にした

対象ファイル: `Game/objects/Player.h/.cpp`、`Game/scenes/GameScene.cpp`、`resource/GlobalVariables/GameScene.json`

- レール上では WASD で動けないようにし、射撃に専念させる。
- SPACE でレールを離れず、レールに沿って進みながらその場で跳ねるようにした。A/D を押しながら SPACE で、押した方向の並走レールへ 0.4 秒かけて山なりに飛び移る。
- 跳ねている間・飛び移っている間は、レールを進む速さを 0.3 倍にし、足元のしぶきを出さない。
- レールを進む速さ（`railSpeed`）を 4.0 から 6.0 にした。

---

## 2026-10-19

今週はスタート演出に取り組んだ。スタート台は最終的に試し撃ち場（チュートリアル的な空間）にする予定で、そこからステージへ移る「出発」を見せ場にした。

### 1. インクで塗る仕組み（UV 空間ペイント）

対象ファイル: `Engine/Graphics/Effect/PaintCanvas.h/.cpp`・`PaintManager.h/.cpp`（新規）、`Engine/Graphics/Shaders/Paint/InkPaint.*`（新規）、`Engine/Graphics/Shaders/Obj3D/Object3d.PS.hlsl`、`Engine/Graphics/3D/Obj3D.h/.cpp`・`Obj3DCommon.h/.cpp`、`tools/generate_paint_box_model.py`（新規）、`resource/StartPlatform/*Paint.obj`（新規）

- モデルごとに塗りテクスチャ（PaintCanvas）を持ち、塗りたい点をワールド座標の球として予約しておき、1 フレーム 1 回まとめてテクスチャに描き込む。
- 描き込みは頂点シェーダーで UV をそのまま画面の座標として出し、ピクセルシェーダーで各ピクセルのワールド座標がインクの球に入っているかを調べる（UV 空間ペイント）。UV の島の境目に隙間ができないよう、使える環境では保守的ラスタライズを使う。
- インクの縁は 2 段のノイズでぎざぎざにし、周りに小さなしずくも付ける。描画するときは塗りの値を 0.5 でしきい値にして縁をくっきりさせ、塗りの高さの傾きから法線を求めて盛り上がって見せる（インクの部分だけ光沢を強くする）。
- 塗りテクスチャは解放できないため、PaintManager が名前ごとに使い回す。
- スタート台・ゲートの柱と梁を、塗れるように UV を付けた箱のモデルに差し替えた。

### 2. 試し撃ち

対象ファイル: `Game/objects/PlayerBulletManager.h/.cpp`、`Game/objects/InkEffectManager.h/.cpp`、`Game/objects/StartPlatform.h/.cpp`、`Game/scenes/GameScene.cpp`

- スタート台の上でも撃てるようにした。この弾は的・敵に当たらない。
- 弾・しぶきの移動線分と台の上面の交差を調べ、着いたところにインクを塗る（弾はしぶきも跳ねさせる）。

### 3. 出発ゲートを開ける演出を派手にした

対象ファイル: `Game/Title/GateCameraDirector.h/.cpp`（新規）、`Game/Camera/ScreenImpact.h/.cpp`（新規）、`Game/Camera/CameraPose.h`（新規）、`Engine/Framework/Application.h/.cpp`、`Game/objects/StartPlatform.h/.cpp`、`Game/Title/StartSequence.h/.cpp`、`Game/scenes/GameScene.h/.cpp`

- ゲートを開け始めると、目印から床の上をインクが走って柱を塗り上がり、梁を塗り、遮断棒が跳ね上がる。
- カメラをゲートの斜め前へ寄せて塗り上がる様子を見せ、開ききったらプレイ用カメラへ戻す（GateCameraDirector）。画の位置は調整項目で変えられる。
- 遮断棒が跳ね上がる瞬間に、インクを高く飛び散らせ、ヒットストップ・画面の揺れ（ScreenImpact）・ゲートを中心にした放射ブラー（`Application::TriggerRadialBlur()`）をかける。

### 4. 出発の演出を切り替えられるようにした

対象ファイル: `Game/Title/Departure/`（新規。BaseDeparture・DepartureContext・DepartureDirector・各演出）、`Game/objects/Player.h/.cpp`、`Game/objects/StartPlatform.h/.cpp`、`Game/Title/StartSequence.h/.cpp`、`Game/scenes/GameScene.h/.cpp`、`Engine/Graphics/3D/Camera.h`

- 1 種類 1 クラス（BaseDeparture を継承）で作り、ImGui の「GameScene Debug」→「Departure」で切り替える（調整項目 `departureStyle` に保存）。新しい案をすぐ足して試せるようにした。
- Walk（歩いて乗る）・SuperJump（打ち上がって空から降りる）・InkDive（インクに潜って画面を覆い、レールの始点から飛び出す）・Catapult（レールの始点で溜めて速さ 3 倍で飛び出す）を作った。
- 最初に案に出した「発射レールで登る」は、台とレールの始点が近すぎて登りのレールを置けないため、Catapult に変えた。
- 出発台を使う演出のときは、ゲートの先に出発台の目印を出し、その手前の線を越えたら始める。
- Player に、レールの始点に乗せる・演出中の表示を上書きする・レールを進む速さを一時的に上げる、の 3 つを足した。

### 5. ゲートのカウントダウンと CountdownFlip

対象ファイル: `Game/Title/Departure/CountdownFlipDeparture.h/.cpp`（新規）、`Game/objects/StartPlatform.h/.cpp`、`Game/Title/StageIntroUI.h/.cpp`、`Game/Title/StartSequence.h/.cpp`、`Game/scenes/GameScene.cpp`、`tools/generate_start_stage_assets.py`、`resource/UI/count1.png`〜`count3.png`（新規）

- ゲートの開き方を、どの演出でも 3・2・1 のカウントダウンにした。合図のたびにインクが柱を一段ずつ一気に塗り上がってランプを 1 段点け、画面に数字を出す。0 の合図で遮断棒が跳ね上がる（開け始めてから 2.5 秒）。
- Catapult の手触りが良かったため、それをもとに CountdownFlip を作り、初期値にした。ゲートを開け始めた瞬間に始まり、合図のたびに身を沈めて溜め、0 で前方宙返りを 1 回してレールの始点へ飛び乗る。着地で「GO!」と同時に速さ 3 倍・視野角を広げる・放射ブラー・ヒットストップ。
- 宙返りがしぶきに隠れないよう、跳んでいる前半はカメラを溜めていた位置に残して体を目で追い、後半でプレイ用カメラに追いつかせる。踏み切りと空中のしぶきは出さない。

### 6. しぶきを小さくした

対象ファイル: `Game/objects/InkEffectManager.h`

- しぶきが画面の邪魔になっていたため、撃ったとき・レール走行中・出発の合図のしぶきをおよそ 6 割の大きさにした。台に残る跡の大きさは変えないよう、跡の半径の倍率を 3.5 から 6.0 に上げた。

### 7. 提出用の設定と ESC での終了

対象ファイル: `Game/scenes/GameScene.h/.cpp`、`Engine/Framework/Framework.cpp`、`Game/Title/TitleUI.h/.cpp`、`tools/generate_help_texture.py`、`resource/UI/quitHint.png`（新規）

- 提出用に、レールに乗って 2 秒たつと黒いフェードで暗転してタイトルへ戻るようにした（`kReturnToTitleAfterBoarding` / `kReturnToTitleDelay`）。
- ESC キーでゲームを終了できるようにした。タイトル画面の右上に「ESC ゲーム終了」の案内を、「H あそびかた」と同じ見た目で出す。

### ビルド確認（1.〜7.）

Development 構成（x64）でビルドが通ることを確認した。ゲームを自動で操作して画面を撮り、出発の演出の切り替え（SuperJump・InkDive・Catapult）、ゲートのカウントダウン、CountdownFlip の宙返りと GO!、「ESC ゲーム終了」の表示、ESC で終了することを確認した。6. のしぶきの大きさと 7. のタイトルへ戻る動きは、画面では確認していない。Release 構成は確認していない。

---

## 2026-10-21

### 1. 提出用のタイトルへ戻す処理を消した

対象ファイル: `Game/scenes/GameScene.h/.cpp`

- 提出が終わったため、レールに乗って 2 秒たつとタイトルへ戻る処理（`kReturnToTitleAfterBoarding` / `kReturnToTitleDelay` / `playTimeSinceBoarded_`）を消した。

### 2. Skip Title でスタート演出も飛ばすようにした

対象ファイル: `Game/scenes/GameScene.h/.cpp`、`Game/Title/StartSequence.h/.cpp`

- ImGui の「Skip Title」が ON のときは、Play に入った瞬間（と R キーでやり直したとき）に `Player::BoardRailStart()` でレールの始点に乗せ、スタート台の上を歩かずにすぐプレイを始める。
- スキップ時はゲートを開けないようにし、カウントダウンの音・数字やゲートが開く演出（しぶき・画面の揺れ）を出さない。ゲートはレールの始点より手前にあるため、閉じたままでも通り抜けない。

### 3. 選択中のレールの制御点を見分けやすくした

対象ファイル: `Game/Editor/RailEditor.cpp`

- Edit モードのレールエディターで、一覧で選択中の制御点の目印を水色にし、他の制御点より大きく（0.2 → 0.35）描くようにした。アクティブなレールの黄色と見分けやすくするため。

### 4. 雲海の地面をカメラに合わせて並べ直すようにした

対象ファイル: `Game/objects/Ground.h/.cpp`

- これまではレールの開始地点を基準に 45 枚（奥 8・手前 1・横 5）を固定で並べていたため、レールを伸ばすと地面の外へ出て下に何も無くなっていた。
- タイルをカメラの真下のマスを中心に、前後左右へ 5 枚ずつ（1 層 11×11 = 121 枚、雲と霧 4 層で 605 枚）並べるようにした。カメラの真下のマスが変わったときだけ並べ直す。
- カメラから水平に 100（カメラの遠平面の初期値）以内には必ずタイルがある。枚数が足りているかは `static_assert` で確かめている。
- タイルは必ず格子のマスの中心に置くので、並べ直しても雲の起伏・霧の模様は動いて見えない。

### ビルド確認（1.〜4.）

Development 構成（x64）でビルドが通ることを確認した。画面では確認していない。

### 5. レールを延長しやすくした

対象ファイル: `Game/Editor/RailEditor.h/.cpp`

- 「Add Control Point」で追加する制御点を、原点ではなく末尾 2 点と同じ向き・間隔で伸ばした位置に置くようにした（制御点が 1 つのときは +X へ 5）。回転・速度は末尾の制御点を引き継ぎ、分岐設定は引き継がない。追加した制御点はそのまま選択状態にする。
- 選択中の制御点の編集欄に「Insert After」を追加した。次の制御点との中間（位置・回転・速度を半分ずつ補間）に制御点を挿入して選択する。末尾の制御点で押したときは Add Control Point と同じく延長する。
- 制御点一覧を高さ 150 の子ウィンドウに入れ、制御点が増えても一覧だけがスクロールし、下の編集欄が隠れないようにした。
- Edit 中の曲線の点列と Play 中の管の数を固定（100 点・120 区間）から、Catmull-Rom の区間数に比例する数（1 区間あたり 20 点・24 本）に変えた。延長しても点の間隔が粗くなったり管が角ばったりしないようにするため。今のレール（区間数 5）では今までと同じ数になる。足りない分だけ生成し、多い分は使わないだけにする。

### ビルド確認（5.）

Visual Studio でのビルドはしていない。DirectX まわりのヘッダーを仮のものに差し替えて g++ で構文チェックだけ通した。画面では確認していない。

### 6. Edit モードの作業をしやすくした

対象ファイル: `Game/Editor/RailEditor.h/.cpp`、`Game/Editor/TargetEditor.h/.cpp`、`Game/Editor/EnemyEditor.h/.cpp`、`Game/Editor/EditHistory.h`（新規）、`Game/Editor/EditorDocumentManager.h`（新規）、`Game/Editor/EditorPicking.h`（新規）、`Game/scenes/GameScene.h/.cpp`、`Game/objects/Player.h/.cpp`、`Game/Camera/DebugTopCamera.h/.cpp`、`Engine/Manager/EditorWidgets.h`、`Engine/Base/MyMath.h`、`MyGameEngine.vcxproj(.filters)`

- **途中から Play**：GameScene Debug に「Start From Selected Point」を追加した。ON で Rail Editor の制御点を選んでいれば、Play でタイトル・スタート台を飛ばし、その制御点の位置から始める。落ちたとき・R キーでやり直したときも同じ位置から始める。`Player::BoardRailStart()` に開始位置の進行度を渡せるようにした。
- **Play から戻ったときの選択**：今までは Play に入るとレールが 0 番に戻り、制御点の選択も消えていた。Play 前に編集していたレールと制御点を、Edit に戻ったときに選び直すようにした。
- **Scene ビューで制御点をドラッグ**：Rail Editor に「Drag Points In Scene」（初期 OFF）と「Keep Height」（初期 ON）を追加した。マウスで選ぶ処理は `EditorPicking.h` にまとめた（的・敵のエディターの同じ処理は今回は触っていない）。
- **選んだ制御点にカメラを寄せる**：Rail Editor の「Focus」ボタンと、一覧のダブルクリックで、俯瞰デバッグカメラを今の向きのまま制御点の手前 8.0 へ寄せる（`DebugTopCamera::FocusOnPoint()`）。
- **Undo / Redo**：レール・的・敵の編集を Ctrl+Z / Ctrl+Y（Ctrl+Shift+Z）と GameScene Debug のボタンで戻せるようにした。エディターをまたいで編集した順に戻る。
- **未保存の表示とまとめて保存**：各エディターの Save の下に「未保存」を出し、GameScene Debug に未保存のエディター名と「Save All」（Ctrl+S）を追加した。GlobalVariables（調整項目）は対象外。
- **全長と秒数**：Rail Editor にアクティブなレールの全長と走り切るまでの秒数の目安、選択中の制御点までの長さと秒数を出すようにした。
- **Speed とロールの効く位置を直した**：`GetSpeedOnRail()` とロールの補間が位置と違う区間分け（制御点数 − 1）をしていて、制御点に設定した値が別の場所で効いていた。位置と同じ区間分けにし、制御点を通る位置でその値になるようにした。今のレールは Speed がすべて 1.0、ロールがすべて 0 のため見た目は変わらない。
- 編集の比較用に `Vector3` に `operator==` を追加した。

### ビルド確認（6.）

Visual Studio でのビルドはしていない。DirectX まわりのヘッダーを仮のものに差し替え、RailEditor・TargetEditor・EnemyEditor・DebugTopCamera を g++ で構文チェックした（USE_IMGUI あり・なし）。RailEditor はさらに仮のヘッダーでリンクして、制御点の進行度・Speed の効く位置・Undo / Redo・未保存の判定・延長位置が正しいことを確かめた。GameScene と Player は依存が多いため確認できていない。画面では確認していない。

### 7. レールを編集しているときに画面が一瞬止まるのを直した

対象ファイル: `Game/Editor/RailEditor.h/.cpp`

- 原因：レールの目印の点は 1 つずつ Obj3D で、作るたびに GPU のバッファを 2 つ確保（`CreateCommittedResource`）する。New Rail で着地範囲の点 100 個（バッファ 200 個）、制御点を 1 つ足すたびに曲線の点 20 個（5. のレールの延長で区間数に比例させたため）を、その場でまとめて作っていた。
- 曲線の点列と着地範囲の点を、レールごとに持つのをやめ、全レール共通の置き場から使い回すようにした。起動時に、読み込んだレールに必要な数＋200 個を作っておく。
- 足りなくなったときは 1 フレームに 8 個まで作り、作りきれなかった点は次のフレーム以降に描く（数フレームで全部描かれる）。
- New Rail で作るのは制御点の目印 1 つだけになった。Play 中の管も、起動時に今の区間数の分を作っておく。

### ビルド確認（7.）

Visual Studio でのビルドはしていない。DirectX まわりを仮のヘッダーに差し替えて g++ で構文チェックし、オブジェクトを作った数を数える仮の Obj3D でリンクして確かめた。New Rail で作る数は 101 → 1。制御点を 20 個足したときや Show Landing Range を ON にしたときも、1 フレームに作る数が上限（8 個）を超えないことを確認した。画面では確認していない。

### 8. カメラを動かしながら ImGui を操作すると反応が遅れるのを直した

対象ファイル: `Engine/Base/WinAPI.cpp`、`Engine/Base/TimeManager.cpp`

- 原因 1：`WinAPI::ProcessMessage()` が Windows のメッセージを 1 フレームに 1 つしか処理していなかった。WASD を押し続けたりマウスを動かしたりしてメッセージが多く来ると処理が追いつかず、メッセージから入力を受け取る ImGui の反応が遅れていた（カメラ操作は DirectInput で読むため遅れず、ずれて見えた）。たまっているメッセージを毎フレームすべて処理するようにした。
- 原因 2：`TimeManager` が 60fps に合わせるために `sleep_for` で待つが、Windows のタイマーの精度が初期状態の約 15.6 ミリ秒のままだったため、少しだけ待つつもりでも長く眠り、次のフレームが垂直同期に間に合わずときどき 1 フレーム止まっていた。`Initialize()` で `timeBeginPeriod(1)` を呼び、精度を 1 ミリ秒に上げた。

### ビルド確認（8.）

Visual Studio でのビルドはしていない（この環境には Windows の開発環境が無い）。画面では確認していない。

### 9. 雲海の地面を広げて重くなったのを軽くした

対象ファイル: `Game/objects/Ground.h/.cpp`

- 原因：4. で地面を 1 層 11×11 枚（雲と霧 4 層で 605 枚）に広げたが、タイルは 1 枚 4608 ポリゴンあり、カメラの後ろや横の映らないタイルも含めて毎フレーム全部（約 280 万ポリゴン）描いていた。また、動かないタイルの行列（逆行列の計算を含む）を毎フレーム 605 枚分求め直していた。
- 描画のたびにカメラの視錐台に映るマスを求め、映らないマスは雲も霧も描かないようにした。マスを雲と霧の層ごと囲む球が視錐台の外にあるかで判定し、1 回求めた結果を雲と霧の全層で使う。
- タイルの行列は、カメラの真下のマスが変わって並べ直したときだけ求めるようにした。

### ビルド確認（9.）

Visual Studio でのビルドはしていない。DirectX まわりを仮のヘッダーに差し替えて g++ でビルドし、エンジンの行列関数で作ったカメラ（視野角 60°・遠平面 100）で確かめた。描くタイルの数は、レールを走るカメラで 605 → 約 200〜230 枚（地面を広げる前の 225 枚とほぼ同じ）、真上から見下ろす俯瞰カメラで約 225 枚。画面に映るはずのタイルを描かずに捨てていないことと、並べ直さない間は行列を 1 枚も求め直さないことも確かめた。画面では確認していない。

### 10. 雲海の地面の端を空に溶け込ませて、どこまでも続いて見えるようにした

対象ファイル: `Engine/Graphics/Shaders/Obj3D/Object3d.PS.hlsl`、`Engine/Graphics/3D/Model.h/.cpp`、`Engine/Graphics/3D/Obj3D.cpp`、`Game/objects/Ground.h/.cpp`

- 遠くほど色を「その方向に見える空（スカイボックス）の色」へ近づける処理を、Obj3D のピクセルシェーダーに足した。スカイボックスはカメラを中心に回転させずに描いているので、カメラからピクセルへの向きで同じキューブマップを読めば、後ろに見える空とまったく同じ色になる。半透明の霧も同じ色へ近づけるので、空の上に空の色を重ねるだけになり切れ目が出ない。
- マテリアルに `distanceFadeStart` / `distanceFadeEnd` を追加した（シェーダー側にだけあった `useBlinnPhong` も C++ 側に足して並びをそろえた。初期値は 0 で、今までと見た目は変わらない）。
- 雲海の地面の雲と霧に、カメラからの距離 45〜95 で溶け込ませる設定をした。距離は調整項目 `horizonFadeStart` / `horizonFadeEnd` で変えられる。End は地面が切れる距離（100）を超えないよう制限する。
- 距離は水平ではなくカメラからの距離で測る。カメラの遠平面で切れる位置は必ずこの距離 100 以上になるので、カメラが高い位置にあっても遠平面の切れ目も隠れる（その代わり、高い位置から見下ろす俯瞰カメラでは足元も少し空の色に寄る）。

### ビルド確認（10.）

Visual Studio でのビルドはしていない。ピクセルシェーダーは Linux 版の DXC でエンジンと同じオプション（ps_6_0、-Zpr など）でコンパイルが通ることを確認し、定数バッファの並び（useBlinnPhong 104・distanceFadeStart 108・distanceFadeEnd 112）が C++ の `Model::Material` と一致することを確かめた。Ground は仮のヘッダーで g++ でビルドした。画面では確認していない。

### 11. 遠くのレール・スタート台なども空に溶け込ませるようにした

対象ファイル: `Engine/Graphics/3D/Obj3DCommon.h`、`Engine/Graphics/3D/Obj3D.h/.cpp`、`Engine/Graphics/3D/Camera.h`、`Game/objects/Ground.cpp`、`Game/scenes/GameScene.h/.cpp`

- 地面だけが空に溶け込み、遠くのレールなどがくっきりしたままだと、もやの手前に浮いて見え、カメラの遠平面（100）で急に現れたり消えたりする。地面以外の 3D オブジェクトも、遠くほど空の色に溶け込ませるようにした。
- `Obj3dCommon` に全オブジェクト共通の溶け込ませる距離を持たせ、`Obj3D::Draw()` で毎回マテリアルに書き込む。物ごとに設定しなくても、レール・スタート台・的・敵などすべてに効く。地面のタイルは `SetUseSceneDistanceFade(false)` で独自の距離（45〜95）のままにした。
- レールは進む先や飛び移れるレールを見せる道しるべで、敵は 25 以内でプレイヤーに気づくため、地面より遠い 60〜95 から溶け込ませる。調整項目 `objectFadeStart` / `objectFadeEnd` で変えられる。End はカメラの遠平面（`Camera::GetFarClip()` を追加）を超えないよう制限する。
- 溶け込ませるのは Play 中だけ。Edit 中は俯瞰カメラで遠くの制御点も編集するため溶け込ませない。シーンを抜けるときは 0 に戻し、他のシーンへ持ち越さない。

### ビルド確認（11.）

Visual Studio でのビルドはしていない。Ground は仮のヘッダーで g++ でビルドした。Obj3D・Obj3dCommon・GameScene は DirectX まわりの依存が多いため、この環境では確認できていない。画面では確認していない。

### 12. レールに螺旋をまとめて作れるようにした

対象ファイル: `Game/Editor/RailEditor.h/.cpp`

- らせん階段のようなレールを制御点 1 つずつで作ると手間がかかるため、Rail Editor の選択中の制御点に「Spiral」を追加した。半径・1 周で上がる高さ・周回数・1 周の制御点数・回る向きを決めて「Generate Spiral」を押すと、その制御点の後ろに螺旋の制御点をまとめて挿入する。
- 前の制御点から進んできた向きのまま螺旋に入るよう、円の中心を進む向きの右（左）横に置き、高さは角度に比例して上げる。
- 末尾の制御点から作るときは、最後の制御点は曲線が通らないため、螺旋の終わりまで曲線が通るよう 1 つ先にもう 1 つ足す。
- 速度・回転は始まりの制御点のものを引き継ぎ、分岐設定は引き継がない。作った後は普通の制御点と同じように保存・編集でき、Ctrl+Z で作る前に戻せる。

### ビルド確認（12.）

Visual Studio でのビルドはしていない。DirectX まわりを仮のヘッダーに差し替えて g++ でビルドし、今の Rail 4 のポイント 4 から初期設定（半径 8・1 周 6 上がる・2 周・1 周 8 点・右回り）で作って確かめた。制御点は 5 → 22 個になり、すべて中心から半径 8 の位置に並ぶ。曲線（Catmull-Rom）の円からのずれは最大 0.07 で、曲線は螺旋の終わりの点でちょうど終わり、ポイント 4 では入ってきた向きのまま螺旋に入る。画面では確認していない。

### 13. シーンが切り替わると保存していない編集が消えるのを直した

対象ファイル: `Game/Editor/RailEditor.h/.cpp`、`Game/Editor/TargetEditor.h/.cpp`、`Game/Editor/EnemyEditor.h/.cpp`

- 原因：クリア・ゲームオーバーなどでシーンが切り替わると GameScene ごとエディターが作り直され、レール・的・敵をファイルから読み込み直すため、保存していない編集（追加した螺旋など）が消えていた。
- エディターが破棄されるときに保存していない変更があれば、その内容を static な置き場に取っておき、次に作ったエディターが読み込みの後で戻すようにした。
- ファイルには書き込まない。戻した内容は「未保存」のままで、Save で保存、Ctrl+Z か Load でファイルの状態に戻せる。
- アプリを終了したときの未保存の変更は、これまで通り消える。

### ビルド確認（13.）

Visual Studio でのビルドはしていない。DirectX まわりを仮のヘッダーに差し替えて g++ でビルドし、Rail 4 に螺旋を追加 → エディターを破棄（シーンの切り替え）→ 作り直す、の流れで確かめた。螺旋（制御点 22 個）が戻って「未保存」になり、Ctrl+Z でファイルの状態（5 個）に戻ること、保存していないときはファイルが書き換わらないこと、的のエディターでも追加した的が戻ることを確認した。画面では確認していない。

---

## 2026-10-24

### 1. Play 中のレールの管を 1 本のモデルにまとめて軽くした

対象ファイル: `Engine/Graphics/3D/Model.h/.cpp`、`Engine/Graphics/3D/Obj3D.h/.cpp`、`Game/Editor/RailEditor.h/.cpp`

- 原因：レールの管を、Catmull-Rom の 1 区間あたり 24 本の円柱（1 本ずつ別の Obj3D）で描いていた。螺旋のレール（Rail 4：制御点 23 個、Rail 5：制御点 48 個）を足したことで、全レールで約 1850 本になり、毎フレーム約 1850 回の描画命令を出していた。
- レール 1 本ごとに、管の形の頂点をコードで作って 1 つのモデルにした。レールに沿って区間の境目ごとに 8 角形の輪を置き、隣り合う輪どうしを三角形でつなぐ。
  - 輪の横向きは前の輪から引き継ぎ（進む向きの成分だけ取り除く）、螺旋などで管がねじれて見えないようにした。
  - 管は Play に入った最初の描画で作る。前回作ったときから制御点が変わっていないレールは作り直さない。
- 頂点はレールの始点から順に並ぶため、塗った区間はインデックスの連続した範囲になる。同じモデルを「塗っていない色」と「塗った色」の 2 つの Obj3D で共有し、それぞれ自分の範囲だけを描く。描画命令はレール 1 本につき最大 3 回（6 本で最大 18 回）になった。
- エンジン側に次の 2 つを追加した。
  - `Model::Initialize(ModelCommon*, ModelData)`：ファイルを読まずに、コードで作った頂点・インデックスからモデルを作る（ファイルから読む `Initialize` も、読み込んだ後はこれを呼ぶ）。
  - `Model::Draw` の開始インデックス・インデックス数の指定と、`Obj3D::SetDrawIndexRange()`：モデルの一部だけを描く。省略時は今まで通りモデル全体を描く。
- 管 1 本ずつの Obj3D（`tubeObjects`）・区間ごとの表示フラグ（`tubeSegmentVisible`）と、起動時にそれを作っておく処理を消した。管のモデル `RailTube/railTube.obj` は使わなくなった（テクスチャ `RailTube/white.png` は引き続き使う）。
- 管の両端はふたの無い開いた形になった（半径 0.05 のため、ほぼ見えない）。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `RailEditor.h::kTubeSideCount` | 8 | 管の輪の周りの分割数 |
| `RailEditor.h::kTubeIndicesPerQuad` | 6 | 四角形 1 つを三角形 2 つで描くときのインデックスの数 |
| `RailEditor.h::kMinTubeDirectionLength` | 1e-4 | 管の向きが定まらないとみなす長さ（前の輪の向きを使う） |
| `Model.h::kDrawAllIndices` | UINT32_MAX | 描くインデックスの数に指定すると、モデルの最後まで描く |

### ビルド確認（1.）

Visual Studio（Debug / Development / Release、x64）でビルドが通ることを確認した。実機で動かし、螺旋のレールを走っても重くならず、全体が大きく軽くなったことを確認した。

### 2. チェックポイントからの復帰

対象ファイル: `Game/Editor/RailEditor.h/.cpp`、`Game/objects/TargetManager.h/.cpp`、`Game/scenes/GameScene.h/.cpp`、`resource/rail/rail4.json`

- 制御点に `isCheckpoint` を追加した。Rail Editor の制御点の編集欄の「Checkpoint」で切り替え、JSON に保存する（無い古いデータは false として読む）。一覧には (Checkpoint) と出し、Edit 中の目印は緑で描く。延長・螺旋で作った制御点には引き継がない。
- レールを進んでいる間に、チェックポイントの制御点を通ったら、レールと制御点・その時点の的ごとの生存状態を覚える。途中に着地した場合も、着地した位置より手前のチェックポイントは通ったものとする。
- 落ちたら、暗転の間にいつも通り `ResetPlayState()` で全部を戻した後、チェックポイントの位置からレールに乗せ、的を覚えた状態に戻す。チェックポイントより前に壊した的は壊れたまま（壊した数もそのまま）、後の的は復活する。体力・敵・連鎖数・レールの塗りは最初の状態に戻る。
- Play に入り直したときと R キーのやり直しでは、覚えたチェックポイントを忘れて最初から始める。
- 「Start From Selected Point」で始めたときは、始めた制御点より手前のチェックポイントは通ったものとしない（今まで通り、落ちたら選んだ制御点からやり直す）。
- ステージ 1 では、Rail 4 の螺旋の入り口（制御点 3）をチェックポイントにした。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `RailEditor.cpp::kCheckpointColor` | (0.2, 1.0, 0.4, 1.0) | チェックポイントにした制御点の、Edit 中の目印の色 |

### ビルド確認（2.）

未確認（Visual Studio でのビルドと実機での動作確認が必要）。

### 3. ゴールを Rail 5 の終点だけにした

対象ファイル: `Game/objects/Player.h/.cpp`

- 原因：クリアの判定が「今乗っているレールが終点まで進んだら」だけで、どのレールかを見ていなかった。Rail 4 の終点（約 4m 横を Rail 5 が通る）まで、Rail 5 へ乗り換えずに進むと、レールの途中でクリア画面へ移っていた。Rail 1〜3 も、乗り換えずに終点まで進むと同じことが起きていた。
- ゴールのレールを `Player.h::kGoalRailIndex` で決め、そのレールの終点に着いたときだけクリアにした（`Player::HasReachedGoal()` はそのまま）。
- ほかのレールの終点に着いたら、`Player::UpdateRailProgress()` でレールから外し、終点の位置から真下へ落とす。どのレールにも乗れずに地面まで落ちたら、いつもの落下と同じくやり直す（チェックポイントを通っていればそこから）。
- 終点で跳ねている途中・飛び移っている途中の状態は、外すときに消す。残すと、外れた直後に同じレールの終点へ着地し直してしまうため。
- 仮の対応。ステージごとにレールを分けるときは、ゴールのレールをレールのデータ側に持たせる。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `Player.h::kGoalRailIndex` | 5 | 終点に着いたらクリアにするレールのインデックス（Rail 5） |

### ビルド確認（3.）

Visual Studio（Debug / Development / Release、x64）でビルドが通ることを確認した。実機での動作確認はまだ。

### 4. 鍵の的を壊すと次のレールが出る仕組み

対象ファイル: `Game/Editor/TargetEditor.h/.cpp`、`Game/objects/TargetManager.h/.cpp`、`Game/Editor/RailEditor.h/.cpp`、`Game/scenes/GameScene.h/.cpp`、`resource/target/target.json`

- 的に `unlockRailIndex`（壊すと出るレール。-1 は鍵の的ではない）を追加した。Inspector の「Unlock Rail」で編集し、JSON に保存する（無い古いデータは -1 として読む）。鍵の的は水色で描き、Hierarchy に (Key: Rail N) と出す。
- 同じレールを指定した鍵の的が 1 つでも残っていれば、そのレールは出ていない。出ていないレールは白い点線で描き（始点・終点の目印も出さない）、着地・並走レールへの乗り換えの対象から外す。
  - 点線は、管と同じ頂点のうち点線の区間の三角形だけを詰めた別のモデルにし、1 回の描画で描く。
- 鍵の的をすべて壊したら、0.8 秒かけて始点から終点へ管を伸ばす。伸びていない区間は点線のまま描く。伸ばしている途中から乗れる。
- 壊し損ねると、レールが出ないまま今のレールの終点に着き、落ちてやり直しになる（チェックポイントを通っていればそこから）。
- レールが出ているかは、的が壊れているかから毎フレーム求める（新しく覚えておく状態は無い）。リスタート・チェックポイントからのやり直しでは、伸ばす演出を飛ばしてすぐに反映する。チェックポイントを通った時点で出ていたレールは出たまま。
- 仮の配置として、Rail 1〜5 それぞれの始点から 4m 先・1.5m 上に、そのレールを出す鍵の的を 1 つずつ足した（Target 17〜21）。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `RailEditor.h::kDashPeriodSegments` / `kDashDrawSegments` | 8 / 4 | 点線の 1 周期の管の区間数と、そのうち描く区間数 |
| `RailEditor.h::kRailUnlockDuration` | 0.8 | レールが出たときに、管を伸ばしきるまでの秒数 |
| `RailEditor.cpp::kLockedRailColor` | (0.85, 0.85, 0.9, 1.0) | 出ていないレールの点線の色 |
| `TargetManager.h::kNormalTargetColor` / `kKeyTargetColor` | (1, 1, 1, 1) / (0.3, 0.85, 1.0, 1.0) | 普通の的と鍵の的の色 |

### ビルド確認（4.）

未確認（Visual Studio でのビルドと実機での動作確認が必要）。

### 5. 弾の撃つ感覚を変えた（押しっぱなしで連射・射程・ばらつき）

対象ファイル: `Game/objects/PlayerBulletManager.h/.cpp`、`Game/scenes/GameScene.h/.cpp`

- 左クリックを押している間、0.1 秒ごとに撃ち続けるようにした。押した瞬間はすぐに 1 発目を撃つ。始め直したときは間隔をリセットする。
- 弾に射程を付けた。射程 30 まではまっすぐ飛び（重力を受けない）、射程を過ぎると勢いが落ちて強い重力ですぐ落ちる。以前は重力 9.8 を最初から受けて、寿命まで遠くへ飛んでいた。
- 撃つたびに狙う方向を少しランダムにずらし、弾をばらけさせた。横には大きく、縦には小さくずらす。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `GameScene.h::kFireInterval` | 0.1 | 押しっぱなしで連射するときの発射間隔（秒） |
| `PlayerBulletManager.h::kRange` | 30.0 | 射程（この距離まではまっすぐ飛ぶ） |
| `PlayerBulletManager.h::kFallGravity` | 40.0 | 射程を過ぎた弾の重力加速度 |
| `PlayerBulletManager.h::kFallDrag` | 4.0 | 射程を過ぎた弾の勢いが 1 秒あたりに減る割合 |
| `PlayerBulletManager.h::kSpreadAngleHorizontal` | 0.05 | 横にずらす最大の角度（ラジアン） |
| `PlayerBulletManager.h::kSpreadAngleVertical` | 0.02 | 縦にずらす最大の角度（ラジアン） |

`kGravity`（9.8）は使わなくなったので消した。

### ビルド確認（5.）

未確認（Visual Studio でのビルドと実機での手触りの確認が必要）。

### 6. 自機の弾を使い回すようにし、射程を短くした

対象ファイル: `Game/objects/PlayerBulletManager.h/.cpp`、`Game/scenes/GameScene.cpp`

- 弾を発射のたびに `make_unique<Obj3D>` で作っていたのをやめ、初期化時に 24 発まとめて作って使い回すようにした（敵弾と同じやり方）。連射（0.1 秒間隔）で寿命 2 秒の弾が同時に飛ぶのは最大 20 発なので、24 発あれば撃てなくなることはない。
- 命中・寿命切れ・着地した弾は未使用に戻すだけにし、リストから削除する `RemoveDeadBullets()` は消した。`Clear()` も弾を消さずに未使用に戻す。
- 描画用の行列の更新は、飛んでいる弾だけ行う。
- 射程を 30 から 20 に短くした。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `PlayerBulletManager.h::kMaxBulletCount` | 24 | 使い回す弾の最大数 |
| `PlayerBulletManager.h::kRange` | 30.0 → 20.0 | 射程 |

### ビルド確認（6.）

未確認（Visual Studio でのビルドと実機での手触りの確認が必要）。

### 7. 弾がレティクルの通りに飛ぶようにし、弾を細くした

対象ファイル: `Game/scenes/GameScene.cpp`、`Game/objects/TargetManager.h/.cpp`、`Game/objects/EnemyManager.h/.cpp`、`Game/objects/PlayerBulletManager.h`

- 原因：弾はカメラの前方 30 の点へ向けて撃っていたが、手元（カメラの前下 3 ほど）とカメラの位置がずれているため、それより手前の的ではレティクルとずれていた。さらに射程を 20 にしたことで、弾が 30 の点に着く前に落ちていた。ばらつき（横 0.05 ラジアン）も、射程の先で 1 ずれるほど大きかった。
- 的をレティクルで狙えている（レティクルの中心が赤い）ときは、その的の中心へ向けて撃つようにした（`TargetManager::FindAimedTarget()`）。
- レティクルの方向が敵の当たり判定の球を通るときは、その敵の中心へ向けて撃つようにした（`EnemyManager::FindAimedEnemy()`）。的と敵の両方を狙えているときは手前のほうを狙う。
- どちらも狙えていないときに狙う点を、カメラの前方 30 から 18 にした（射程 20 より手前にして、落ち始める前に通るようにした）。
- ばらつきを小さくした（射程の先で横に最大 0.3、縦に最大 0.1 ずれる。的の当たり判定の半径 0.6 より小さい）。
- 弾の見た目を細く短くした（当たり判定の大きさはそのまま）。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `GameScene.cpp::kAimConvergeDistance` | 30.0 → 18.0 | 何も狙えていないときに狙う、カメラの前方の点までの距離 |
| `PlayerBulletManager.h::kSpreadAngleHorizontal` | 0.05 → 0.015 | 横にずらす最大の角度（ラジアン） |
| `PlayerBulletManager.h::kSpreadAngleVertical` | 0.02 → 0.005 | 縦にずらす最大の角度（ラジアン） |
| `PlayerBulletManager.h::kStreakWidthRate` | 0.6 → 0.35 | 弾の見た目の太さの割合 |
| `PlayerBulletManager.h::kStreakLengthRate` | 5.0 → 3.0 | 弾の見た目の長さの割合 |

### ビルド確認（7.）

未確認（Visual Studio でのビルドと実機での手触りの確認が必要）。

### 8. 押しっぱなしで撃っているのに弾が見えなくなることがあったのを直した

対象ファイル: `Game/scenes/GameScene.cpp`、`Game/objects/TargetManager.h/.cpp`、`Game/objects/EnemyManager.h/.cpp`

- 原因：7. で、レティクルで狙えている的・敵の中心へ向けて撃つようにしたが、カメラはプレイヤーの後ろ 3 ほどにあるため、カメラとプレイヤーの間にある的・敵も「狙えている」とみなされていた。そのときは手元から後ろ（カメラの方）へ弾が飛び、画面に映らないため、撃つのが途中で止まったように見えていた。
- 弾を向ける的・敵は、カメラの前方に沿って手元より 1.0 以上先にあるものだけにした（`FindAimedTarget()` / `FindAimedEnemy()` に距離の下限 `minDistance` を渡す）。手前のものを飛ばして、その奥で狙えているものがあればそちらを狙う。
- 弾の使い回し（24 発）が足りなくなっていないかも確かめた。0.1 秒ごとに 30 秒間撃ち続けても撃てなかった回は 0 回で、同時に飛んでいた弾は最大 20 発だった。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `GameScene.cpp::kMinAimAheadOfMuzzle` | 1.0 | 弾を向ける的・敵の、手元からカメラの前方への距離の下限 |

### ビルド確認（8.）

未確認（Visual Studio でのビルドと実機での確認が必要）。

### 9. 弾が足りなくなっても撃つのが止まらないようにし、射撃のデバッグ表示を足した

対象ファイル: `Game/objects/PlayerBulletManager.h/.cpp`、`Game/scenes/GameScene.cpp`

- レールを進んでいるとたまに撃てなくなることがある、という報告への対応。原因はまだ分かっていない。
- 弾の数（24 発）は、計算の上では足りている（0.1 秒間隔・寿命 2 秒で同時に飛ぶのは最大 20 発。30 秒撃ち続けても足りなくならないことを確かめた）。念のため、未使用の弾が無いときは、一番長く飛んでいる弾を消して使い回すようにし、弾の数が原因で撃つのが止まることは起きないようにした。
- 「Rail Branch Debug」ウィンドウに、射撃のデバッグ表示を足した。撃てなくなったときにこれを見れば、原因を絞り込める。
  - `Mouse Left`：左クリックが押されていると読めているか（released のままなら、入力が届いていない）
  - `Player Bullets`：飛んでいる弾の数 / 最大数
  - `Bullet Pool Full`：未使用の弾が無く、飛んでいる弾を使い回して撃った回数（増えていれば、弾の数が足りていない）

### ビルド確認（9.）

未確認（Visual Studio でのビルドと実機での確認が必要）。

### 10. プレイ中にマウスカーソルがウィンドウの外へ出て撃てなくなることがあったのを直した

対象ファイル: `Game/scenes/GameScene.cpp`

- 原因：9. の「たまに撃てなくなる」の原因。Play 中にカーソルを隠している間は `ClipCursor` でウィンドウの中に閉じ込めているが、閉じ込めをかけるのは最初の 1 回だけだった。閉じ込めはウィンドウの移動や他のアプリの操作などで Windows 側から外されることがあり、外れたあともかけ直さないため、カーソルがウィンドウの外へ出て、クリックがゲームに届かなくなっていた。
- カーソルを隠している間は、毎フレーム今の閉じ込めの範囲（`GetClipCursor`）とウィンドウのクライアント領域を比べ、外れていたり範囲がずれていたりしたらかけ直すようにした。ウィンドウを動かしたときも、動かした先の範囲に閉じ込め直す。

### ビルド確認（10.）

未確認（Visual Studio でのビルドと実機での確認が必要）。

### 11. 射程を 10 に短くした

対象ファイル: `Game/objects/PlayerBulletManager.h`、`Game/scenes/GameScene.cpp`

- 射程 20 では、鍵の的どうしの間隔（`target.json` でおよそ 18〜21）とほぼ同じで、さらに射程を過ぎてからも最大 10 ほど前へ進んでいたため、まだ近づいていない先の鍵の的まで壊せてしまっていた。
- 射程を 10 にし、射程を過ぎてからの勢いの落ち方を 4 から 8 に強めた（射程の先へ進むのは最大 5 まで）。
- 何も狙えていないときに狙う点を、射程より手前になるよう、カメラの前方 18 から 12 にした（手元からはおよそ 9 先）。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `PlayerBulletManager.h::kRange` | 20.0 → 10.0 | 射程 |
| `PlayerBulletManager.h::kFallDrag` | 4.0 → 8.0 | 射程を過ぎた弾の勢いが 1 秒あたりに減る割合 |
| `GameScene.cpp::kAimConvergeDistance` | 18.0 → 12.0 | 何も狙えていないときに狙う、カメラの前方の点までの距離 |

### ビルド確認（11.）

未確認（Visual Studio でのビルドと実機での手触りの確認が必要）。

### 12. 弾がいつもレティクルの通りに飛ぶようにし、ばらつきを小さくした

対象ファイル: `Game/scenes/GameScene.cpp`、`Game/objects/PlayerBulletManager.h`

- 原因：7. で、的・敵をレティクルで狙えているときはその中心へ向けて撃つようにしたが、的は「カメラ→的」とカメラの前方のなす角が 0.1 ラジアン以内なら狙えているとみなす（レティクルの中心が赤くなる）。そのため、レティクルが的から少し外れていても、弾が的の中心へ曲がって飛び、レティクルの通りに飛ばない瞬間があった。
- 的・敵の中心そのものではなく、レティクルの線（カメラの前方へ伸ばした線）の上の、その的・敵と同じ奥行きの点へ向けて撃つようにした。手元とカメラのずれは奥行きを合わせることで直し、弾はいつもレティクルの通りに飛ぶ。
- ばらつきを、横 0.015 → 0.008、縦 0.005 → 0.003 ラジアンに小さくした（射程 10 の先で横に最大 0.08、縦に最大 0.03 ずれる）。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `PlayerBulletManager.h::kSpreadAngleHorizontal` | 0.015 → 0.008 | 横にずらす最大の角度（ラジアン） |
| `PlayerBulletManager.h::kSpreadAngleVertical` | 0.005 → 0.003 | 縦にずらす最大の角度（ラジアン） |

### ビルド確認（12.）

未確認（Visual Studio でのビルドと実機での手触りの確認が必要）。

---

## 2026-10-26

### 1. ゲームオーバー画面からコンティニューできるようにした

対象ファイル: `Game/objects/ContinuePoint.h`（新規）、`Game/scenes/GameOverScene.h/.cpp`、`Game/scenes/GameScene.h/.cpp`、`tools/generate_result_assets.py`、`tools/generate_help_texture.py`、`resource/UI/gameOverTitle.png`・`help.png`、`resource/UI/continueOption.png`・`toTitleOption.png`（新規）、`MyGameEngine.vcxproj(.filters)`

- これまではゲームオーバーになると SPACE でタイトルへ戻るしかなく、チェックポイントを通っていても最初からやり直しだった。
- ゲームオーバー画面に「コンティニュー」「タイトルへ」の項目を出し、W/S（↑/↓）で選んで SPACE で決定するようにした。最初はコンティニューを選んでいる。選んでいる項目は黄色で少し大きく、選んでいない項目は暗く表示する。
- コンティニューを選ぶと、タイトル・スタート台を飛ばして、最後に通ったチェックポイントからレールに乗って始まる。的はチェックポイントを通った時点の状態に戻り、体力・敵・連鎖数・レールの塗りは最初の状態に戻る（落下リスタートと同じ戻し方）。チェックポイントを通る前に体力が尽きていたときは、レールの始点から始まる。
- シーンは切り替えのたびに作り直されるため、`StageResult` と同じく static な値（`ContinuePoint`）でシーンをまたいで渡す。GameScene が体力 0 の瞬間に最後に通ったチェックポイントを書き込み、GameOverScene がコンティニューを選んだら `isRequested` を立て、次の GameScene が Play に入るときに読んで下ろす。
- 再開した後にまたレールから落ちたときも、同じチェックポイントからやり直す。
- 「GAME OVER」の画像の案内を「SPACE でタイトルへ」から「W/S でえらんで SPACE で決定」に変え、画面の名前を中央より上へずらして下に項目を並べた。項目の画像も同じスクリプトで生成する。
- 操作説明（H キー）の画面の流れの「クリア/ゲームオーバー → タイトルへ戻る」を、クリアとゲームオーバーに分けた。行が 1 つ増えたため、行の間隔を 88 から 76 に詰めた。
- 「Start From Selected Point」で始めてチェックポイントを通る前に体力が尽きたときは、選んだ制御点ではなくレールの始点から始まる（デバッグ用の設定はシーンを作り直すと消えるため）。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `GameOverScene.cpp::kTitleCenterYRate` | 0.35 | 画面の名前の画像の中心の高さ（画面の高さに対する割合） |
| `GameOverScene.cpp::kFirstOptionCenterYRate` | 0.63 | 一番上の項目の中心の高さ（画面の高さに対する割合） |
| `GameOverScene.cpp::kOptionSpacingYRate` | 0.12 | 項目どうしの間隔（画面の高さに対する割合） |
| `GameOverScene.cpp::kSelectedOptionColor` / `kSelectedOptionScale` | (1.0, 0.85, 0.15, 1.0) / 1.1 | 選んでいる項目の色と大きさの倍率 |
| `GameOverScene.cpp::kUnselectedOptionColor` / `kUnselectedOptionScale` | (0.6, 0.6, 0.6, 0.8) / 1.0 | 選んでいない項目の色と大きさの倍率 |

### ビルド確認（1.）

この変更は Windows でのビルド・実機での確認をしていない。`GameOverScene.cpp` を仮のヘッダーで clang の構文チェックにかけ、ImGui がある構成・無い構成どちらでもエラーが無いことだけ確認した。`GameScene.cpp` は DirectX のヘッダーが要るため構文チェックもしていない（Visual Studio でのビルドと実機での動作確認が必要）。

### 2. 残機を追加した

対象ファイル: `Game/scenes/GameScene.h/.cpp`、`tools/generate_help_texture.py`、`resource/UI/help.png`

- これまではレールから落ちても何も減らず何度でもやり直せ、体力が0になるとすぐゲームオーバーだった。
- 残機（最大 3）を追加した。プレイ中にレールから落ちたとき・体力が 0 になったときに 1 つ減らし、残っていれば落下リスタートと同じ暗転（ImGui の「Fall Transition」で選んだ演出）でチェックポイント（通っていなければスタート台）からやり直す。体力は満タンに戻る。
- 残機が 0 になったら、これまでの体力 0 と同じく暗いインクが垂れる演出（`InkDrip`）でゲームオーバー画面へ移る。
- 出発前にスタート台から落ちたときは、まだプレイを始めていないため残機を減らさずにやり直す。
- 残機は `ResetPlayState()` では戻さず、Play に入ったとき（ゲームオーバー画面からのコンティニューを含む）と R キーのやり直しで満タンに戻す。
- 落下リスタートの暗転を `RestartAfterMiss()` に、残機を減らしてやり直すかゲームオーバーにするかを `LoseLife()` にまとめた。
- 残機は「Rail Branch Debug」ウィンドウに Lives として出している。
- 操作説明（H キー）の画面の流れを「プレイ → 残機を減らしてやり直し（体力が 0 / レールから落ちる）」「プレイ → ゲームオーバー（残機が 0）」に変えた。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `GameScene.h::kMaxLives` | 3 | Play を始めたとき・コンティニューしたときの残機 |

### ビルド確認（2.）

未確認（Visual Studio でのビルドと実機での動作確認が必要）。

### 3. 左上のアイコンを残機の表示にし、体力は画面の縁のインクで表すようにした

対象ファイル: `Game/UI/DamageInkEffect.h/.cpp`（新規）、`Engine/Graphics/Shaders/PostProcess/DamageInk.PS.hlsl`（新規）、`Engine/Graphics/Shaders/PostProcess/PostProcess.hlsli`、`Engine/Graphics/Effect/PostProcess.h/.cpp`、`Engine/Framework/Application.h/.cpp`、`Game/UI/StageHUD.h/.cpp`、`Game/scenes/GameScene.h/.cpp`、`tools/generate_help_texture.py`、`resource/UI/help.png`、`MyGameEngine.vcxproj(.filters)`

- 左上のアイコン（`lifeIcon.png`）は体力を出していたが、残機の表示に変えた。残機が減ると、減った分のアイコンが暗く半透明になる。
- 体力は、新しく作ったポストプロセス `DamageInk` で表す。体力が減るほど画面の縁から敵のインク（ピンク）が波打ちながら迫り、景色の色が抜けていく。
  - 体力 3：何もかけない。体力 2：四隅にインクがにじむ。体力 1：インクが縁全体を囲み、鼓動のように脈打つ。
  - 被弾した瞬間は、画面全体を一瞬ピンクに寄せる（0.4 秒で戻る）。
  - インクの広がりは少しずつ寄せるため、じわっと迫って見える。やり直しで体力が満タンに戻ると、インクが引いていく。
- ポストプロセスは 1 回に 1 種類しかかけられないため、ほかのポストプロセス（出発ゲートが開く瞬間の放射ブラーなど）をかけている間はそちらを優先し、終わったら体力の表現に戻すようにした（`Application::Draw()`）。PostProcess Settings の Filter Type にも DamageInk を足した。
- 定数バッファの最後の詰め物（`padding3[2]`）を、体力の減り具合（`damageAmount`）と被弾の色づき（`damageFlash`）に置き換えた。大きさは 64 バイトのまま変わらない。インクの縁を動かす時間には、すでに毎フレーム進めている `randomTime` を使う。
- Play 中だけかけ、Edit モードとシーンの終わりでは外す。クリア画面・ゲームオーバー画面には持ち越さない。
- 操作説明（H キー）の画面の流れの条件を「体力が 0 になる / レールから落ちる」「残機（左上）が 0 になる」に直した。
- 見た目は、シェーダーと同じ計算を Python で再現して確かめた。

### ビルド確認（3.）

この変更は Windows でのビルド・実機での確認をしていない。`DamageInkEffect.cpp` を仮のヘッダーで clang の構文チェックにかけ、エラーが無いことだけ確認した。`DamageInk.PS.hlsl` はシェーダーのコンパイラで確かめていない（実行時にコンパイルされるため、起動時に失敗すると assert で止まる）。
