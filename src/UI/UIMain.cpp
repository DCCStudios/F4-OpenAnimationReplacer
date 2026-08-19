#include "UI/UIMain.h"
#include "UI/UIManager.h"
#include "UI/BoneDebugViz.h"
#include "OpenAnimationReplacer.h"
#include "ReplacerMods.h"
#include "ReplacementAnimation.h"
#include "BaseConditions.h"
#include "Conditions.h"
#include "AnimationLog.h"
#include "Hooks.h"
#include "Jobs.h"
#include "Parsing.h"
#include "Settings.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <mutex>

// Havok skeleton bones — matches the actual animation skeleton definition.
// File scope because both the track filter section and the custom events
// bone picker (CullBone/UncullBone) present this list.
static const char* kKnownBones[] = {
	// Core body
	"Root", "COM", "Pelvis", "Spine1", "Spine2", "Chest", "Neck", "Head",
	// Left arm
	"LArm_Collarbone", "LArm_UpperArm", "LArm_UpperTwist1", "LArm_UpperTwist2",
	"LArm_ForeArm1", "LArm_ForeArm2", "LArm_ForeArm3", "LArm_Hand",
	"LArm_Finger11", "LArm_Finger12", "LArm_Finger13",
	"LArm_Finger21", "LArm_Finger22", "LArm_Finger23",
	"LArm_Finger31", "LArm_Finger32", "LArm_Finger33",
	"LArm_Finger41", "LArm_Finger42", "LArm_Finger43",
	"LArm_Finger51", "LArm_Finger52", "LArm_Finger53",
	// Right arm
	"RArm_Collarbone", "RArm_UpperArm", "RArm_UpperTwist1", "RArm_UpperTwist2",
	"RArm_ForeArm1", "RArm_ForeArm2", "RArm_ForeArm3", "PipboyBone", "RArm_Hand",
	"RArm_Finger11", "RArm_Finger12", "RArm_Finger13",
	"RArm_Finger21", "RArm_Finger22", "RArm_Finger23",
	"RArm_Finger31", "RArm_Finger32", "RArm_Finger33",
	"RArm_Finger41", "RArm_Finger42", "RArm_Finger43",
	"RArm_Finger51", "RArm_Finger52", "RArm_Finger53",
	// Legs
	"LLeg_Thigh", "LLeg_Calf", "LLeg_Foot", "LLeg_Toe1",
	"RLeg_Thigh", "RLeg_Calf", "RLeg_Foot", "RLeg_Toe1",
	// Weapon (right hand)
	"Weapon", "WeaponBolt", "WeaponTrigger", "WeaponMagazine",
	"WeaponMagazineChild1", "WeaponMagazineChild2", "WeaponMagazineChild3",
	"WeaponMagazineChild4", "WeaponMagazineChild5",
	"WeaponMagazineChild6", "WeaponMagazineChild7", "WeaponMagazineChild8",
	"WeaponMagazineChild9", "WeaponMagazineChild10", "WeaponMagazineChild11",
	"WeaponMagazineChild12", "WeaponMagazineChild13", "WeaponMagazineChild14",
	"WeaponMagazineChild15",
	"WeaponOptics1", "WeaponOptics2",
	"WeaponExtra1", "WeaponExtra2", "WeaponExtra3",
	"WeaponExtra4", "WeaponExtra5", "WeaponExtra6", "WeaponExtra7",
	"WeaponExtra8", "WeaponExtra9", "WeaponExtra10", "WeaponExtra11",
	"WeaponExtra12", "WeaponExtra13", "WeaponExtra14", "WeaponExtra15",
	"WeaponExtra16", "WeaponExtra17", "WeaponExtra18", "WeaponExtra19", "WeaponExtra20",
	"WeaponBipod", "WeaponBipodL", "WeaponBipodR",
	// Weapon (left hand)
	"WeaponLeft",
	// IK / Camera / Anim objects
	"WeaponIKTargetL", "WeaponIKTargetR",
	"WeaponIKTargetLMirror", "WeaponIKTargetRMirror",
	"Camera", "Camera Control", "CamTarget",
	"AnimObjectA", "AnimObjectB",
	"AnimObjectL1", "AnimObjectL2", "AnimObjectL3",
	"AnimObjectR1", "AnimObjectR2", "AnimObjectR3",
	// Helpers
	"L_RibHelper", "R_RibHelper",
};
static constexpr int kNumKnownBones = static_cast<int>(sizeof(kKnownBones) / sizeof(kKnownBones[0]));

ImGuiWindowFlags UIMain::GetWindowFlags() const
{
	return ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse;
}

void UIMain::DrawContents()
{
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("View")) {
			auto* uiMgr = UIManager::GetSingleton();
			if (ImGui::MenuItem("Animation Log")) uiMgr->ToggleWindow(WindowID::kAnimationLog);
			if (ImGui::MenuItem("Event Log")) uiMgr->ToggleWindow(WindowID::kAnimationEventLog);
			if (ImGui::MenuItem("Active Replacements")) uiMgr->ToggleWindow(WindowID::kDebugOverlay);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Actions")) {
			if (ImGui::MenuItem("Reload All Configs")) {
				// Queued to the GAME thread — the reload frees objects that
				// live graph updates (and this UI) hold pointers to, so it
				// must never run here on the render thread.
				RequestConfigReload();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	DrawFilterBar();
	ImGui::Separator();
	DrawTabBar();
	DrawBottomBar();

	if (showSettings) {
		DrawSettingsPanel();
	} else {
		// Closing Settings (button or window X) cancels an in-progress rebind.
		capturingToggleKey = false;
	}
}

void UIMain::DrawFilterBar()
{
	float availWidth = ImGui::GetContentRegionAvail().x;

	ImGui::PushStyleColor(ImGuiCol_FrameBg, UICommon::Colors::FilterBg);
	ImGui::SetNextItemWidth(availWidth * 0.4f);
	ImGui::InputTextWithHint("##filter", "Filter mods...", filterText, sizeof(filterText));
	ImGui::PopStyleColor();

	ImGui::SameLine(availWidth * 0.5f);

	if (!modeInitialized) {
		currentMode = static_cast<UICommon::EditorMode>(
			std::clamp(Settings::GetSingleton()->iEditorMode, 0, 2));
		modeInitialized = true;
	}
	int modeInt = static_cast<int>(currentMode);
	UICommon::EditorMode prevMode = currentMode;
	if (ImGui::RadioButton("Inspect", modeInt == 0)) currentMode = UICommon::EditorMode::kInspect;
	ImGui::SameLine();
	if (ImGui::RadioButton("User", modeInt == 1)) currentMode = UICommon::EditorMode::kUser;
	ImGui::SameLine();
	if (ImGui::RadioButton("Author", modeInt == 2)) currentMode = UICommon::EditorMode::kAuthor;
	if (currentMode != prevMode) {
		Settings::GetSingleton()->iEditorMode = static_cast<int>(currentMode);
		Settings::GetSingleton()->Save();
	}

	ImGui::SameLine(availWidth - 140);
	ImGui::Text("Target:");
	ImGui::SameLine();
	char buf[16];
	snprintf(buf, sizeof(buf), "0x%X", evalTargetFormID);
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputText("##evalTarget", buf, sizeof(buf))) {
		try { evalTargetFormID = std::stoul(buf, nullptr, 16); } catch (...) {}
	}
}

void UIMain::DrawTabBar()
{
	if (ImGui::BeginTabBar("MainTabs")) {
		if (ImGui::BeginTabItem("Replacer Mods")) {
			DrawReplacerModsTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Replacement Animations")) {
			DrawReplacementAnimsTab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

void UIMain::DrawReplacerModsTab()
{
	auto* oar = OpenAnimationReplacer::GetSingleton();

	// Hold the mods read-lock for the WHOLE tab (tree, details panel, and the
	// rename/description popups all dereference ReplacerMod/SubMod pointers).
	// The game-thread config reload takes the write lock in ClearAllMods, so
	// it cannot free those objects mid-draw. DrawModTree relies on this lock
	// being held by us.
	ReadLocker modsLock(oar->GetModsMutex());

	// A config reload destroyed and re-created every mod object: drop the raw
	// pointers we cached across frames before anything dereferences them.
	const auto gen = oar->GetModsGeneration();
	if (gen != lastModsGeneration) {
		lastModsGeneration = gen;
		selectedSubMod = nullptr;
		renamingSubMod = nullptr;
		editingDescSubMod = nullptr;
		creatingConfigSubMod = nullptr;
		creatingConfigMod = nullptr;
	}

	float availWidth = ImGui::GetContentRegionAvail().x;
	float firstColW = availWidth * firstColumnPercent;
	float secondColW = availWidth - firstColW - ImGui::GetStyle().ItemSpacing.x;

	float childHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() - 4;

	ImGui::BeginChild("ModTreeCol", ImVec2(firstColW, childHeight), true);
	DrawModTree();
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("DetailsCol", ImVec2(secondColW, childHeight), true);
	if (selectedSubMod) {
		DrawSubModDetails(selectedSubMod);
	} else {
		ImGui::TextDisabled("Select a SubMod from the tree to view details");
	}
	ImGui::EndChild();

	// Rename SubMod modal popup
	if (renamingSubMod) {
		ImGui::OpenPopup("Rename SubMod");
	}
	if (ImGui::BeginPopupModal("Rename SubMod", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Enter new name:");
		ImGui::SetNextItemWidth(300);
		bool submitted = ImGui::InputText("##RenameInput", renameBuffer, sizeof(renameBuffer),
			ImGuiInputTextFlags_EnterReturnsTrue);

		// Auto-focus the input on first frame
		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere(-1);
		}

		ImGui::Spacing();
		if (submitted || ImGui::Button("OK", ImVec2(120, 0))) {
			if (renamingSubMod && renameBuffer[0] != '\0') {
				renamingSubMod->SetName(renameBuffer);
				renamingSubMod->SetDirty(true);
				logger::info("[OAR-UI] SubMod renamed to '{}'", renameBuffer);
			}
			renamingSubMod = nullptr;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) {
			renamingSubMod = nullptr;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Edit Description modal popup (opened from the right-panel description
	// text via double-click or the right-click context menu).
	if (editingDescSubMod) {
		ImGui::OpenPopup("Edit Description");
	}
	if (ImGui::BeginPopupModal("Edit Description", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Description for '%s':",
			editingDescSubMod ? editingDescSubMod->GetName().c_str() : "");
		ImGui::SetNextItemWidth(420);
		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
		}
		ImGui::InputTextMultiline("##DescEditInput", descEditBuffer, sizeof(descEditBuffer),
			ImVec2(420, 120));

		ImGui::Spacing();
		if (ImGui::Button("OK", ImVec2(120, 0))) {
			if (editingDescSubMod) {
				editingDescSubMod->SetDescription(descEditBuffer);
				editingDescSubMod->SetDirty(true);
				logger::info("[OAR-UI] SubMod '{}' description updated", editingDescSubMod->GetName());
			}
			editingDescSubMod = nullptr;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) {
			editingDescSubMod = nullptr;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Create SubMod config.json modal. Writes a minimal config.json into the
	// folder and populates the live (already-loaded) SubMod object in place, so
	// the user can keep editing it without a full reload. hasConfig flips true
	// so the tree stops flagging it.
	if (creatingConfigSubMod) {
		ImGui::OpenPopup("Create SubMod Config");
	}
	if (ImGui::BeginPopupModal("Create SubMod Config", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextWrapped("Create config.json for this folder. Afterwards you can edit it (conditions, track filter, etc.) like any other submod.");
		ImGui::Spacing();

		ImGui::Text("Name:");
		ImGui::SetNextItemWidth(420);
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		ImGui::InputText("##CreateSubName", createNameBuffer, sizeof(createNameBuffer));

		ImGui::Text("Description:");
		ImGui::InputTextMultiline("##CreateSubDesc", createDescBuffer, sizeof(createDescBuffer), ImVec2(420, 90));

		ImGui::Text("Priority:");
		ImGui::SetNextItemWidth(120);
		ImGui::InputInt("##CreateSubPriority", &createPriorityValue);

		ImGui::Spacing();
		const bool canCreate = createNameBuffer[0] != '\0';
		if (!canCreate) ImGui::BeginDisabled();
		if (ImGui::Button("Create", ImVec2(120, 0))) {
			auto* sm = creatingConfigSubMod;
			// Populate the live object so editing continues seamlessly.
			sm->SetName(createNameBuffer);
			sm->SetDescription(createDescBuffer);
			sm->SetPriority(createPriorityValue);
			sm->hasConfig = true;

			// Minimal starter config: identity + an empty conditions array
			// (empty = always matches, matching the folder's current behavior).
			nlohmann::json json;
			json["name"] = createNameBuffer;
			json["description"] = createDescBuffer;
			json["priority"] = createPriorityValue;
			json["conditions"] = nlohmann::json::array();

			auto savePath = sm->GetPath() / "config.json";
			JobQueue::GetSingleton()->Enqueue(
				std::make_unique<SaveConfigJob>(savePath, std::move(json)));
			logger::info("[OAR-UI] Created config.json for '{}' at '{}'",
				createNameBuffer, savePath.string());

			selectedSubMod = sm;
			creatingConfigSubMod = nullptr;
			ImGui::CloseCurrentPopup();
		}
		if (!canCreate) ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) {
			creatingConfigSubMod = nullptr;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Create Mod config.json modal (name / author / description only, per
	// ParseModConfig).
	if (creatingConfigMod) {
		ImGui::OpenPopup("Create Mod Config");
	}
	if (ImGui::BeginPopupModal("Create Mod Config", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextWrapped("Create config.json for this mod folder. This names the mod group in the list; behavior lives in each submod's own config.");
		ImGui::Spacing();

		ImGui::Text("Name:");
		ImGui::SetNextItemWidth(420);
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		ImGui::InputText("##CreateModName", createNameBuffer, sizeof(createNameBuffer));

		ImGui::Text("Author:");
		ImGui::SetNextItemWidth(420);
		ImGui::InputText("##CreateModAuthor", createAuthorBuffer, sizeof(createAuthorBuffer));

		ImGui::Text("Description:");
		ImGui::InputTextMultiline("##CreateModDesc", createDescBuffer, sizeof(createDescBuffer), ImVec2(420, 90));

		ImGui::Spacing();
		const bool canCreateMod = createNameBuffer[0] != '\0';
		if (!canCreateMod) ImGui::BeginDisabled();
		if (ImGui::Button("Create", ImVec2(120, 0))) {
			auto* m = creatingConfigMod;
			m->SetName(createNameBuffer);
			m->SetAuthor(createAuthorBuffer);
			m->SetDescription(createDescBuffer);
			m->hasConfig = true;

			nlohmann::json json;
			json["name"] = createNameBuffer;
			json["author"] = createAuthorBuffer;
			json["description"] = createDescBuffer;

			auto savePath = m->GetPath() / "config.json";
			JobQueue::GetSingleton()->Enqueue(
				std::make_unique<SaveConfigJob>(savePath, std::move(json)));
			logger::info("[OAR-UI] Created mod config.json for '{}' at '{}'",
				createNameBuffer, savePath.string());

			creatingConfigMod = nullptr;
			ImGui::CloseCurrentPopup();
		}
		if (!canCreateMod) ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) {
			creatingConfigMod = nullptr;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void UIMain::BeginEditDescription(SubMod* a_subMod)
{
	if (!a_subMod) return;
	editingDescSubMod = a_subMod;
	strncpy_s(descEditBuffer, a_subMod->GetDescription().c_str(), sizeof(descEditBuffer) - 1);
}

void UIMain::BeginCreateSubModConfig(SubMod* a_subMod)
{
	if (!a_subMod) return;
	creatingConfigSubMod = a_subMod;
	creatingConfigMod = nullptr;
	// Prefill from what the folder already parsed to: name = folder name,
	// priority = the number parsed from the folder name (0 if not numeric).
	strncpy_s(createNameBuffer, a_subMod->GetName().c_str(), sizeof(createNameBuffer) - 1);
	createDescBuffer[0] = '\0';
	createPriorityValue = a_subMod->GetPriority();
}

void UIMain::BeginCreateModConfig(ReplacerMod* a_mod)
{
	if (!a_mod) return;
	creatingConfigMod = a_mod;
	creatingConfigSubMod = nullptr;
	strncpy_s(createNameBuffer, a_mod->GetName().c_str(), sizeof(createNameBuffer) - 1);
	strncpy_s(createAuthorBuffer, a_mod->GetAuthor().c_str(), sizeof(createAuthorBuffer) - 1);
	createDescBuffer[0] = '\0';
}

void UIMain::DrawReplacementAnimsTab()
{
	float childHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() - 4;
	ImGui::BeginChild("AnimList", ImVec2(0, childHeight), true);

	auto* oar = OpenAnimationReplacer::GetSingleton();
	ReadLocker modsLock(oar->GetModsMutex());

	for (const auto& mod : oar->GetReplacerMods()) {
		for (const auto& sub : mod->GetSubMods()) {
			auto& anims = sub->GetReplacementAnimations();
			if (anims.empty()) continue;

			bool hasFilterMatch = filterText[0] == '\0';
			if (!hasFilterMatch) {
				for (auto* a : anims) {
					if (a && (UICommon::FuzzyMatch(filterText, a->GetOriginalPath().c_str()) ||
					           UICommon::FuzzyMatch(filterText, a->GetReplacementPath().c_str()))) {
						hasFilterMatch = true;
						break;
					}
				}
			}
			if (!hasFilterMatch) continue;

			std::string header = std::format("[{}] {} / {}", sub->GetPriority(), mod->GetName(), sub->GetName());
			if (ImGui::CollapsingHeader(header.c_str())) {
				for (auto* anim : anims) {
					if (!anim) continue;
					if (filterText[0] != '\0' &&
						!UICommon::FuzzyMatch(filterText, anim->GetOriginalPath().c_str()) &&
						!UICommon::FuzzyMatch(filterText, anim->GetReplacementPath().c_str())) continue;

				std::string origShort = UICommon::ShortenAnimPath(anim->GetOriginalPath());
				std::string replShort = UICommon::ShortenAnimPath(anim->GetReplacementPath());
				ImGui::TextColored(UICommon::Colors::Disabled, "  %s", origShort.c_str());
				ImGui::SameLine();
				ImGui::TextUnformatted("->");
				ImGui::SameLine();
				ImGui::TextColored(UICommon::Colors::AccentBlue, "%s", replShort.c_str());
				ImGui::SameLine();
				ImGui::TextColored(UICommon::Colors::Disabled, "[%d]", anim->GetBindingIndex());
				}
			}
		}
	}

	ImGui::EndChild();
}

void UIMain::DrawModTree()
{
	// NOTE: the caller (DrawReplacerModsTab) holds the mods read-lock for the
	// whole tab — do not re-acquire it here (shared_mutex is not recursive;
	// a writer waiting between the two acquisitions would deadlock us).
	auto* oar = OpenAnimationReplacer::GetSingleton();
	auto& mods = oar->GetReplacerMods();

	ImGui::Text("%zu mods, %zu replacements", mods.size(), oar->GetTotalReplacementCount());
	ImGui::Separator();

	for (const auto& mod : mods) {
		if (filterText[0] != '\0') {
			bool anyMatch = UICommon::FuzzyMatch(filterText, mod->GetName().c_str()) ||
			                UICommon::FuzzyMatch(filterText, mod->GetAuthor().c_str());
			if (!anyMatch) {
				for (const auto& sub : mod->GetSubMods()) {
					if (UICommon::FuzzyMatch(filterText, sub->GetName().c_str())) {
						anyMatch = true;
						break;
					}
				}
			}
			if (!anyMatch) continue;
		}

		ImGui::PushID(mod.get());

		// Flag mods with no config.json on disk so the user knows the folder is
		// running under its raw folder name and can formalize it.
		const bool modNoConfig = !mod->hasConfig;
		std::string modLabel = mod->GetName();
		if (modNoConfig) modLabel += "  (no config)";
		if (modNoConfig) ImGui::PushStyleColor(ImGuiCol_Text, UICommon::Colors::Disabled);

		ImGuiTreeNodeFlags modFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
		bool modOpen = ImGui::TreeNodeEx(modLabel.c_str(), modFlags);

		if (modNoConfig) ImGui::PopStyleColor();

		if (ImGui::IsItemHovered() && !mod->GetDescription().empty()) {
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(mod->GetDescription().c_str());
			if (!mod->GetAuthor().empty()) {
				ImGui::TextColored(UICommon::Colors::Disabled, "Author: %s", mod->GetAuthor().c_str());
			}
			ImGui::EndTooltip();
		}

		// Right-click a mod folder: offer to create its config.json (name /
		// author / description) when it has none.
		if (ImGui::BeginPopupContextItem("ModContext")) {
			if (modNoConfig) {
				if (ImGui::MenuItem("Create config.json...")) {
					BeginCreateModConfig(mod.get());
				}
			} else {
				ImGui::TextDisabled("config.json present");
			}
			ImGui::EndPopup();
		}

		if (modOpen) {
			for (const auto& sub : mod->GetSubMods()) {
				DrawSubModNode(sub.get(), mod.get());
			}
			ImGui::TreePop();
		}

		ImGui::PopID();
	}
}

void UIMain::DrawSubModNode(SubMod* a_subMod, ReplacerMod* a_mod)
{
	if (!a_subMod) return;

	if (filterText[0] != '\0' &&
		!UICommon::FuzzyMatch(filterText, a_subMod->GetName().c_str()) &&
		!UICommon::FuzzyMatch(filterText, a_mod->GetName().c_str())) {
		return;
	}

	ImGui::PushID(a_subMod);

	bool isDisabled = a_subMod->IsDisabled();
	bool isDirty = a_subMod->IsDirty();
	bool isSelected = (selectedSubMod == a_subMod);

	ImVec4 textColor = isDisabled ? UICommon::Colors::Disabled :
	                   isDirty ? UICommon::Colors::Dirty :
	                   ImGui::GetStyleColorVec4(ImGuiCol_Text);
	ImGui::PushStyleColor(ImGuiCol_Text, textColor);

	const bool noConfig = !a_subMod->hasConfig;
	std::string label = std::format("[{}] {}", a_subMod->GetPriority(), a_subMod->GetName());
	if (isDirty) label += " *";
	if (a_subMod->hasUserConfig) label += " (User)";
	if (noConfig) label += "  (no config)";

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

	ImGui::TreeNodeEx(label.c_str(), flags);
	if (ImGui::IsItemClicked()) {
		selectedSubMod = a_subMod;
	}

	if (ImGui::IsItemHovered() && noConfig) {
		ImGui::SetTooltip(
			"This folder has no config.json. It loads under its folder name and\n"
			"matches unconditionally. Right-click to create a config and give it\n"
			"a name, description, and conditions.");
	}

	// Right-click context menu
	if (ImGui::BeginPopupContextItem("SubModContext")) {
		if (noConfig) {
			if (ImGui::MenuItem("Create config.json...")) {
				BeginCreateSubModConfig(a_subMod);
			}
			ImGui::Separator();
		}
		if (ImGui::MenuItem("Rename...")) {
			renamingSubMod = a_subMod;
			strncpy_s(renameBuffer, a_subMod->GetName().c_str(), sizeof(renameBuffer) - 1);
			ImGui::OpenPopup("RenameSubMod");
		}
		ImGui::Separator();
		bool dis = a_subMod->IsDisabled();
		if (ImGui::MenuItem(dis ? "Enable" : "Disable")) {
			a_subMod->SetDisabled(!dis);
			a_subMod->SetDirty(true);
		}
		ImGui::EndPopup();
	}

	ImGui::PopStyleColor();
	ImGui::PopID();
}

void UIMain::DrawSubModDetails(SubMod* a_subMod)
{
	if (!a_subMod) return;

	bool editable = currentMode != UICommon::EditorMode::kInspect;

	ImGui::TextColored(UICommon::Colors::AccentBlue, "%s", a_subMod->GetName().c_str());

	// Description: clickable in Author/Condition modes so a double-click or
	// right-click opens the edit popup. Empty descriptions still render a
	// placeholder so there's always something to interact with.
	ImGui::PushID("SubModDesc");
	if (!a_subMod->GetDescription().empty()) {
		ImGui::TextWrapped("%s", a_subMod->GetDescription().c_str());
	} else if (editable) {
		ImGui::TextDisabled("(no description; double-click or right-click to edit)");
	} else {
		ImGui::TextDisabled("(no description)");
	}
	if (editable) {
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Double-click or right-click to edit description");
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				BeginEditDescription(a_subMod);
			}
		}
		if (ImGui::BeginPopupContextItem("DescContext")) {
			if (ImGui::MenuItem("Edit Description...")) {
				BeginEditDescription(a_subMod);
			}
			ImGui::EndPopup();
		}
	}
	ImGui::PopID();

	ImGui::Separator();

	if (editable) {
		bool isDisabled = a_subMod->IsDisabled();
		if (ImGui::Checkbox("Disabled", &isDisabled)) {
			a_subMod->SetDisabled(isDisabled);
			a_subMod->SetDirty(true);
			logger::info("[OAR-UI] SubMod '{}' Disabled toggled -> {}", a_subMod->GetName(), isDisabled);
		}

		ImGui::SameLine(200);
		int priority = a_subMod->GetPriority();
		ImGui::SetNextItemWidth(100);
		if (ImGui::InputInt("Priority", &priority)) {
			a_subMod->SetPriority(priority);
			a_subMod->SetDirty(true);
			// The winner-selection lists are pre-sorted by priority; re-sort
			// them (on the game thread) so the change applies immediately
			// instead of only after a config reload.
			RequestLookupResort();
		}

		bool interruptible = a_subMod->IsInterruptible();
		if (ImGui::Checkbox("Interruptible (?)", &interruptible)) {
			a_subMod->SetInterruptible(interruptible);
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Re-evaluate conditions every frame while animation is playing");

		bool keepRandom = a_subMod->GetKeepRandomResultsOnLoop();
		if (ImGui::Checkbox("Keep random results on loop (?)", &keepRandom)) {
			a_subMod->SetKeepRandomResultsOnLoop(keepRandom);
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(
			"When a random variant is selected, keep playing the same one when\n"
			"the animation loops instead of re-rolling. Each actor remembers\n"
			"their own selected variant independently.\n\n"
			"Requires variant animation files (e.g. anim_1.hkx, anim_2.hkx)\n"
			"in the SubMod folder alongside the base animation.");

		bool shareRandom = a_subMod->GetShareRandomResults();
		if (ImGui::Checkbox("Share random results (?)", &shareRandom)) {
			a_subMod->SetShareRandomResults(shareRandom);
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(
			"All actors use the same randomly-selected variant instead of each\n"
			"actor rolling independently. Useful for synchronized group animations.\n\n"
			"Requires variant animation files (e.g. anim_1.hkx, anim_2.hkx)\n"
			"in the SubMod folder alongside the base animation.");

		bool replAnnot = a_subMod->GetReplaceAnnotations();
		if (ImGui::Checkbox("Replace Annotations (?)", &replAnnot)) {
			a_subMod->SetReplaceAnnotations(replAnnot);
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(
			"When ON: fires sounds/events from the replacement animation.\n"
			"When OFF: only replaces visuals — original animation's sounds and\n"
			"events (weaponFire, etc.) still fire at their native timings.\n"
			"Turn OFF for fire animations to prevent double-shots.");

		bool replOnLoop = a_subMod->GetReplaceOnLoop();
		if (ImGui::Checkbox("Replace on Loop (?)", &replOnLoop)) {
			a_subMod->SetReplaceOnLoop(replOnLoop);
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(
			"When the animation loops, re-evaluate conditions and apply a\n"
			"new replacement if one matches. If OFF, the original (non-replaced)\n"
			"animation plays on subsequent loops.");
		ImGui::SameLine();
		bool replOnEcho = a_subMod->GetReplaceOnEcho();
		if (ImGui::Checkbox("Replace on Echo (?)", &replOnEcho)) {
			a_subMod->SetReplaceOnEcho(replOnEcho);
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(
			"When the animation receives an 'echo' (game re-triggers the same clip\n"
			"without deactivating it first), re-evaluate conditions and apply a\n"
			"replacement. Common with idle animations.");

		bool playOnce = a_subMod->GetPlayOnceFullBody();
		if (ImGui::Checkbox("Lock Replacement Until Clip Ends (?)", &playOnce)) {
			a_subMod->SetPlayOnceFullBody(playOnce);
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(
			"Once this replacement starts playing, lock it in place — conditions\n"
			"are NOT re-evaluated until the clip naturally finishes or deactivates.\n"
			"Prevents mid-animation interruptions from game state changes.\n\n"
			"Use for reloads, one-shot animations, or any case where game state\n"
			"changes mid-animation (e.g. ammo count refilling during reload)\n"
			"would incorrectly interrupt the replacement.");

		bool holdShorter = a_subMod->GetEndClipIfShorter();
		if (ImGui::Checkbox("End Clip If Shorter (?)", &holdShorter)) {
			a_subMod->SetEndClipIfShorter(holdShorter);
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(
			"When the replacement animation is shorter than the original it replaces,\n"
			"the clip would otherwise keep running for the original's (longer)\n"
			"length — the original's tail plays out (full body) or the source clip\n"
			"runs on past the donor (track filter).\n\n"
			"Enable this to make the replacement's length authoritative: the clip\n"
			"ends when the replacement ends, as if the original were that length.\n"
			"No original tail, no held frame. Applies to full-body and track-filtered\n"
			"replacements; no effect when the replacement is equal length or longer.");

		bool leafMatch = a_subMod->GetLeafMatching();
		if (ImGui::Checkbox("Match By Filename (Leaf Matching) (?)", &leafMatch)) {
			a_subMod->SetLeafMatching(leafMatch);
			a_subMod->SetDirty(true);
			// Membership + probe order live in the leaf lookup tables — queue the
			// game-thread rebuild so the toggle applies immediately (same path as
			// a priority edit).
			RequestLookupResort();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(
			"Match this submod's animations by FILENAME alone, ignoring the folder\n"
			"path. A wpnmelee.hkx in this submod replaces ANY clip whose animation\n"
			"file is named wpnmelee.hkx — every weapon, every path — and it WINS\n"
			"over submods that registered the exact path, whenever this submod's\n"
			"conditions pass. When they fail, normal path matching applies as usual.\n\n"
			"Use this to cover a whole family of per-weapon animation files with one\n"
			"submod instead of mirroring every weapon's folder. Gate it with\n"
			"conditions — with no conditions it replaces every clip that shares the\n"
			"filename.");

		// Custom blend times
		float blendInterrupt = a_subMod->GetCustomBlendTimeOnInterrupt();
		ImGui::SetNextItemWidth(80);
		if (ImGui::InputFloat("Blend time (interrupt) (?)", &blendInterrupt, 0, 0, "%.2f")) {
			a_subMod->customBlendTimeOnInterrupt = blendInterrupt;
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Custom blend duration in seconds when interrupting. Default: 0.20s. Set negative to use default.");

		float blendOut = a_subMod->GetCustomBlendOutTime();
		ImGui::SetNextItemWidth(80);
		if (ImGui::InputFloat("Blend out time (?)", &blendOut, 0, 0, "%.2f")) {
			a_subMod->customBlendOutTime = blendOut;
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(
			"Full-body blend duration in seconds when this replacement ends\n"
			"(conditions become false). Set negative to use the blend-in time\n"
			"above (default behavior). 0 = instant snap back to the original.");

		int curveIdx = static_cast<int>(a_subMod->blendCurve);
		ImGui::SetNextItemWidth(140);
		if (ImGui::Combo("Blend curve (?)", &curveIdx,
				"Linear\0Quadratic\0Cubic\0Hermite Cubic\0Sinusoidal\0Exponential\0")) {
			a_subMod->blendCurve = static_cast<BlendCurve>(curveIdx);
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(
			"Shape of the blend ramp, for both the full-body blend and this\n"
			"submod's track filter blend in/out.\n\n"
			"Linear: constant rate.\n"
			"Quadratic: gentle ease in and out (default).\n"
			"Cubic: stronger ease, more time spent near the endpoints.\n"
			"Hermite Cubic: smoothstep — flat at both ends, smoothest handoff.\n"
			"Sinusoidal: cosine ramp, very soft.\n"
			"Exponential: near-flat then sharp, for snappy transitions.");

		float deactivDelay = a_subMod->GetDeactivationDelay();
		ImGui::SetNextItemWidth(80);
		if (ImGui::InputFloat("Deactivation Delay (?)", &deactivDelay, 0, 0, "%.2f")) {
			if (deactivDelay < 0.0f) deactivDelay = 0.0f;
			a_subMod->SetDeactivationDelay(deactivDelay);
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Seconds to keep the replacement active after conditions become false, before blend-out begins. 0 = disabled (immediate).");

		// --- Custom Animation Events ---
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("Custom Animation Events");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(
			"Behavior events to force-fire at the start or end of this replacement.\n"
			"Useful for ensuring state machine transitions (e.g. ReloadEnd, SprintStop)\n"
			"fire correctly even when original triggers are suppressed.");

		static const char* s_commonEvents[] = {
			"ReloadComplete", "ReloadEnd", "reloadStart", "reloadState",
			"reloadStateEnter", "reloadStateExit",
			"AttackEnd", "attackStart", "attackStop", "attackRelease",
			"meleeEnd", "meleeStart", "EndMeleeAttack",
			"SprintStart", "SprintStop", "MoveStart", "MoveStop",
			"jumpStart", "jumpEnd", "jumpLand",
			"WeaponFire", "FireSingle", "weaponDraw", "weaponSheath",
			"WeapEquip", "weapUnequip",
			"sneakStart", "sneakStop",
			"GunDown", "GunUp",
			"EjectShellCasing", "Recoil",
		"CullBone", "UncullBone",
			"idleLoopingStart", "idleLoopingExit", "IdleStop",
			"blockStart", "blockStop", "blockEnd",
			"staggerExit", "staggerStop",
			"DoNotInterrupt", "EarlyExit", "InstantExitClip",
			"FootLeft", "FootRight", "FootDown",
			"sightedStateEnter", "sightedStateExit",
			"initiateBoltStart", "initiateEnd", "initiateStart",
			"grenadeThrowStart", "throwEnd",
		};
		static const int s_numCommonEvents = sizeof(s_commonEvents) / sizeof(s_commonEvents[0]);

		auto DrawEventList = [&](const char* label, std::vector<std::string>& events, const char* id) {
			ImGui::PushID(id);
			ImGui::Text("%s:", label);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Events fire in order from top to bottom.");

			for (int i = 0; i < (int)events.size(); ++i) {
				ImGui::PushID(i);
				ImGui::Text("  %d. %s", i + 1, events[i].c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton("X")) {
					events.erase(events.begin() + i);
					a_subMod->SetDirty(true);
					ImGui::PopID();
					break;
				}
				if (i > 0) {
					ImGui::SameLine();
					if (ImGui::SmallButton("^")) {
						std::swap(events[i], events[i - 1]);
						a_subMod->SetDirty(true);
					}
				}
				if (i < (int)events.size() - 1) {
					ImGui::SameLine();
					if (ImGui::SmallButton("v")) {
						std::swap(events[i], events[i + 1]);
						a_subMod->SetDirty(true);
					}
				}
				ImGui::PopID();
			}

			// Dropdown for common events
			static int selectedCommon = 0;
			ImGui::SetNextItemWidth(200);
			ImGui::Combo("##common", &selectedCommon, s_commonEvents, s_numCommonEvents);

			// CullBone / UncullBone are bone-targeted: the engine dispatches
			// annotation events split at the first '.' into tag + payload, so
			// these need a bone argument ("CullBone.WeaponMagazine"). OAR's
			// event emission performs the same split (see NotifyEventSinks in
			// Hooks.cpp), so the composed dotted name reaches the engine's
			// cull handler exactly like a native annotation. Show a bone
			// picker instead of adding the bare selector.
			const char* selEvtName = s_commonEvents[selectedCommon];
			const bool wantsBone = std::strcmp(selEvtName, "CullBone") == 0 ||
			                       std::strcmp(selEvtName, "UncullBone") == 0;
			if (!wantsBone) {
				ImGui::SameLine();
				if (ImGui::Button("Add from List")) {
					events.push_back(selEvtName);
					a_subMod->SetDirty(true);
				}
			} else {
				ImGui::Indent(16.f);
				ImGui::TextDisabled("%s which bone?", std::strcmp(selEvtName, "CullBone") == 0 ? "Hide (cull)" : "Show (uncull)");
				static int selectedCullBone = 0;
				ImGui::SetNextItemWidth(180);
				ImGui::Combo("##cullBoneList", &selectedCullBone, kKnownBones, kNumKnownBones);
				ImGui::SameLine();
				static char cullBoneBuf[128] = "";
				ImGui::SetNextItemWidth(160);
				ImGui::InputTextWithHint("##cullBoneTyped", "...or type a bone name", cullBoneBuf, sizeof(cullBoneBuf));
				ImGui::SameLine();
				if (ImGui::Button("Add##cullBoneAdd")) {
					// Typed name wins over the list selection when present.
					const char* bone = cullBoneBuf[0] != '\0' ? cullBoneBuf : kKnownBones[selectedCullBone];
					events.push_back(std::string(selEvtName) + "." + bone);
					a_subMod->SetDirty(true);
					cullBoneBuf[0] = '\0';
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(
						"Adds '%s.<bone>' — hides/shows every mesh attached to that\n"
						"bone, same as the CullBone/UncullBone annotations weapon\n"
						"reload animations use for magazines. A typed name overrides\n"
						"the list selection.", selEvtName);
				}
				ImGui::Unindent(16.f);
			}

			// Manual text entry
			static char customBuf[128] = "";
			ImGui::SetNextItemWidth(200);
			ImGui::InputText("##custom", customBuf, sizeof(customBuf));
			ImGui::SameLine();
			if (ImGui::Button("Add Custom")) {
				std::string s(customBuf);
				if (!s.empty()) {
					events.push_back(s);
					a_subMod->SetDirty(true);
					customBuf[0] = '\0';
				}
			}
			ImGui::PopID();
		};

		DrawEventList("Events on Start", a_subMod->eventsOnStart, "evtStart");
		ImGui::Spacing();
		DrawEventList("Events on End", a_subMod->eventsOnEnd, "evtEnd");

		// --- Annotation suppression ---
		ImGui::Spacing();
		bool suppressAll = a_subMod->suppressAllAnnotations;
		if (ImGui::Checkbox("Suppress ALL annotations (?)", &suppressAll)) {
			a_subMod->suppressAllAnnotations = suppressAll;
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(
			"Mute every annotation of the replacement file while this SubMod's\n"
			"replacement plays — no sounds or events are fired by OAR.\n"
			"Requires 'Replace Annotations' ON (that is the mode where OAR\n"
			"controls annotation emission).");

		if (!a_subMod->suppressAllAnnotations) {
			ImGui::PushID("suppressAnnot");
			ImGui::Text("Suppressed annotations (?):");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip(
				"Annotations with these exact names (case-insensitive) are muted\n"
				"when the replacement plays. Example: add 'WeaponFire' for a\n"
				"dry-fire animation whose source file still carries the fire\n"
				"annotation. Full text must match, e.g. 'SoundPlay.WPNRifleFire'.");
			for (int i = 0; i < (int)a_subMod->suppressedAnnotations.size(); ++i) {
				ImGui::PushID(i);
				ImGui::Text("  %s", a_subMod->suppressedAnnotations[i].c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton("X")) {
					a_subMod->suppressedAnnotations.erase(a_subMod->suppressedAnnotations.begin() + i);
					a_subMod->SetDirty(true);
					ImGui::PopID();
					break;
				}
				ImGui::PopID();
			}
			static char suppressBuf[128] = "";
			ImGui::SetNextItemWidth(200);
			ImGui::InputTextWithHint("##suppressCustom", "Annotation name (e.g. WeaponFire)",
				suppressBuf, sizeof(suppressBuf));
			ImGui::SameLine();
			if (ImGui::Button("Add##suppress")) {
				std::string s(suppressBuf);
				if (!s.empty()) {
					a_subMod->suppressedAnnotations.push_back(s);
					a_subMod->SetDirty(true);
					suppressBuf[0] = '\0';
				}
			}
			ImGui::PopID();
		}

	} else {
		ImGui::Text("Priority: %d", a_subMod->GetPriority());
		ImGui::SameLine(200);
		ImGui::Text("Disabled: %s", a_subMod->IsDisabled() ? "Yes" : "No");
		ImGui::Text("Interruptible: %s  |  Loop: %s  |  Echo: %s",
			a_subMod->IsInterruptible() ? "Yes" : "No",
			a_subMod->GetReplaceOnLoop() ? "Yes" : "No",
			a_subMod->GetReplaceOnEcho() ? "Yes" : "No");
		ImGui::Text("Keep random on loop: %s  |  Share random: %s",
			a_subMod->GetKeepRandomResultsOnLoop() ? "Yes" : "No",
			a_subMod->GetShareRandomResults() ? "Yes" : "No");
		if (a_subMod->GetPlayOnceFullBody())
			ImGui::Text("Lock Replacement Until Clip Ends: Yes");
		if (a_subMod->GetDeactivationDelay() > 0.0f)
			ImGui::Text("Deactivation delay: %.2fs", a_subMod->GetDeactivationDelay());
		if (!a_subMod->eventsOnStart.empty()) {
			std::string startStr = "Events on Start:";
			for (auto& e : a_subMod->eventsOnStart) startStr += " " + e;
			ImGui::Text("%s", startStr.c_str());
		}
		if (!a_subMod->eventsOnEnd.empty()) {
			std::string endStr = "Events on End:";
			for (auto& e : a_subMod->eventsOnEnd) endStr += " " + e;
			ImGui::Text("%s", endStr.c_str());
		}
		if (a_subMod->suppressAllAnnotations) {
			ImGui::Text("Suppress annotations: ALL");
		} else if (!a_subMod->suppressedAnnotations.empty()) {
			std::string supStr = "Suppressed annotations:";
			for (auto& s : a_subMod->suppressedAnnotations) supStr += " " + s;
			ImGui::Text("%s", supStr.c_str());
		}
	}

	// --- Track Filter (partial body animation layering) ---
	ImGui::Spacing();
	if (ImGui::CollapsingHeader("Track Filter (Partial Body Layering)")) {
		DrawTrackFilterSection(a_subMod, editable);
	}

	// --- Replacement Animations (collapsed by default, under submod like original) ---
	ImGui::Spacing();
	if (ImGui::CollapsingHeader("Replacement Animations")) {
		DrawReplacementAnimList(a_subMod);
	}

	// --- Conditions ---
	ImGui::Spacing();
	if (ImGui::CollapsingHeader("Conditions", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Spacing();

		if (auto* condSet = a_subMod->GetConditionSet()) {
			RE::TESObjectREFR* evalTarget = nullptr;
			if (evalTargetFormID != 0) {
				evalTarget = RE::TESForm::GetFormByID<RE::TESObjectREFR>(evalTargetFormID);
			}
			if (!evalTarget) {
				evalTarget = RE::PlayerCharacter::GetSingleton();
			}
			if (evalTarget) {
				for (const auto& cond : condSet->GetConditions()) {
					if (cond) cond->Evaluate(evalTarget, nullptr, a_subMod);
				}
			}
		}

		DrawConditionSet(a_subMod->GetConditionSet(), a_subMod, 0);
	}

	if (editable) {
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (a_subMod->IsDirty()) {
			if (ImGui::Button("Save user config")) {
				nlohmann::json json;
				json["name"] = a_subMod->GetName();
				json["description"] = a_subMod->GetDescription();
				json["priority"] = a_subMod->GetPriority();
				json["disabled"] = a_subMod->IsDisabled();
				json["interruptible"] = a_subMod->IsInterruptible();
				json["replaceOnLoop"] = a_subMod->GetReplaceOnLoop();
				json["replaceOnEcho"] = a_subMod->GetReplaceOnEcho();
				json["keepRandomResultsOnLoop"] = a_subMod->GetKeepRandomResultsOnLoop();
				json["shareRandomResults"] = a_subMod->GetShareRandomResults();
				json["replaceAnnotations"] = a_subMod->GetReplaceAnnotations();
			// Round-trip annotation suppression so a UI save can't drop it.
			if (a_subMod->suppressAllAnnotations)
				json["suppressAnnotations"] = true;
			else if (!a_subMod->suppressedAnnotations.empty())
				json["suppressAnnotations"] = a_subMod->suppressedAnnotations;
			if (a_subMod->GetCustomBlendTimeOnInterrupt() >= 0.f)
				json["customBlendTimeOnInterrupt"] = a_subMod->GetCustomBlendTimeOnInterrupt();
			if (a_subMod->GetCustomBlendOutTime() >= 0.f)
				json["customBlendOutTime"] = a_subMod->GetCustomBlendOutTime();
				{
					static constexpr const char* kCurveNames[] = {
						"linear", "quadratic", "cubic", "hermiteCubic", "sinusoidal", "exponential"
					};
					const auto idx = static_cast<size_t>(a_subMod->blendCurve);
					json["blendCurve"] = kCurveNames[idx < std::size(kCurveNames) ? idx : 1];
				}
			if (a_subMod->GetDeactivationDelay() > 0.0f)
				json["deactivationDelay"] = a_subMod->GetDeactivationDelay();
			if (a_subMod->GetPlayOnceFullBody())
				json["playOnceFullBody"] = true;
			if (a_subMod->GetEndClipIfShorter())
				json["endClipIfShorter"] = true;
			if (a_subMod->GetLeafMatching())
				json["leafMatching"] = true;
			if (!a_subMod->eventsOnStart.empty())
				json["eventsOnStart"] = a_subMod->eventsOnStart;
			if (!a_subMod->eventsOnEnd.empty())
				json["eventsOnEnd"] = a_subMod->eventsOnEnd;

			{
				auto& tf = a_subMod->trackFilter;
				nlohmann::json tfJson;
				tfJson["enabled"] = tf.enabled;
				tfJson["mode"] = (tf.mode == SubMod::TrackFilter::Mode::Override) ? "override" : "additive";
				tfJson["weight"] = tf.weight;
				tfJson["blendInTime"] = tf.blendInTime;
				tfJson["blendOutTime"] = tf.blendOutTime;
				tfJson["blendOutAtEnd"] = tf.blendOutAtEnd;
				tfJson["sampleFrame"] = tf.sampleFrame;
				tfJson["modelSpaceAnchor"] = tf.modelSpaceAnchor;
				tfJson["includeChildren"] = tf.includeChildren;
				tfJson["bones"] = tf.boneNames;
				tfJson["excludeChildren"] = tf.excludeChildren;
				tfJson["excludeBones"] = tf.excludeBoneNames;
				tfJson["freezeBones"] = tf.freezeBoneNames;
				json["trackFilter"] = tfJson;
			}

			// Serialize variant configuration
			{
				bool hasVariants = false;
				for (auto* ra : a_subMod->GetReplacementAnimations()) {
					if (ra && ra->HasVariants()) { hasVariants = true; break; }
				}
				if (hasVariants) {
					nlohmann::json varJson;
					varJson["enabled"] = a_subMod->variantsEnabled;
					varJson["mode"] = (a_subMod->variantMode == VariantMode::kSequential) ? "sequential" : "random";
					varJson["rerollPolicy"] = (a_subMod->variantRerollPolicy == VariantRerollPolicy::kWhileActive) ? "whileActive" : "onEachPlay";
					nlohmann::json weightsJson = nlohmann::json::object();
					for (auto* ra : a_subMod->GetReplacementAnimations()) {
						if (!ra || !ra->HasVariants()) continue;
						for (auto& ve : ra->GetVariants()->GetEntries()) {
							weightsJson[ve.filename] = ve.weight;
						}
					}
					if (!weightsJson.empty())
						varJson["weights"] = weightsJson;
					json["variants"] = varJson;
				}
			}

			if (auto* condSet = a_subMod->GetConditionSet()) {
					auto& arr = json["conditions"];
					arr = nlohmann::json::array();
					for (const auto& cond : condSet->GetConditions()) {
						nlohmann::json condJson;
						cond->Serialize(condJson);
						arr.push_back(condJson);
					}
				}

				std::string filename = (currentMode == UICommon::EditorMode::kUser)
					? "user.json" : "config.json";
				auto savePath = a_subMod->GetPath() / filename;
				JobQueue::GetSingleton()->Enqueue(
					std::make_unique<SaveConfigJob>(savePath, std::move(json)));
				a_subMod->SetDirty(false);
			}
			ImGui::SameLine();
		}

		ImGui::SameLine();
		if (ImGui::Button("Reload config")) {
			RequestConfigReload();
		}

		if (a_subMod->hasUserConfig) {
			ImGui::SameLine();
			if (UICommon::ButtonWithConfirmationModal(
				"Delete user config", "Confirm Delete",
				"Are you sure you want to delete the user config?"))
			{
				auto userPath = a_subMod->GetPath() / "user.json";
				try { std::filesystem::remove(userPath); } catch (...) {}
				RequestConfigReload();
			}
		}
	}
}

void UIMain::DrawConditionSet(ConditionSet* a_condSet, SubMod* a_subMod, int a_depth)
{
	if (!a_condSet) return;

	bool editable = currentMode != UICommon::EditorMode::kInspect;

	if (a_condSet->IsEmpty()) {
		UICommon::TextUnformattedDisabled("No conditions (always matches)");
		// Do NOT return in edit mode: the "Add new condition" button below is
		// the only way to put the FIRST child into an OR/AND/XOR/TARGET/PLAYER
		// set — returning here made freshly-added composites impossible to
		// fill from the UI.
		if (!editable) {
			return;
		}
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImGuiStyle& style = ImGui::GetStyle();
	ImVec2 lineStart = ImGui::GetCursorScreenPos();
	lineStart.x -= style.IndentSpacing * 0.6f;
	lineStart.y += style.FramePadding.y;
	ImVec2 lineEnd = lineStart;

	int index = 0;
	bool conditionDeleted = false;
	auto& condVec = a_condSet->GetConditions();
	for (size_t i = 0; i < condVec.size(); ++i) {
		ImVec2 beforePos = ImGui::GetCursorScreenPos();
		DrawCondition(condVec[i].get(), a_condSet, static_cast<int>(i), a_subMod, a_depth);

		if (condVec.size() <= i || !condVec[i]) {
			conditionDeleted = true;
			break;
		}

		ImVec2 afterPos = ImGui::GetCursorScreenPos();

		if (a_depth > 0) {
			float midY = (beforePos.y + afterPos.y) * 0.5f;
			drawList->AddLine(ImVec2(lineStart.x, midY), ImVec2(lineStart.x + 10.f, midY),
				ImGui::GetColorU32(UICommon::Colors::TreeLine));
			lineEnd.y = midY;
		}

		index++;
	}

	if (a_depth > 0 && index > 0) {
		drawList->AddLine(lineStart, lineEnd, ImGui::GetColorU32(UICommon::Colors::TreeLine));
	}

	if (editable) {
		ImGui::Spacing();
		if (ImGui::SmallButton("Add new condition")) {
			ImGui::OpenPopup("AddCondition");
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Condition set...")) {
			ImGui::OpenPopup("CondSetMenu");
		}
		if (ImGui::BeginPopup("CondSetMenu")) {
			if (ImGui::MenuItem("Copy all conditions")) {
				nlohmann::json arr = nlohmann::json::array();
				for (const auto& cond : a_condSet->GetConditions()) {
					nlohmann::json j;
					cond->Serialize(j);
					arr.push_back(j);
				}
				copiedConditionJson = arr.dump();
			}
			if (ImGui::MenuItem("Paste conditions", nullptr, false, !copiedConditionJson.empty())) {
				try {
					auto parsed = nlohmann::json::parse(copiedConditionJson);
					if (parsed.is_array()) {
						for (auto& elem : parsed) {
							auto newCond = CreateConditionFromJson(elem);
							if (newCond) a_condSet->AddCondition(std::move(newCond));
						}
					} else {
						auto newCond = CreateConditionFromJson(parsed);
						if (newCond) a_condSet->AddCondition(std::move(newCond));
					}
					a_subMod->SetDirty(true);
				} catch (...) {}
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Clear all")) {
				a_condSet->ClearConditions();
				a_subMod->SetDirty(true);
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("AddCondition")) {
			static char condFilter[128]{};
			ImGui::InputTextWithHint("##condSearch", "Search...", condFilter, sizeof(condFilter));

			auto* factory = ConditionFactory::GetSingleton();
			for (const auto& [name, fn] : factory->GetAllFactories()) {
				if (condFilter[0] != '\0' && !UICommon::FuzzyMatch(condFilter, name.c_str())) continue;
				auto tempCond = fn();
				bool condIsStub = tempCond && tempCond->IsStub();
				std::string displayName = condIsStub ? name + "  [N/A]" : name;
				if (condIsStub) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.1f, 1.0f));
				if (ImGui::MenuItem(displayName.c_str())) {
					a_condSet->AddCondition(fn());
					a_subMod->SetDirty(true);
					condFilter[0] = '\0';
				}
				if (condIsStub) {
					ImGui::PopStyleColor();
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("%s", tempCond->GetStubReason().c_str());
					}
				}
			}
			ImGui::EndPopup();
		}
	}
}

void UIMain::DrawCondition(ICondition* a_condition, ConditionSet* a_parentSet, int a_index, SubMod* a_subMod, int a_depth)
{
	if (!a_condition) return;

	ImGui::PushID(a_index);

	bool editable = currentMode != UICommon::EditorMode::kInspect;
	bool isNegated = a_condition->IsNegated();
	bool isDisabled = a_condition->IsDisabled();
	bool hasValue = a_condition->lastEvalResult.has_value();
	bool evalResult = a_condition->lastEvalResult.value_or(false);

	// Every composite condition type that owns a child ConditionSet. XOR and
	// the TARGET/PLAYER context wrappers were missing here — their children
	// were invisible in the UI (and could never be added), even though parsing
	// and evaluation handled them fine.
	ConditionSet* childSet = nullptr;
	if (auto* orCond = dynamic_cast<ORCondition*>(a_condition)) {
		childSet = &orCond->GetConditionSet();
	} else if (auto* andCond = dynamic_cast<ANDCondition*>(a_condition)) {
		childSet = &andCond->GetConditionSet();
	} else if (auto* xorCond = dynamic_cast<XORCondition*>(a_condition)) {
		childSet = &xorCond->GetConditionSet();
	} else if (auto* targetWrap = dynamic_cast<TargetConditionWrapper*>(a_condition)) {
		childSet = &targetWrap->GetConditionSet();
	} else if (auto* playerWrap = dynamic_cast<PlayerConditionWrapper*>(a_condition)) {
		childSet = &playerWrap->GetConditionSet();
	}
	bool hasChildren = childSet != nullptr;

	std::string condName = a_condition->GetName();
	if (isNegated) condName = "NOT " + condName;
	std::string paramStr = a_condition->GetParameterString();

	bool bStyleVarPushed = false;
	if (isDisabled) {
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * ImGui::GetStyle().DisabledAlpha);
		bStyleVarPushed = true;
	}

	std::string tableId = std::format("##condTbl{}", a_index);
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
	if (ImGui::BeginTable(tableId.c_str(), 1, ImGuiTableFlags_BordersOuter)) {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);

		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;
		bool canExpand = hasChildren || editable;
		if (!canExpand) {
			nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		bool nodeOpen = ImGui::TreeNodeEx(a_condition, nodeFlags, "");

		if (ImGui::IsItemHovered()) {
			auto desc = a_condition->GetDescription();
			if (!desc.empty()) {
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.f);
				ImGui::TextUnformatted(desc.c_str());
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
		}

		// Right-click on the tree node row (must check before drawing the checkbox)
		bool wantsContextMenu = editable && ImGui::IsItemClicked(ImGuiMouseButton_Right);

		if (editable) {
			ImGui::SameLine();
			bool bEnabled = !isDisabled;
			if (ImGui::Checkbox("##toggleCond", &bEnabled)) {
				a_condition->SetDisabled(!bEnabled);
				a_subMod->SetDirty(true);
				logger::info("[OAR-UI] Condition '{}' on SubMod '{}' enabled toggled -> {}",
					a_condition->GetName(), a_subMod->GetName(), bEnabled);
			}
		}

		if (wantsContextMenu) {
			ImGui::OpenPopup("ConditionContextMenu");
		}
		bool deletedViaContextMenu = false;
		if (ImGui::BeginPopup("ConditionContextMenu")) {
			if (ImGui::MenuItem("Copy condition")) {
				nlohmann::json condJson;
				a_condition->Serialize(condJson);
				copiedConditionJson = condJson.dump();
			}
			if (ImGui::MenuItem("Paste condition", nullptr, false, !copiedConditionJson.empty())) {
				try {
					auto parsed = nlohmann::json::parse(copiedConditionJson);
					auto newCond = CreateConditionFromJson(parsed);
					if (newCond) {
						a_parentSet->AddCondition(std::move(newCond));
						a_subMod->SetDirty(true);
					}
				} catch (...) {}
			}
			if (ImGui::MenuItem("Duplicate")) {
				nlohmann::json condJson;
				a_condition->Serialize(condJson);
				auto newCond = CreateConditionFromJson(condJson);
				if (newCond) {
					a_parentSet->AddCondition(std::move(newCond));
					a_subMod->SetDirty(true);
				}
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Negate")) {
				a_condition->SetNegated(!a_condition->IsNegated());
				a_subMod->SetDirty(true);
			}
			if (ImGui::MenuItem("Delete")) {
				a_parentSet->RemoveCondition(a_index);
				a_subMod->SetDirty(true);
				deletedViaContextMenu = true;
			}
			ImGui::EndPopup();
		}
		if (deletedViaContextMenu) {
			ImGui::EndTable();
			ImGui::PopStyleVar();
			if (bStyleVarPushed) ImGui::PopStyleVar();
			ImGui::PopID();
			return;
		}

		ImGui::SameLine();
		bool isStub = a_condition->IsStub();
		ImVec4 textColor = isStub ? ImVec4(0.6f, 0.4f, 0.1f, 1.0f) :
		                   isNegated ? UICommon::Colors::CondNegated :
		                   ImGui::GetStyleColorVec4(ImGuiCol_Text);
		ImGui::PushStyleColor(ImGuiCol_Text, textColor);
		ImGui::TextUnformatted(condName.c_str());
		ImGui::PopStyleColor();

		if (isStub) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.1f, 1.0f), "[NOT IMPLEMENTED]");
			if (ImGui::IsItemHovered()) {
				std::string reason = a_condition->GetStubReason();
				if (!reason.empty()) {
					ImGui::SetTooltip("%s", reason.c_str());
				}
			}
		}

		float secondColX = (ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX()) * firstColumnPercent;
		ImGui::SameLine(secondColX);
		ImGui::TextUnformatted(paramStr.c_str());

		UICommon::DrawConditionEvalResult(evalResult, hasValue);

		if (nodeOpen) {
			ImGui::Spacing();

			bool deletedViaButton = false;
			if (editable) {
				bool bNOT = a_condition->IsNegated();
				if (ImGui::Checkbox("Negate", &bNOT)) {
					a_condition->SetNegated(bNOT);
					a_subMod->SetDirty(true);
				}

				ImGui::SameLine(secondColX);
				if (ImGui::Button("Delete condition")) {
					a_parentSet->RemoveCondition(a_index);
					a_subMod->SetDirty(true);
					deletedViaButton = true;
				}
			}

			if (!deletedViaButton) {
				if (childSet) {
					ImGui::Indent();
					DrawConditionSet(childSet, a_subMod, a_depth + 1);
					ImGui::Unindent();
				}

				if (editable) {
					bool dirty = false;
					a_condition->DrawEditWidgets(dirty);
					if (dirty) a_subMod->SetDirty(true);
				}
			}

			if (canExpand) {
				ImGui::TreePop();
			}
		}

		ImGui::EndTable();
	}
	ImGui::PopStyleVar();

	if (bStyleVarPushed) {
		ImGui::PopStyleVar();
	}

	ImGui::PopID();
}

// Session-only debug buttons drawn next to each bone in the track filter
// lists: a green-mesh highlight toggle and a 3-state 3D name label
// (off -> joint position -> attached mesh position). See BoneDebugViz.
static void DrawBoneDebugButtons(const std::string& a_boneName)
{
	const bool highlighted = BoneDebugViz::IsHighlighted(a_boneName);
	if (highlighted) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.55f, 0.10f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.70f, 0.15f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.85f, 0.20f, 1.0f));
	}
	if (ImGui::SmallButton("HL##boneHighlight")) {
		BoneDebugViz::ToggleHighlight(a_boneName);
	}
	if (highlighted) {
		ImGui::PopStyleColor(3);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Highlight every mesh attached under this node in bright green\n"
			"(emissive tint). Click again to restore the original appearance.\n"
			"Not saved — resets when the game closes.");
	}

	ImGui::SameLine();
	const int labelMode = BoneDebugViz::GetLabelMode(a_boneName);
	if (labelMode != 0) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.40f, 0.60f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.50f, 0.75f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.60f, 0.90f, 1.0f));
	}
	const char* labelText = labelMode == 0 ? "Tag##boneLabel" :
	                        labelMode == 1 ? "Tag:J##boneLabel" :
	                                         "Tag:M##boneLabel";
	if (ImGui::SmallButton(labelText)) {
		BoneDebugViz::CycleLabel(a_boneName);
	}
	if (labelMode != 0) {
		ImGui::PopStyleColor(3);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Show this bone's name as floating 3D text in the world.\n"
			"Click cycles: off -> at the joint (J) -> at the attached mesh (M) -> off.\n"
			"Not saved — resets when the game closes.");
	}
}

void UIMain::DrawTrackFilterSection(SubMod* a_subMod, bool a_editable)
{
	if (!a_subMod) return;

	auto& tf = a_subMod->trackFilter;

	// Bone list: file-scope kKnownBones (shared with the custom events UI).

	ImGui::Indent(8.f);

	if (a_editable) {
		if (ImGui::Checkbox("Enabled##trackFilter", &tf.enabled)) {
			a_subMod->SetDirty(true);
			// The file redirect map excludes track-filtered submods (their
			// files are pose donors, not file replacements). Rebuild it now
			// so toggling the filter takes effect without a config reload.
			Hooks::FileRedirectHooks::BuildFileRedirectMap();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"When enabled, this submod's replacement animations are applied\n"
				"only to the specified bones instead of replacing the full animation.\n"
				"The filtered bones are blended on top of whatever base animation\n"
				"is currently playing.");
		}

		if (tf.enabled) {
			ImGui::Spacing();

			int modeInt = (tf.mode == SubMod::TrackFilter::Mode::Override) ? 0 : 1;
			ImGui::SetNextItemWidth(140);
			if (ImGui::Combo("Blend Mode##trackFilter", &modeInt, "Override\0Additive\0")) {
				tf.mode = (modeInt == 0) ? SubMod::TrackFilter::Mode::Override : SubMod::TrackFilter::Mode::Additive;
				a_subMod->SetDirty(true);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Override: replacement bone transforms directly replace the base pose (lerped by weight).\n"
					"Additive: replacement transforms are added on top of the base pose.");
			}

			ImGui::SetNextItemWidth(200);
			if (ImGui::SliderFloat("Weight##trackFilter", &tf.weight, 0.0f, 1.0f, "%.2f")) {
				a_subMod->SetDirty(true);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Blend strength. 0 = no effect, 1 = full replacement/additive strength.");
			}

			if (ImGui::SliderFloat("Blend In##trackFilter", &tf.blendInTime, 0.0f, 2.0f, "%.2f s")) {
				a_subMod->SetDirty(true);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Time in seconds to ramp from 0 to full weight when conditions become true.\n0 = instant snap.");
			}

			if (ImGui::SliderFloat("Blend Out##trackFilter", &tf.blendOutTime, 0.0f, 2.0f, "%.2f s")) {
				a_subMod->SetDirty(true);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Time in seconds to ramp from full weight to 0 when conditions become false,\nor when a one-shot animation ends.\n0 = instant snap.");
			}

			if (ImGui::Checkbox("Blend Out After End##trackFilter", &tf.blendOutAtEnd)) {
				a_subMod->SetDirty(true);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Controls where Blend Out sits relative to the animation's final frame\n"
					"when a one-shot overlay ends on its own.\n\n"
					"Off (default): the fade FINISHES on the final frame, so it starts\n"
					"Blend Out seconds early and the animation keeps playing as it fades.\n\n"
					"On: the fade STARTS on the final frame and runs past it, holding the\n"
					"last pose while it ramps down.");
			}

			bool useFixedFrame = tf.sampleFrame >= 0.0f;
			if (ImGui::Checkbox("Sample Fixed Frame (?)##trackFilter", &useFixedFrame)) {
				tf.sampleFrame = useFixedFrame ? 0.0f : -1.0f;
				a_subMod->SetDirty(true);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Take the filtered bones' pose from ONE fixed frame of the replacement\n"
					"animation instead of following the clip's playback time — a held\n"
					"static pose (e.g. a slide locked back from frame 24 of the animation).\n"
					"When off, the replacement is sampled at the clip's current time.");
			}
			if (useFixedFrame) {
				ImGui::SameLine();
				int frame = static_cast<int>(tf.sampleFrame);
				ImGui::SetNextItemWidth(100);
				if (ImGui::InputInt("Frame##trackFilterFrame", &frame)) {
					tf.sampleFrame = static_cast<float>(std::max(0, frame));
					a_subMod->SetDirty(true);
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(
						"Frame number in the replacement animation to sample (30 frames per\n"
						"second; clamped to the animation's length).");
				}
			} else {
				if (ImGui::Checkbox("Anchor In Model Space (?)##trackFilter", &tf.modelSpaceAnchor)) {
					a_subMod->SetDirty(true);
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(
						"Re-express the filtered chain's root bones so the chain lands exactly\n"
						"where the replacement animation puts it relative to the character root.\n"
						"Without this, the replacement's bone rotations play under the BASE\n"
						"animation's (different) torso pose and the motion looks off.\n"
						"Override mode only. Recommended ON for partial-body action overlays.");
				}
			}

			if (ImGui::Checkbox("Include Children##trackFilter", &tf.includeChildren)) {
				tf.version.fetch_add(1, std::memory_order_relaxed);
				a_subMod->SetDirty(true);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"When enabled, all child bones in the hierarchy below the specified\n"
					"bones are also included. e.g. 'LArm_UpperArm' will include the\n"
					"entire left arm chain down to the hand.");
			}

			ImGui::Spacing();
			ImGui::Separator();

			// Take a snapshot for display to avoid holding lock during ImGui rendering
			std::vector<std::string> boneSnapshot;
			{
				std::lock_guard lock(tf.boneMutex);
				boneSnapshot = tf.boneNames;
			}
			ImGui::TextColored(UICommon::Colors::AccentBlue, "Filtered Bones (%zu)", boneSnapshot.size());
			ImGui::Spacing();
			int removeIdx = -1;
			for (int i = 0; i < static_cast<int>(boneSnapshot.size()); ++i) {
				ImGui::PushID(i);
				ImGui::BulletText("%s", boneSnapshot[i].c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton("X##removeBone")) {
					removeIdx = i;
				}
				ImGui::SameLine();
				DrawBoneDebugButtons(boneSnapshot[i]);
				ImGui::PopID();
			}
			if (removeIdx >= 0) {
				std::lock_guard lock(tf.boneMutex);
				if (removeIdx < static_cast<int>(tf.boneNames.size())) {
					tf.boneNames.erase(tf.boneNames.begin() + removeIdx);
				}
				tf.version.fetch_add(1, std::memory_order_relaxed);
				a_subMod->SetDirty(true);
			}

			ImGui::Spacing();

			if (ImGui::Button("Add Bone...")) {
				ImGui::OpenPopup("AddBonePopup");
			}
			ImGui::SameLine();

			static char customBoneName[128]{};
			ImGui::SetNextItemWidth(160);
			ImGui::InputTextWithHint("##customBone", "Custom bone name", customBoneName, sizeof(customBoneName));
			ImGui::SameLine();
			bool canAddCustom = customBoneName[0] != '\0';
			if (!canAddCustom) ImGui::BeginDisabled();
			if (ImGui::Button("Add Custom")) {
				std::lock_guard lock(tf.boneMutex);
				bool alreadyExists = false;
				for (auto& name : tf.boneNames) {
					if (name == customBoneName) { alreadyExists = true; break; }
				}
				if (!alreadyExists) {
					tf.boneNames.emplace_back(customBoneName);
					tf.version.fetch_add(1, std::memory_order_relaxed);
					a_subMod->SetDirty(true);
				}
				customBoneName[0] = '\0';
			}
			if (!canAddCustom) ImGui::EndDisabled();

			if (ImGui::BeginPopup("AddBonePopup")) {
				static char boneFilter[64]{};
				ImGui::InputTextWithHint("##boneSearch", "Search or type a bone name...", boneFilter, sizeof(boneFilter));
				ImGui::Separator();

				// The search text doubles as manual entry: offer to add it
				// verbatim unless it exactly names a known bone (that entry is
				// already listed below).
				if (boneFilter[0] != '\0') {
					bool exactKnown = false;
					for (const char* bone : kKnownBones) {
						if (_stricmp(bone, boneFilter) == 0) { exactKnown = true; break; }
					}
					bool alreadyAdded = false;
					for (const auto& name : boneSnapshot) {
						if (name == boneFilter) { alreadyAdded = true; break; }
					}
					if (!exactKnown && !alreadyAdded) {
						std::string typedLabel = std::string("Add typed name: \"") + boneFilter + "\"";
						if (ImGui::MenuItem(typedLabel.c_str())) {
							std::lock_guard lock(tf.boneMutex);
							tf.boneNames.emplace_back(boneFilter);
							tf.version.fetch_add(1, std::memory_order_relaxed);
							a_subMod->SetDirty(true);
							boneFilter[0] = '\0';
						}
					}
				}

				for (const char* bone : kKnownBones) {
					if (boneFilter[0] != '\0' && !UICommon::FuzzyMatch(boneFilter, bone)) continue;

					bool alreadyAdded = false;
					for (const auto& name : boneSnapshot) {
						if (name == bone) { alreadyAdded = true; break; }
					}

					if (alreadyAdded) {
						ImGui::TextDisabled("  %s (already added)", bone);
					} else if (ImGui::MenuItem(bone)) {
						std::lock_guard lock(tf.boneMutex);
						tf.boneNames.emplace_back(bone);
						tf.version.fetch_add(1, std::memory_order_relaxed);
						a_subMod->SetDirty(true);
					}
				}
				ImGui::EndPopup();
			}

			if (boneSnapshot.empty()) {
				ImGui::Spacing();
				ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.1f, 1.0f),
					"No bones selected — track filter will have no effect.");
			}

			// ---- Exclude Bones ----
			ImGui::Spacing();
			ImGui::Separator();

			std::vector<std::string> excludeSnapshot;
			{
				std::lock_guard lock(tf.boneMutex);
				excludeSnapshot = tf.excludeBoneNames;
			}

			ImGui::TextColored(UICommon::Colors::AccentBlue, "Excluded Bones (%zu)", excludeSnapshot.size());
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Bones listed here are REMOVED from the filtered set above.\n"
					"Use this to include a parent + children but exclude specific\n"
					"sub-branches (e.g. include RArm_UpperArm with children,\n"
					"exclude RArm_Hand to skip the hand).");
			}
			ImGui::Spacing();

			if (ImGui::Checkbox("Exclude Children##trackFilterExcl", &tf.excludeChildren)) {
				tf.version.fetch_add(1, std::memory_order_relaxed);
				a_subMod->SetDirty(true);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("When enabled, all child bones below each excluded bone are also excluded.");
			}

			int exclRemoveIdx = -1;
			for (int i = 0; i < static_cast<int>(excludeSnapshot.size()); ++i) {
				ImGui::PushID(1000 + i);
				ImGui::BulletText("%s", excludeSnapshot[i].c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton("X##removeExclBone")) {
					exclRemoveIdx = i;
				}
				ImGui::SameLine();
				DrawBoneDebugButtons(excludeSnapshot[i]);
				ImGui::PopID();
			}
			if (exclRemoveIdx >= 0) {
				std::lock_guard lock(tf.boneMutex);
				if (exclRemoveIdx < static_cast<int>(tf.excludeBoneNames.size())) {
					tf.excludeBoneNames.erase(tf.excludeBoneNames.begin() + exclRemoveIdx);
				}
				tf.version.fetch_add(1, std::memory_order_relaxed);
				a_subMod->SetDirty(true);
			}

			ImGui::Spacing();

			if (ImGui::Button("Add Exclude...")) {
				ImGui::OpenPopup("AddExclBonePopup");
			}
			ImGui::SameLine();

			static char customExclBoneName[128]{};
			ImGui::SetNextItemWidth(160);
			ImGui::InputTextWithHint("##customExclBone", "Custom bone name", customExclBoneName, sizeof(customExclBoneName));
			ImGui::SameLine();
			bool canAddExclCustom = customExclBoneName[0] != '\0';
			if (!canAddExclCustom) ImGui::BeginDisabled();
			if (ImGui::Button("Add Custom##excl")) {
				std::lock_guard lock(tf.boneMutex);
				bool alreadyExists = false;
				for (auto& name : tf.excludeBoneNames) {
					if (name == customExclBoneName) { alreadyExists = true; break; }
				}
				if (!alreadyExists) {
					tf.excludeBoneNames.emplace_back(customExclBoneName);
					tf.version.fetch_add(1, std::memory_order_relaxed);
					a_subMod->SetDirty(true);
				}
				customExclBoneName[0] = '\0';
			}
			if (!canAddExclCustom) ImGui::EndDisabled();

			if (ImGui::BeginPopup("AddExclBonePopup")) {
				static char exclBoneFilter[64]{};
				ImGui::InputTextWithHint("##exclBoneSearch", "Search or type a bone name...", exclBoneFilter, sizeof(exclBoneFilter));
				ImGui::Separator();

				// Same search-as-manual-entry behavior as the include popup.
				if (exclBoneFilter[0] != '\0') {
					bool exactKnown = false;
					for (const char* bone : kKnownBones) {
						if (_stricmp(bone, exclBoneFilter) == 0) { exactKnown = true; break; }
					}
					bool alreadyAdded = false;
					for (const auto& name : excludeSnapshot) {
						if (name == exclBoneFilter) { alreadyAdded = true; break; }
					}
					if (!exactKnown && !alreadyAdded) {
						std::string typedLabel = std::string("Add typed name: \"") + exclBoneFilter + "\"";
						if (ImGui::MenuItem(typedLabel.c_str())) {
							std::lock_guard lock(tf.boneMutex);
							tf.excludeBoneNames.emplace_back(exclBoneFilter);
							tf.version.fetch_add(1, std::memory_order_relaxed);
							a_subMod->SetDirty(true);
							exclBoneFilter[0] = '\0';
						}
					}
				}

				for (const char* bone : kKnownBones) {
					if (exclBoneFilter[0] != '\0' && !UICommon::FuzzyMatch(exclBoneFilter, bone)) continue;

					bool alreadyAdded = false;
					for (const auto& name : excludeSnapshot) {
						if (name == bone) { alreadyAdded = true; break; }
					}

					if (alreadyAdded) {
						ImGui::TextDisabled("  %s (already added)", bone);
					} else if (ImGui::MenuItem(bone)) {
						std::lock_guard lock(tf.boneMutex);
						tf.excludeBoneNames.emplace_back(bone);
						tf.version.fetch_add(1, std::memory_order_relaxed);
						a_subMod->SetDirty(true);
					}
				}
				ImGui::EndPopup();
			}

			// ---- Freeze Bones ----
			ImGui::Spacing();
			ImGui::Separator();

			std::vector<std::string> freezeSnapshot;
			{
				std::lock_guard lock(tf.boneMutex);
				freezeSnapshot = tf.freezeBoneNames;
			}

			ImGui::TextColored(UICommon::Colors::AccentBlue, "Frozen Bones (%zu)", freezeSnapshot.size());
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Bones listed here (with their children) are driven by NEITHER\n"
					"the replacement NOR the underlying animation while this filter\n"
					"is active: each holds the pose it had when the overlay started,\n"
					"and releases through the normal blend-out.\n\n"
					"Excluding a bone leaves the underlying animation driving it -\n"
					"a weapon gripped by replacement-driven hands would still play\n"
					"each weapon's own swing. Freezing pins the grip instead.");
			}
			ImGui::Spacing();

			int frzRemoveIdx = -1;
			for (int i = 0; i < static_cast<int>(freezeSnapshot.size()); ++i) {
				ImGui::PushID(2000 + i);
				ImGui::BulletText("%s", freezeSnapshot[i].c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton("X##removeFrzBone")) {
					frzRemoveIdx = i;
				}
				ImGui::SameLine();
				DrawBoneDebugButtons(freezeSnapshot[i]);
				ImGui::PopID();
			}
			if (frzRemoveIdx >= 0) {
				std::lock_guard lock(tf.boneMutex);
				if (frzRemoveIdx < static_cast<int>(tf.freezeBoneNames.size())) {
					tf.freezeBoneNames.erase(tf.freezeBoneNames.begin() + frzRemoveIdx);
				}
				tf.version.fetch_add(1, std::memory_order_relaxed);
				a_subMod->SetDirty(true);
			}

			ImGui::Spacing();

			if (ImGui::Button("Add Freeze...")) {
				ImGui::OpenPopup("AddFrzBonePopup");
			}
			ImGui::SameLine();

			static char customFrzBoneName[128]{};
			ImGui::SetNextItemWidth(160);
			ImGui::InputTextWithHint("##customFrzBone", "Custom bone name", customFrzBoneName, sizeof(customFrzBoneName));
			ImGui::SameLine();
			bool canAddFrzCustom = customFrzBoneName[0] != '\0';
			if (!canAddFrzCustom) ImGui::BeginDisabled();
			if (ImGui::Button("Add Custom##frz")) {
				std::lock_guard lock(tf.boneMutex);
				bool alreadyExists = false;
				for (auto& name : tf.freezeBoneNames) {
					if (name == customFrzBoneName) { alreadyExists = true; break; }
				}
				if (!alreadyExists) {
					tf.freezeBoneNames.emplace_back(customFrzBoneName);
					tf.version.fetch_add(1, std::memory_order_relaxed);
					a_subMod->SetDirty(true);
				}
				customFrzBoneName[0] = '\0';
			}
			if (!canAddFrzCustom) ImGui::EndDisabled();

			if (ImGui::BeginPopup("AddFrzBonePopup")) {
				static char frzBoneFilter[64]{};
				ImGui::InputTextWithHint("##frzBoneSearch", "Search or type a bone name...", frzBoneFilter, sizeof(frzBoneFilter));
				ImGui::Separator();

				if (frzBoneFilter[0] != '\0') {
					bool exactKnown = false;
					for (const char* bone : kKnownBones) {
						if (_stricmp(bone, frzBoneFilter) == 0) { exactKnown = true; break; }
					}
					bool alreadyAdded = false;
					for (const auto& name : freezeSnapshot) {
						if (name == frzBoneFilter) { alreadyAdded = true; break; }
					}
					if (!exactKnown && !alreadyAdded) {
						std::string typedLabel = std::string("Add typed name: \"") + frzBoneFilter + "\"";
						if (ImGui::MenuItem(typedLabel.c_str())) {
							std::lock_guard lock(tf.boneMutex);
							tf.freezeBoneNames.emplace_back(frzBoneFilter);
							tf.version.fetch_add(1, std::memory_order_relaxed);
							a_subMod->SetDirty(true);
							frzBoneFilter[0] = '\0';
						}
					}
				}

				for (const char* bone : kKnownBones) {
					if (frzBoneFilter[0] != '\0' && !UICommon::FuzzyMatch(frzBoneFilter, bone)) continue;

					bool alreadyAdded = false;
					for (const auto& name : freezeSnapshot) {
						if (name == bone) { alreadyAdded = true; break; }
					}

					if (alreadyAdded) {
						ImGui::TextDisabled("  %s (already added)", bone);
					} else if (ImGui::MenuItem(bone)) {
						std::lock_guard lock(tf.boneMutex);
						tf.freezeBoneNames.emplace_back(bone);
						tf.version.fetch_add(1, std::memory_order_relaxed);
						a_subMod->SetDirty(true);
					}
				}
				ImGui::EndPopup();
			}
		}
	} else {
		ImGui::Text("Enabled: %s", tf.enabled ? "Yes" : "No");
		if (tf.enabled) {
			ImGui::Text("Mode: %s  |  Weight: %.2f  |  Children: %s",
				tf.mode == SubMod::TrackFilter::Mode::Override ? "Override" : "Additive",
				tf.weight,
				tf.includeChildren ? "Yes" : "No");
			std::vector<std::string> displayBones;
			std::vector<std::string> displayExclude;
			{
				std::lock_guard lock(tf.boneMutex);
				displayBones = tf.boneNames;
				displayExclude = tf.excludeBoneNames;
			}
			if (!displayBones.empty()) {
				ImGui::Text("Include Bones:");
				for (const auto& name : displayBones) {
					ImGui::BulletText("%s", name.c_str());
				}
			} else {
				UICommon::TextUnformattedDisabled("No bones configured");
			}
			if (!displayExclude.empty()) {
				ImGui::Text("Exclude Bones (children: %s):", tf.excludeChildren ? "Yes" : "No");
				for (const auto& name : displayExclude) {
					ImGui::BulletText("%s", name.c_str());
				}
			}
		}
	}

	ImGui::Unindent(8.f);
}

void UIMain::DrawReplacementAnimList(SubMod* a_subMod)
{
	if (!a_subMod) return;

	auto& anims = a_subMod->GetReplacementAnimations();
	if (anims.empty()) {
		UICommon::TextUnformattedDisabled("No replacement animations");
		return;
	}

	bool editable = (currentMode != UICommon::EditorMode::kInspect);

	if (ImGui::BeginTable("AnimTable", 3,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable)) {
		ImGui::TableSetupColumn("Original", ImGuiTableColumnFlags_None, 0.4f);
		ImGui::TableSetupColumn("Replacement", ImGuiTableColumnFlags_None, 0.5f);
		ImGui::TableSetupColumn("Variants", ImGuiTableColumnFlags_WidthFixed, 60.f);
		ImGui::TableHeadersRow();

		for (auto* anim : anims) {
			if (!anim) continue;
			ImGui::TableNextRow();

			std::string origShort = UICommon::ShortenAnimPath(anim->GetOriginalPath());
			std::string replShort = UICommon::ShortenAnimPath(anim->GetReplacementPath());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(origShort.c_str());
			if (ImGui::IsItemHovered() && origShort != anim->GetOriginalPath()) {
				ImGui::SetTooltip("%s", anim->GetOriginalPath().c_str());
			}

			ImGui::TableNextColumn();
			ImGui::TextColored(UICommon::Colors::AccentBlue, "%s", replShort.c_str());
			if (ImGui::IsItemHovered() && replShort != anim->GetReplacementPath()) {
				ImGui::SetTooltip("%s", anim->GetReplacementPath().c_str());
			}

			ImGui::TableNextColumn();
			if (anim->HasVariants()) {
				ImGui::Text("%zu", anim->GetVariants()->GetCount());
			} else {
				UICommon::TextUnformattedDisabled("-");
			}
		}

		ImGui::EndTable();
	}

	// Variant controls for animations that have variants
	bool hasAnyVariants = false;
	for (auto* anim : anims) {
		if (anim && anim->HasVariants()) { hasAnyVariants = true; break; }
	}

	if (hasAnyVariants) {
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::TextUnformatted("Variant Animation Settings");
		ImGui::Spacing();

		// Enable/disable toggle
		if (editable) {
			if (ImGui::Checkbox("Enable Variant Selection", &a_subMod->variantsEnabled)) {
				a_subMod->SetDirty(true);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("When enabled, a variant is randomly or sequentially selected from the group.\nWhen disabled, only the base animation plays.");
			}
		} else {
			ImGui::Text("Variant Selection: %s", a_subMod->variantsEnabled ? "Enabled" : "Disabled");
		}

		if (!a_subMod->variantsEnabled) {
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Variant selection is disabled. Only the base animation will play.");
			ImGui::Spacing();
		}

		if (a_subMod->variantsEnabled) {
		// Mode toggle
		int modeInt = static_cast<int>(a_subMod->variantMode);
		if (editable) {
			if (ImGui::RadioButton("Random", &modeInt, 0)) {
				a_subMod->variantMode = VariantMode::kRandom;
				for (auto* anim : anims) {
					if (anim && anim->HasVariants())
						anim->GetVariants()->SetMode(VariantMode::kRandom);
				}
				a_subMod->SetDirty(true);
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("Sequential", &modeInt, 1)) {
				a_subMod->variantMode = VariantMode::kSequential;
				for (auto* anim : anims) {
					if (anim && anim->HasVariants())
						anim->GetVariants()->SetMode(VariantMode::kSequential);
				}
				a_subMod->SetDirty(true);
			}
		} else {
			ImGui::Text("Mode: %s", modeInt == 0 ? "Random" : "Sequential");
		}

		// Reroll policy dropdown
		ImGui::Spacing();
		if (editable) {
			static const char* rerollLabels[] = { "On Each Play", "While Conditions Active" };
			int rerollInt = static_cast<int>(a_subMod->variantRerollPolicy);
			ImGui::TextUnformatted("Variant Selection Timing:");
			if (ImGui::Combo("##reroll_policy", &rerollInt, rerollLabels, IM_ARRAYSIZE(rerollLabels))) {
				a_subMod->variantRerollPolicy = static_cast<VariantRerollPolicy>(rerollInt);
				a_subMod->SetDirty(true);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"On Each Play: A new random variant is selected every time the animation plays.\n"
					"While Conditions Active: A variant is selected once and kept until the conditions become false.");
			}

			bool shareResults = a_subMod->GetShareRandomResults();
			if (ImGui::Checkbox("Share results across actors (?)", &shareResults)) {
				a_subMod->SetShareRandomResults(shareResults);
				a_subMod->SetDirty(true);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("When checked, all actors play the same randomly-selected\n"
					"variant instead of rolling independently.");
			}
		} else {
			static const char* rerollLabels[] = { "On Each Play", "While Conditions Active" };
			int rerollInt = static_cast<int>(a_subMod->variantRerollPolicy);
			ImGui::Text("Variant Selection Timing: %s", rerollLabels[rerollInt]);
		}

		// Per-variant weight sliders (only in random mode)
		if (a_subMod->variantMode == VariantMode::kRandom) {
			ImGui::Spacing();
			ImGui::TextUnformatted("Variant Weights:");

			for (auto* anim : anims) {
				if (!anim || !anim->HasVariants()) continue;

				// Several variant GROUPS can coexist in one submod (one per
				// replaced original), and their variant files share names
				// ("wpnmelee.hkx", "wpnmelee_1.hkx") and indices — so the
				// label alone collides across groups ("3 visible items with
				// conflicting ID"). Scope each group's IDs by its animation.
				ImGui::PushID(anim);

				auto* variants = anim->GetVariants();
				auto& entries = variants->GetEntriesMutable();

				for (size_t i = 0; i < entries.size(); ++i) {
					auto& ve = entries[i];
					std::string label = ve.filename + "##weight_" + std::to_string(i);

					if (editable) {
						ImGui::PushItemWidth(120.f);
						if (ImGui::SliderFloat(label.c_str(), &ve.weight, 0.01f, 10.0f, "%.2f")) {
							a_subMod->variantWeights[ve.filename] = ve.weight;
							a_subMod->SetDirty(true);
						}
						ImGui::PopItemWidth();
					} else {
						ImGui::Text("%s: %.2f", ve.filename.c_str(), ve.weight);
					}
				}

				ImGui::PopID();
			}
		}
		} // end variantsEnabled
	}
}

void UIMain::DrawBottomBar()
{
	ImGui::Separator();

	auto* oar = OpenAnimationReplacer::GetSingleton();
	const char* modeStr = currentMode == UICommon::EditorMode::kInspect ? "Inspect" :
	                      currentMode == UICommon::EditorMode::kUser ? "User" : "Author";

	ImGui::Text("Mode: %s | Mods: %zu | Replacements: %zu",
		modeStr, oar->GetReplacerMods().size(), oar->GetTotalReplacementCount());

	// Measure the buttons using the active font scale instead of reserving a
	// fixed pixel width. The old 240-pixel estimate could place the Settings
	// button beyond the right edge when larger text was selected.
	constexpr const char* kAnimLogLabel = "Anim Log";
	constexpr const char* kEventLogLabel = "Event Log";
	constexpr const char* kSettingsLabel = "Settings";
	const auto& style = ImGui::GetStyle();
	const auto smallButtonWidth = [&](const char* a_label) {
		return ImGui::CalcTextSize(a_label).x + style.FramePadding.x * 2.0f;
	};
	const float buttonGroupWidth =
		smallButtonWidth(kAnimLogLabel) + smallButtonWidth(kEventLogLabel) +
		smallButtonWidth(kSettingsLabel) + style.ItemSpacing.x * 2.0f;
	const ImVec2 nextLineCursor = ImGui::GetCursorScreenPos();
	const float contentRightScreenX = nextLineCursor.x + ImGui::GetContentRegionAvail().x;
	const float buttonGroupScreenX = contentRightScreenX - buttonGroupWidth;

	if (ImGui::GetItemRectMax().x + style.ItemSpacing.x <= buttonGroupScreenX) {
		ImGui::SameLine();
	}
	// If the status text and buttons cannot share one row, the cursor remains on
	// the next row. In either case, right-align the group without exceeding the
	// available content region.
	ImVec2 buttonCursor = ImGui::GetCursorScreenPos();
	buttonCursor.x = std::max(buttonCursor.x, buttonGroupScreenX);
	ImGui::SetCursorScreenPos(buttonCursor);

	auto* uiMgr = UIManager::GetSingleton();
	if (ImGui::SmallButton(kAnimLogLabel)) uiMgr->ToggleWindow(WindowID::kAnimationLog);
	ImGui::SameLine();
	if (ImGui::SmallButton(kEventLogLabel)) uiMgr->ToggleWindow(WindowID::kAnimationEventLog);
	ImGui::SameLine();
	if (ImGui::SmallButton(kSettingsLabel)) showSettings = !showSettings;
}

void UIMain::ApplyCapturedToggleKey(std::uint32_t a_dik)
{
	auto* settings = Settings::GetSingleton();
	settings->iToggleKey = a_dik;
	settings->Save();
	capturingToggleKey = false;
	logger::info("[OAR-UI] Toggle key rebound to DIK 0x{:X} ({})",
		a_dik, UICommon::DIKCodeToName(a_dik));
}

void UIMain::DrawSettingsPanel()
{
	auto* settings = Settings::GetSingleton();
	const float textScale = static_cast<float>(settings->iTextSizePercent) / 100.0f;
	ImGui::SetNextWindowSize(ImVec2(350.0f * textScale, 500.0f * textScale), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("OAR Settings", &showSettings)) {
		ImGui::End();
		return;
	}

	bool dirty = false;

	ImGui::TextColored(UICommon::Colors::AccentBlue, "General");
	{
		// Master switch. Disabling queues a restore on the game thread that puts
		// every replaced clip back on its vanilla animation; enabling lets
		// replacement resume on the next clip update. See OnGlobalEnabledChanged.
		const bool enabledChanged = ImGui::Checkbox("Enabled", &settings->bEnabled);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"Master switch for animation replacement. Unticking stops all\n"
				"replacement and restores the vanilla animations immediately\n"
				"(no reload needed). The Animation Log keeps working either way.");
		}
		if (enabledChanged) {
			dirty = true;
			OnGlobalEnabledChanged(settings->bEnabled);
		}
	}
	dirty |= ImGui::Checkbox("Direct Path Matching", &settings->bDirectPathMatching);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Match replacements against the clip's resolved on-disk animation path\n"
			"(e.g. Weapons\\SCAR\\WPNReload.hkx) instead of by leaf file name.\n"
			"Leaf-name matching is only used as a fallback for clips whose real\n"
			"path cannot be resolved. Disable to restore the legacy leaf-matching\n"
			"behavior everywhere.");
	}
	{
		// The engine's own auto-reloads are always suppressed (they are
		// attack-initiated and get cut short at reloadComplete). This picks
		// what OAR does instead; its reloads use the reload-key path and
		// play the full animation.
		static const char* kAutoReloadModes[] = {
			"Auto-Reload On Last Round",
			"Auto-Reload On Fire Press When Empty",
			"Suppress Auto-Reload",
		};
		ImGui::SetNextItemWidth(280.0f);
		dirty |= ImGui::Combo("Auto-Reload", &settings->iAutoReloadMode, kAutoReloadModes, 3);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"The game's own auto-reloads are attack-initiated, so the engine cuts\n"
				"them short the moment the magazine refills (at the reloadComplete\n"
				"annotation). OAR always suppresses them and instead triggers reloads\n"
				"through the reload-key path, which plays the full animation.\n"
				"\n"
				"On Last Round: reload as soon as the magazine hits 0 by firing.\n"
				"On Fire Press When Empty: reload when you press fire on an empty\n"
				"magazine (vanilla-style trigger); dry fire still plays first.\n"
				"Suppress Auto-Reload: no automatic reloads; reload key only.\n"
				"\n"
				"Compatible with ManualReloadF4SE (whichever loads first applies the\n"
				"engine patch, the other detects it and leaves it alone).");
		}

		dirty |= ImGui::Checkbox("Play Dry-Fire Sound", &settings->bPlayDryFireSound);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"Plays a click when you press fire on an empty magazine (or with no\n"
				"ammo of that type at all). With the engine's fire-empty auto-reload\n"
				"suppressed, the press would otherwise be silent. Uses the weapon's\n"
				"Attack Fail sound, or the vanilla 10mm dry-fire click if it has none.\n"
				"Turn this off if your dry-fire replacement animations already play\n"
				"their own click sounds.");
		}
	}
	dirty |= ImGui::Checkbox("Verbose Logging", &settings->bVerboseLogging);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextColored(UICommon::Colors::AccentBlue, "UI");

	// Toggle hotkey — click the button, then press the new key. Escape cancels.
	{
		std::string currentLabel;
		if (settings->bRequireShift) currentLabel += "Shift+";
		currentLabel += UICommon::DIKCodeToName(settings->iToggleKey);

		ImGui::TextUnformatted("Activation Key");
		ImGui::SameLine();
		UICommon::HelpMarker(
			"Hotkey that opens and closes the OAR editor overlay.\n"
			"Click Change, then press the new key. Escape cancels.\n"
			"Default is F2 (no Shift required).");

		if (capturingToggleKey) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.35f, 0.1f, 1.0f));
			if (ImGui::Button("Press a key... (Esc to cancel)##toggleKey")) {
				capturingToggleKey = false;
			}
			ImGui::PopStyleColor();
		} else {
			std::string btn = "Change (" + currentLabel + ")##toggleKey";
			if (ImGui::Button(btn.c_str())) {
				capturingToggleKey = true;
			}
		}

		dirty |= ImGui::Checkbox("Require Shift", &settings->bRequireShift);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("When enabled, Shift must be held together with the activation key.");
		}

		ImGui::TextDisabled("Current: %s", currentLabel.c_str());
	}

	{
		bool wasPausing = settings->bPauseOnMenuOpen;
		dirty |= ImGui::Checkbox("Pause Game When UI Open", &settings->bPauseOnMenuOpen);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Pauses the game world while the OAR editor window is open.");
		}
		if (wasPausing != settings->bPauseOnMenuOpen) {
			if (auto* ui = RE::UI::GetSingleton()) {
				if (settings->bPauseOnMenuOpen) {
					ui->menuMode += 1;
				} else if (ui->menuMode > 0) {
					ui->menuMode -= 1;
				}
			}
		}
	}

	// ImGui 1.92 moved io.FontGlobalScale to style.FontScaleMain. Keep the
	// user-facing value as an integer percentage for stable INI persistence.
	const int previousTextSize = settings->iTextSizePercent;
	if (ImGui::SliderInt("Text Size", &settings->iTextSizePercent, 80, 200, "%d%%")) {
		const float previousScale = static_cast<float>(previousTextSize) / 100.0f;
		const float newScale = static_cast<float>(settings->iTextSizePercent) / 100.0f;
		ImGui::GetStyle().FontScaleMain = newScale;

		// The settings panel is not a UIWindow, so compensate its dimensions here.
		// Other OAR windows perform the same ratio-based adjustment in TryDraw().
		const ImVec2 currentSize = ImGui::GetWindowSize();
		ImVec2 resized(currentSize.x * newScale / previousScale,
			currentSize.y * newScale / previousScale);
		if (const auto* viewport = ImGui::GetMainViewport()) {
			resized.x = std::min(resized.x, std::max(1.0f, viewport->WorkSize.x - 16.0f));
			resized.y = std::min(resized.y, std::max(1.0f, viewport->WorkSize.y - 16.0f));
		}
		ImGui::SetWindowSize(resized, ImGuiCond_Always);
		dirty = true;
	}

	// firstColumnPercent is stored as a 0.0-to-1.0 ratio, while the user-facing
	// control should show whole percentages rather than rounding to 0 or 1.
	int leftPanelPercent = static_cast<int>(std::lround(firstColumnPercent * 100.0f));
	if (ImGui::SliderInt("Left Panel %", &leftPanelPercent, 20, 80, "%d%%")) {
		firstColumnPercent = static_cast<float>(leftPanelPercent) / 100.0f;
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextColored(UICommon::Colors::AccentBlue, "Animation Log");
	dirty |= ImGui::Checkbox("Log Activate", &settings->bLogActivate);
	dirty |= ImGui::Checkbox("Log Replace", &settings->bLogReplace);
	dirty |= ImGui::Checkbox("Log Loop", &settings->bLogLoop);
	dirty |= ImGui::Checkbox("Log Echo", &settings->bLogEcho);
	dirty |= ImGui::SliderInt("Max Entries", &settings->iMaxLogEntries, 10, 1000);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextColored(UICommon::Colors::AccentBlue, "Loading");
	dirty |= ImGui::Checkbox("Show Loading Progress Bar", &settings->bEnableAnimationQueueProgressBar);
	dirty |= ImGui::SliderFloat("Linger Time (s)", &settings->fAnimationQueueLingerTime, 1.0f, 15.0f, "%.1f");

	if (dirty) {
		settings->Save();
	}

	ImGui::End();
}
