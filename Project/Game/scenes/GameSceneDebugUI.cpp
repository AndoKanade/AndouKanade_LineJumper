#include "GameScene.h"
#include "GameSceneInternal.h"
#include "CameraManager.h"
#include "ImGuiManager.h"
#include "ModelManager.h"
#include "SceneManager.h"
#include "Input.h"
#include "Obj3dCommon.h"
#include "Application.h"
#include "GlobalVariables.h"
#include "EditorWidgets.h"
#include "EditorContext.h"

// レールエディター
#include "Editor/RailEditor.h"
#include "Editor/EditorDocumentManager.h"
// 的・敵の配置エディター(下段の選択パネルで選ぶときのパネル名を使う)
#include "Editor/TargetEditor.h"
#include "Editor/EnemyEditor.h"
// 出発の演出(スタート台からステージのレールへ移るときの演出)
#include "Title/Departure/DepartureDirector.h"
// ゲームオブジェクト
#include "objects/Player.h"
#include "objects/PlayerBulletManager.h"
#include "objects/EnemyManager.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#ifdef USE_IMGUI
// 選んだエディターを imgui.ini に書き出してもらうのに使う(ImGui::MarkIniSettingsDirty)
#include "imgui_internal.h"
#endif

// GameSceneのうち、ImGuiのデバッグ用の画面(状態確認のウィンドウ・各エディターのパネル・下段の選択パネル・調整用のパネル)をまとめたファイル
// USE_IMGUIが無い構成(Release)では、関数は1つも入らない

using namespace GameSceneInternal;

#ifdef USE_IMGUI
namespace{
	// 落下リスタートの演出の名前(ImGui の切り替えに表示する。SceneTransition::Style と同じ順番にする)
	const char* kFallTransitionNames[] = {"Fade", "RailStripes", "FallAndLift", "RetryCutIn"};

	// クリア画面へ切り替えるときの演出の候補と、ImGui の切り替えに表示する名前(同じ順番にする)
	constexpr SceneTransition::Style kClearTransitionStyles[] = {
		SceneTransition::Style::InkBurst,
		SceneTransition::Style::RailPaint,
		SceneTransition::Style::GoalStamp,
		SceneTransition::Style::CloudRise,
	};
	const char* kClearTransitionNames[] = {"InkBurst", "RailPaint", "GoalStamp", "CloudRise"};
	static_assert(std::size(kClearTransitionStyles) == std::size(kClearTransitionNames),"クリアの演出の候補と名前の数が合っていない");

	// 復活したときの見せ方の候補と、ImGui の切り替えに表示する名前(同じ順番にする)
	constexpr Model::RevealStyle kRespawnRevealStyles[] = {
		Model::RevealStyle::Stripes,
		Model::RevealStyle::Sweep,
	};
	const char* kRespawnRevealNames[] = {"Stripes", "Sweep"};
	static_assert(std::size(kRespawnRevealStyles) == std::size(kRespawnRevealNames),"復活の見せ方の候補と名前の数が合っていない");

	// デバッグ用のメインウィンドウのパネル名
	constexpr const char* kGameSceneDebugPanelName = "GameScene Debug";
	// 下段の選択パネルで選べるエディター
	struct ToolPanelEntry{
		const char* panelName;    // 各エディターが EditorWidgets::BeginToolPanel() に渡すパネル名
		const char* documentName; // 未保存の印を付けるのに使う、編集履歴の名前(IEditorDocument::GetDocumentName()。保存する物が無ければnullptr)
	};
	// エディターを増やしたらここに足す。先頭のエディターが初めて起動したときに選ばれる。上から順に Ctrl+1, Ctrl+2... で切り替えられる
	constexpr ToolPanelEntry kToolPanels[] = {
		{kGameSceneDebugPanelName, nullptr},
		{TargetEditor::kPanelName, "Target"},
		{EnemyEditor::kPanelName, "Enemy"},
		{Application::kPostProcessPanelName, nullptr},
	};
	// Ctrl+数字キーで切り替えられるエディターの数(数字キーは1〜9)
	constexpr int kMaxToolPanelShortcutCount = 9;
	// パネル名の後ろに未保存の印を付けたラベルを作るときの、文字列の長さの上限
	constexpr size_t kToolPanelLabelSize = 64;
}

// レール・プレイヤー・敵の状態確認用のデバッグウィンドウ
void GameScene::ShowStatusWindow(){
	ImGui::Begin("Rail Branch Debug");
	ImGui::Text("Rail Count: %d",railEditor_->GetRailCount());
	ImGui::Text("Active Rail Index: %d",railEditor_->GetActiveRailIndex());
	ImGui::Text("Active Rail Point Count: %d",railEditor_->GetControlPointCount());
	ImGui::Text("Rail T: %.3f",player_->GetRailT());
	ImGui::Text("Current Point Index: %d",railEditor_->GetControlPointIndexFromT(player_->GetRailT()));

	// オンレール判定・自由移動のデバッグ表示
	ImGui::Text("On Rail: %s",player_->IsOnRail()?"true":"false");
	if(!player_->IsOnRail()){
		ImGui::Text("Free Velocity Y: %.2f",player_->GetFreeVelocityY());
	}
	ImGui::Text("Jump Key: SPACE");

	// 射撃のデバッグ表示(撃てなくなったときに、左クリックが届いているか・弾の数が足りているかを確かめる)
	ImGui::Text("Mouse Left: %s",(input_ && input_->PushMouseButton(kShootMouseButton))?"pressed":"released");
	ImGui::Text("Player Bullets: %d / %d",bulletManager_->GetAliveCount(),bulletManager_->GetMaxCount());
	ImGui::Text("Bullet Pool Full: %d",bulletManager_->GetPoolFullCount());

	// 雑魚敵のデバッグ表示
	enemyManager_->ShowDebugInfo();

	// プレイヤーの体力・無敵時間のデバッグ表示
	ImGui::Text("Player HP: %d / %d",player_->GetHp(),player_->GetMaxHp());
	ImGui::Text("Lives: %d / %d",lives_,kMaxLives);
	ImGui::Text("Player Invincible: %.2f",player_->GetInvincibleTimer());
	ImGui::End();
}

// Editモード中の編集用パネル
void GameScene::ShowEditorPanels(){
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
	if(!activeCamera){
		return;
	}

	// 操作するエディターの選択パネル(下段・左半分)
	ShowEditorSelector();

	// デバッグ用のメインウィンドウ(選択パネルで選ばれているときだけ右の列に出す)
	if(EditorWidgets::BeginToolPanel(kGameSceneDebugPanelName)){
		ShowDebugPanelContents(activeCamera);
		ImGui::End();
	}

	Application::GetInstance()->ShowPostProcessUI();
}

// 操作するエディターの選択パネル
// どのエディターを選んでいても使えるよう、Undo/Redo・まとめて保存と未保存の一覧もここに置く
void GameScene::ShowEditorSelector(){
	EditorContext* editorContext = EditorContext::GetInstance();
	EditorDocumentManager* documentManager = EditorDocumentManager::GetInstance();
	// 選んだエディターが変わったら、次に起動したときも同じエディターを開けるよう imgui.ini に書き出してもらう
	const std::string previousPanel = editorContext->GetActiveToolPanel();

	// Sceneビューで、選んでいないエディターの物(的・敵)をクリックしていたら、そのエディターへ切り替える
	editorContext->ResolveScenePicks();

	// Ctrl+数字キーで、上から順にエディターを切り替える(文字入力中は数字を打てるよう受け付けない)
	if(!ImGui::GetIO().WantTextInput){
		const int shortcutCount = std::min(static_cast<int>(std::size(kToolPanels)),kMaxToolPanelShortcutCount);
		for(int i = 0; i < shortcutCount; ++i){
			if(ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | static_cast<ImGuiKey>(ImGuiKey_1 + i))){
				editorContext->SetActiveToolPanel(kToolPanels[i].panelName);
			}
		}
	}

	// まだ何も選ばれていない(初めての起動)・一覧に無い名前が選ばれているときは、先頭のエディターを選ぶ
	bool isKnownPanel = false;
	for(const ToolPanelEntry& entry : kToolPanels){
		if(editorContext->IsActiveToolPanel(entry.panelName)){
			isKnownPanel = true;
		}
	}
	if(!isKnownPanel){
		editorContext->SetActiveToolPanel(kToolPanels[0].panelName);
	}

	EditorWidgets::BeginFixedPanel("Editors",EditorWidgets::ComputeLayout().editorSelector);

	// 選んだエディターの操作パネルを右の列に出す(次のフレームから切り替わる)
	for(size_t i = 0; i < std::size(kToolPanels); ++i){
		const ToolPanelEntry& entry = kToolPanels[i];

		// 保存していない変更があるエディターは、名前の後ろに * を付ける
		bool isModified = false;
		if(entry.documentName){
			for(const IEditorDocument* document : documentManager->GetDocuments()){
				if(std::strcmp(document->GetDocumentName(),entry.documentName) == 0 && document->IsModified()){
					isModified = true;
				}
			}
		}
		// ### 以降をIDにして、* が付いても同じボタンとして扱われるようにする
		char label[kToolPanelLabelSize];
		snprintf(label,sizeof(label),"%s%s###%s",entry.panelName,isModified?" *":"",entry.panelName);

		if(i > 0){
			ImGui::SameLine();
		}
		if(ImGui::RadioButton(label,editorContext->IsActiveToolPanel(entry.panelName))){
			editorContext->SetActiveToolPanel(entry.panelName);
		}
	}
	ImGui::TextDisabled("(Ctrl+1〜%d: 切り替え / Sceneビューで的・敵をクリックすると、そのエディターに切り替わる)",std::min(static_cast<int>(std::size(kToolPanels)),kMaxToolPanelShortcutCount));

	if(editorContext->GetActiveToolPanel() != previousPanel){
		ImGui::MarkIniSettingsDirty();
	}

	ImGui::Separator();

	// レール・的・敵の編集のUndo/Redoと、まとめて保存(Ctrl+Z / Ctrl+Y / Ctrl+Sでも操作できる)
	documentManager->HandleShortcuts();
	ImGui::BeginDisabled(!documentManager->CanUndo());
	if(ImGui::Button("Undo")){
		documentManager->Undo();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!documentManager->CanRedo());
	if(ImGui::Button("Redo")){
		documentManager->Redo();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if(ImGui::Button("Save All")){
		documentManager->SaveAll();
	}

	// 保存していない変更があるエディターの名前を並べる(保存し忘れに気づけるようにする)
	bool hasUnsavedDocument = false;
	for(const IEditorDocument* document : documentManager->GetDocuments()){
		if(!document->IsModified()){
			continue;
		}
		if(!hasUnsavedDocument){
			ImGui::TextUnformatted("Unsaved:");
			hasUnsavedDocument = true;
		}
		ImGui::SameLine();
		ImGui::TextUnformatted(document->GetDocumentName());
	}

	ImGui::End();
}

// デバッグ用のメインウィンドウの中身(デバッグ用の開始設定・演出の切り替えとプレビュー・カメラ・ライティング)
void GameScene::ShowDebugPanelContents(Camera* activeCamera){
	// ONにすると、Playを押したときにタイトル・スタート演出を飛ばし、レールの上からすぐプレイを始める(デバッグ用)
	ImGui::Checkbox("Skip Title",&skipTitle_);
	// ONにすると、Playを押したときにRail Editorで選んでいる制御点からプレイを始める(タイトル・スタート台も飛ばす。落ちたときも同じ制御点から始め直す)
	ImGui::Checkbox("Start From Selected Point",&startsFromSelectedPoint_);
	ImGui::TextDisabled("(未選択ならいつも通り)");

	// 落下リスタートの演出を切り替える。Preview を押すと落ちなくてもその場で演出だけを確認できる
	// 選んだ演出は調整項目に書き込み、"Global Variables" ウィンドウの Save で JSON に保存する
	GlobalVariables* gv = GlobalVariables::GetInstance();
	const SceneTransition::Style fallTransitionStyle = GetTransitionStyle(kFallTransitionKey);
	int fallStyleIndex = static_cast<int>(fallTransitionStyle);
	if(ImGui::Combo("Fall Transition",&fallStyleIndex,kFallTransitionNames,IM_ARRAYSIZE(kFallTransitionNames))){
		gv->SetValue(kGameSceneGroup,kFallTransitionKey,static_cast<int32_t>(fallStyleIndex));
	}
	if(ImGui::Button("Preview Fall Transition")){
		sceneManager_->StartTransition(nullptr,fallTransitionStyle);
	}

	// クリア画面へ切り替えるときの演出を切り替える。Preview を押すとクリアしなくてもその場で演出だけを確認できる
	const SceneTransition::Style clearTransitionStyle = GetTransitionStyle(kClearTransitionKey);
	int clearStyleIndex = 0;
	for(int i = 0; i < static_cast<int>(std::size(kClearTransitionStyles)); ++i){
		if(kClearTransitionStyles[i] == clearTransitionStyle){
			clearStyleIndex = i;
		}
	}
	if(ImGui::Combo("Clear Transition",&clearStyleIndex,kClearTransitionNames,IM_ARRAYSIZE(kClearTransitionNames))){
		gv->SetValue(kGameSceneGroup,kClearTransitionKey,static_cast<int32_t>(kClearTransitionStyles[clearStyleIndex]));
	}
	if(ImGui::Button("Preview Clear Transition")){
		sceneManager_->StartTransition(nullptr,clearTransitionStyle);
	}

	// 復活したときの、透明な体が元に戻る見せ方を切り替える。Preview を押すと、その場のプレイヤーで見た目を確認できる
	const Model::RevealStyle respawnRevealStyle = GetRespawnRevealStyle();
	int respawnRevealIndex = 0;
	for(int i = 0; i < static_cast<int>(std::size(kRespawnRevealStyles)); ++i){
		if(kRespawnRevealStyles[i] == respawnRevealStyle){
			respawnRevealIndex = i;
		}
	}
	if(ImGui::Combo("Respawn Reveal",&respawnRevealIndex,kRespawnRevealNames,IM_ARRAYSIZE(kRespawnRevealNames))){
		gv->SetValue(kGameSceneGroup,kRespawnRevealKey,static_cast<int32_t>(kRespawnRevealStyles[respawnRevealIndex]));
	}
	if(ImGui::Button("Preview Respawn Reveal")){
		player_->StartReveal(respawnRevealStyle,kRespawnRevealDuration);
	}

	// スタート台からステージのレールへ移るときの出発の演出を切り替える(次に出発ゲートが開いたときから使われる)
	int departureStyleIndex = static_cast<int>(GetDepartureStyle());
	if(ImGui::Combo("Departure",&departureStyleIndex,DepartureDirector::kStyleNames,IM_ARRAYSIZE(DepartureDirector::kStyleNames))){
		gv->SetValue(kGameSceneGroup,kDepartureStyleKey,static_cast<int32_t>(departureStyleIndex));
	}

	// カメラ設定のUI
	if(ImGui::CollapsingHeader("Camera Settings")){
		Vector3 camPos = activeCamera->GetTranslate();
		if(ImGui::DragFloat3("Camera Pos",&camPos.x,0.1f)){
			activeCamera->SetTranslate(camPos);
		}

		Vector3 camRot = activeCamera->GetRotate();
		if(ImGui::DragFloat3("Camera Rotate",&camRot.x,0.01f)){
			activeCamera->SetRotate(camRot);
		}
	}

	// ライティング設定のUI
	if(ImGui::CollapsingHeader("Lighting")){
		if(PointLight* pData = object3dCommon_->GetPointLightData()){
			ImGui::Text("Point Light");
			ImGui::ColorEdit4("Point Color",&pData->color.x);
			ImGui::DragFloat3("Point Pos",&pData->position.x,0.1f);
			ImGui::DragFloat("Point Intensity",&pData->intensity,0.1f,0.0f,100.0f);
		}
		if(SpotLight* sData = object3dCommon_->GetSpotLightData()){
			ImGui::Text("Spot Light");
			ImGui::ColorEdit4("Spot Color",&sData->color.x);
		}
	}

	ModelManager::GetInstance()->UpdateLightGui();
}
#endif
