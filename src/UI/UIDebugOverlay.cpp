#include "UI/UIDebugOverlay.h"
#include "UI/UICommon.h"
#include "ActiveReplacementTracker.h"
#include "ReplacerMods.h"

#include <imgui.h>
#include <algorithm>

void UIDebugOverlay::DrawContents()
{
	auto* tracker = ActiveReplacementTracker::GetSingleton();
	auto snapshot = tracker->GetSnapshot();

	ImGui::Text("Active Replacements: %zu", snapshot.size());
	ImGui::Separator();

	if (snapshot.empty()) {
		ImGui::TextDisabled("No animations currently replaced.");
		return;
	}

	std::sort(snapshot.begin(), snapshot.end(), [](const auto& a, const auto& b) {
		if (a.actorFormID != b.actorFormID) return a.actorFormID < b.actorFormID;
		return a.clipSuffix < b.clipSuffix;
	});

	if (ImGui::BeginTable("##ActiveReplacements", 5,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
		ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {

		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Actor", ImGuiTableColumnFlags_WidthFixed, 140.f);
		ImGui::TableSetupColumn("Clip", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Replacement", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("SubMod", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthFixed, 50.f);
		ImGui::TableHeadersRow();

		for (auto& entry : snapshot) {
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			if (entry.actorFormID != 0) {
				ImGui::TextWrapped("%s [%08X]", entry.actorName.c_str(), entry.actorFormID);
			} else {
				ImGui::TextDisabled("(unknown)");
			}

			// Wrap long clip names / paths at the column edge so the full text is
			// always visible (rows grow vertically instead of clipping).
			ImGui::TableNextColumn();
			ImGui::TextWrapped("%s", entry.clipSuffix.c_str());
			if (!entry.fullPath.empty()) {
				// Full resolved on-disk path (from the subgraph resolution)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.60f, 1.0f));
				ImGui::TextWrapped("%s", entry.fullPath.c_str());
				ImGui::PopStyleColor();
			}

			ImGui::TableNextColumn();
			ImGui::TextWrapped("%s", entry.replacementPath.c_str());

			ImGui::TableNextColumn();
			ImGui::TextWrapped("%s", entry.subModName.c_str());

			ImGui::TableNextColumn();
			// Re-evaluate conditions live against the current game state
			bool currentlyPassing = false;
			if (entry.subMod && entry.actorFormID != 0) {
				auto* form = RE::TESForm::GetFormByID(entry.actorFormID);
				auto* refr = form ? form->As<RE::TESObjectREFR>() : nullptr;
				if (refr) {
					// Pass nullptr for clipGen — timing conditions will return false,
					// but all gameplay conditions evaluate correctly
					currentlyPassing = entry.subMod->EvaluateConditions(refr, nullptr);
				}
			} else if (!entry.subMod) {
				// No condition set (unconditional replacement) — always active
				currentlyPassing = true;
			}

			if (currentlyPassing) {
				ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "YES");
			} else {
				ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "NO");
			}
		}

		ImGui::EndTable();
	}
}
