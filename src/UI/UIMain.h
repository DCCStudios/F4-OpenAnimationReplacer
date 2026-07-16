#pragma once

#include "UI/UIWindow.h"
#include "UI/UICommon.h"

class ReplacerMod;
class SubMod;
class ConditionSet;
class ICondition;
class ReplacementAnimation;

class UIMain : public UIWindow
{
public:
	static UIMain* GetSingleton()
	{
		static UIMain singleton;
		return &singleton;
	}

	// True while the Settings panel is waiting for the next keypress to
	// rebind the UI toggle hotkey. WndProc checks this so the press is
	// captured as the new bind instead of toggling/closing the menu.
	bool IsCapturingToggleKey() const { return capturingToggleKey; }
	void SetCapturingToggleKey(bool a_capturing) { capturingToggleKey = a_capturing; }
	void ApplyCapturedToggleKey(std::uint32_t a_dik);

protected:
	void DrawContents() override;
	ImGuiWindowFlags GetWindowFlags() const override;
	ImVec2 GetDefaultSize() const override { return ImVec2(1280, 850); }

private:
	UIMain() : UIWindow(WindowID::kMain, "Open Animation Replacer") {}

	void DrawFilterBar();
	void DrawTabBar();
	void DrawReplacerModsTab();
	void DrawReplacementAnimsTab();
	void DrawModTree();
	void DrawSubModDetails(SubMod* a_subMod);
	void DrawSubModNode(SubMod* a_subMod, ReplacerMod* a_mod);
	void DrawConditionSet(ConditionSet* a_condSet, SubMod* a_subMod, int a_depth = 0);
	void DrawCondition(ICondition* a_condition, ConditionSet* a_parentSet, int a_index, SubMod* a_subMod, int a_depth);
	void DrawTrackFilterSection(SubMod* a_subMod, bool a_editable);
	void DrawReplacementAnimList(SubMod* a_subMod);
	void DrawBottomBar();
	void DrawSettingsPanel();

	UICommon::EditorMode currentMode{ UICommon::EditorMode::kInspect };
	bool modeInitialized{ false };
	char filterText[256]{};
	uint32_t evalTargetFormID{ 0x14 };
	SubMod* selectedSubMod{ nullptr };
	bool showSettings{ false };
	bool capturingToggleKey{ false };
	float firstColumnPercent{ 0.45f };
	std::string copiedConditionJson;

	// Rename popup state
	SubMod* renamingSubMod{ nullptr };
	char renameBuffer[256]{};
};
