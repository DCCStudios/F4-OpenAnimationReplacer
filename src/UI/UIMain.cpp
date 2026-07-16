#include "UI/UIMain.h"
#include "UI/UIManager.h"
#include "OpenAnimationReplacer.h"
#include "ReplacerMods.h"
#include "ReplacementAnimation.h"
#include "BaseConditions.h"
#include "Conditions.h"
#include "AnimationLog.h"
#include "Jobs.h"
#include "Parsing.h"
#include "Settings.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <mutex>

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
				JobQueue::GetSingleton()->Enqueue(std::make_unique<ReloadConfigJob>());
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
	auto* oar = OpenAnimationReplacer::GetSingleton();
	ReadLocker modsLock(oar->GetModsMutex());

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

		ImGuiTreeNodeFlags modFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
		bool modOpen = ImGui::TreeNodeEx(mod->GetName().c_str(), modFlags);

		if (ImGui::IsItemHovered() && !mod->GetDescription().empty()) {
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(mod->GetDescription().c_str());
			if (!mod->GetAuthor().empty()) {
				ImGui::TextColored(UICommon::Colors::Disabled, "Author: %s", mod->GetAuthor().c_str());
			}
			ImGui::EndTooltip();
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

	std::string label = std::format("[{}] {}", a_subMod->GetPriority(), a_subMod->GetName());
	if (isDirty) label += " *";
	if (a_subMod->hasUserConfig) label += " (User)";

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

	ImGui::TreeNodeEx(label.c_str(), flags);
	if (ImGui::IsItemClicked()) {
		selectedSubMod = a_subMod;
	}

	// Right-click context menu
	if (ImGui::BeginPopupContextItem("SubModContext")) {
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
	if (!a_subMod->GetDescription().empty()) {
		ImGui::TextWrapped("%s", a_subMod->GetDescription().c_str());
	}
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

		// Custom blend times
		float blendInterrupt = a_subMod->GetCustomBlendTimeOnInterrupt();
		ImGui::SetNextItemWidth(80);
		if (ImGui::InputFloat("Blend time (interrupt) (?)", &blendInterrupt, 0, 0, "%.2f")) {
			a_subMod->customBlendTimeOnInterrupt = blendInterrupt;
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Custom blend duration in seconds when interrupting. Default: 0.20s. Set negative to use default.");

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
			ImGui::SameLine();
			if (ImGui::Button("Add from List")) {
				events.push_back(s_commonEvents[selectedCommon]);
				a_subMod->SetDirty(true);
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
			if (a_subMod->GetCustomBlendTimeOnInterrupt() >= 0.f)
				json["customBlendTimeOnInterrupt"] = a_subMod->GetCustomBlendTimeOnInterrupt();
			if (a_subMod->GetDeactivationDelay() > 0.0f)
				json["deactivationDelay"] = a_subMod->GetDeactivationDelay();
			if (a_subMod->GetPlayOnceFullBody())
				json["playOnceFullBody"] = true;
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
				tfJson["includeChildren"] = tf.includeChildren;
				tfJson["bones"] = tf.boneNames;
				tfJson["excludeChildren"] = tf.excludeChildren;
				tfJson["excludeBones"] = tf.excludeBoneNames;
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
			JobQueue::GetSingleton()->Enqueue(std::make_unique<ReloadConfigJob>());
		}

		if (a_subMod->hasUserConfig) {
			ImGui::SameLine();
			if (UICommon::ButtonWithConfirmationModal(
				"Delete user config", "Confirm Delete",
				"Are you sure you want to delete the user config?"))
			{
				auto userPath = a_subMod->GetPath() / "user.json";
				try { std::filesystem::remove(userPath); } catch (...) {}
				JobQueue::GetSingleton()->Enqueue(std::make_unique<ReloadConfigJob>());
			}
		}
	}
}

void UIMain::DrawConditionSet(ConditionSet* a_condSet, SubMod* a_subMod, int a_depth)
{
	if (!a_condSet || a_condSet->IsEmpty()) {
		UICommon::TextUnformattedDisabled("No conditions (always matches)");
		return;
	}

	bool editable = currentMode != UICommon::EditorMode::kInspect;

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

	auto* orCond = dynamic_cast<ORCondition*>(a_condition);
	auto* andCond = dynamic_cast<ANDCondition*>(a_condition);
	bool hasChildren = orCond || andCond;

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
				if (hasChildren) {
					if (orCond) {
						ImGui::Indent();
						DrawConditionSet(&orCond->GetConditionSet(), a_subMod, a_depth + 1);
						ImGui::Unindent();
					} else if (andCond) {
						ImGui::Indent();
						DrawConditionSet(&andCond->GetConditionSet(), a_subMod, a_depth + 1);
						ImGui::Unindent();
					}
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

void UIMain::DrawTrackFilterSection(SubMod* a_subMod, bool a_editable)
{
	if (!a_subMod) return;

	auto& tf = a_subMod->trackFilter;

	// Havok skeleton bones — matches the actual animation skeleton definition
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

	ImGui::Indent(8.f);

	if (a_editable) {
		if (ImGui::Checkbox("Enabled##trackFilter", &tf.enabled)) {
			a_subMod->SetDirty(true);
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
				ImGui::SetTooltip("Time in seconds to ramp from full weight to 0 when conditions become false.\n0 = instant snap.");
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
				ImGui::InputTextWithHint("##boneSearch", "Search bones...", boneFilter, sizeof(boneFilter));
				ImGui::Separator();

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
				ImGui::InputTextWithHint("##exclBoneSearch", "Search bones...", exclBoneFilter, sizeof(exclBoneFilter));
				ImGui::Separator();

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

	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 240);

	auto* uiMgr = UIManager::GetSingleton();
	if (ImGui::SmallButton("Anim Log")) uiMgr->ToggleWindow(WindowID::kAnimationLog);
	ImGui::SameLine();
	if (ImGui::SmallButton("Event Log")) uiMgr->ToggleWindow(WindowID::kAnimationEventLog);
	ImGui::SameLine();
	if (ImGui::SmallButton("Settings")) showSettings = !showSettings;
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
	ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("OAR Settings", &showSettings)) {
		ImGui::End();
		return;
	}

	auto* settings = Settings::GetSingleton();
	bool dirty = false;

	ImGui::TextColored(UICommon::Colors::AccentBlue, "General");
	dirty |= ImGui::Checkbox("Enabled", &settings->bEnabled);
	dirty |= ImGui::Checkbox("Direct Path Matching", &settings->bDirectPathMatching);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Match replacements against the clip's resolved on-disk animation path\n"
			"(e.g. Weapons\\SCAR\\WPNReload.hkx) instead of by leaf file name.\n"
			"Leaf-name matching is only used as a fallback for clips whose real\n"
			"path cannot be resolved. Disable to restore the legacy leaf-matching\n"
			"behavior everywhere.");
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

	// imgui 1.92 moved io.FontGlobalScale to style.FontScaleMain
	float scale = ImGui::GetStyle().FontScaleMain;
	if (ImGui::SliderFloat("UI Scale", &scale, 0.8f, 2.0f, "%.1f")) {
		ImGui::GetStyle().FontScaleMain = scale;
	}

	ImGui::SliderFloat("Left Panel %", &firstColumnPercent, 0.2f, 0.8f, "%.0f%%");

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
