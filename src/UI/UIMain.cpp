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

	if (showSettings) DrawSettingsPanel();
}

void UIMain::DrawFilterBar()
{
	float availWidth = ImGui::GetContentRegionAvail().x;

	ImGui::PushStyleColor(ImGuiCol_FrameBg, UICommon::Colors::FilterBg);
	ImGui::SetNextItemWidth(availWidth * 0.4f);
	ImGui::InputTextWithHint("##filter", "Filter mods...", filterText, sizeof(filterText));
	ImGui::PopStyleColor();

	ImGui::SameLine(availWidth * 0.5f);

	int modeInt = static_cast<int>(currentMode);
	if (ImGui::RadioButton("Inspect", modeInt == 0)) currentMode = UICommon::EditorMode::kInspect;
	ImGui::SameLine();
	if (ImGui::RadioButton("User", modeInt == 1)) currentMode = UICommon::EditorMode::kUser;
	ImGui::SameLine();
	if (ImGui::RadioButton("Author", modeInt == 2)) currentMode = UICommon::EditorMode::kAuthor;

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
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Don't re-roll random variant when animation loops");

		bool shareRandom = a_subMod->GetShareRandomResults();
		if (ImGui::Checkbox("Share random results", &shareRandom)) {
			a_subMod->SetShareRandomResults(shareRandom);
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("All actors share the same random variant selection");

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
		if (ImGui::Checkbox("Replace on Loop", &replOnLoop)) {
			a_subMod->SetReplaceOnLoop(replOnLoop);
			a_subMod->SetDirty(true);
		}
		ImGui::SameLine();
		bool replOnEcho = a_subMod->GetReplaceOnEcho();
		if (ImGui::Checkbox("Replace on Echo", &replOnEcho)) {
			a_subMod->SetReplaceOnEcho(replOnEcho);
			a_subMod->SetDirty(true);
		}

		// Custom blend times
		float blendInterrupt = a_subMod->GetCustomBlendTimeOnInterrupt();
		ImGui::SetNextItemWidth(80);
		if (ImGui::InputFloat("Blend time (interrupt) (?)", &blendInterrupt, 0, 0, "%.2f")) {
			a_subMod->customBlendTimeOnInterrupt = blendInterrupt;
			a_subMod->SetDirty(true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Custom blend duration in seconds when interrupting. Default: 0.20s. Set negative to use default.");
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
	for (const auto& cond : a_condSet->GetConditions()) {
		ImVec2 beforePos = ImGui::GetCursorScreenPos();
		DrawCondition(cond.get(), a_condSet, index, a_subMod, a_depth);
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
			}
			ImGui::EndPopup();
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
				}
			}

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

void UIMain::DrawReplacementAnimList(SubMod* a_subMod)
{
	if (!a_subMod) return;

	auto& anims = a_subMod->GetReplacementAnimations();
	if (anims.empty()) {
		UICommon::TextUnformattedDisabled("No replacement animations");
		return;
	}

	if (ImGui::BeginTable("AnimTable", 3,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable)) {
		ImGui::TableSetupColumn("Original", ImGuiTableColumnFlags_None, 0.4f);
		ImGui::TableSetupColumn("Replacement", ImGuiTableColumnFlags_None, 0.5f);
		ImGui::TableSetupColumn("Idx", ImGuiTableColumnFlags_WidthFixed, 40.f);
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
			ImGui::Text("%d", anim->GetBindingIndex());
		}

		ImGui::EndTable();
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
	dirty |= ImGui::Checkbox("Verbose Logging", &settings->bVerboseLogging);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextColored(UICommon::Colors::AccentBlue, "UI");

	dirty |= ImGui::Checkbox("Pause Game When UI Open", &settings->bPauseOnMenuOpen);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Pauses the game world while the OAR editor window is open.");
	}

	float scale = ImGui::GetIO().FontGlobalScale;
	if (ImGui::SliderFloat("UI Scale", &scale, 0.8f, 2.0f, "%.1f")) {
		ImGui::GetIO().FontGlobalScale = scale;
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
