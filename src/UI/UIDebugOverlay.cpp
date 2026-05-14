#include "UI/UIDebugOverlay.h"
#include "UI/UICommon.h"
#include "ActiveReplacementTracker.h"

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
				ImGui::Text("%s [%08X]", entry.actorName.c_str(), entry.actorFormID);
			} else {
				ImGui::TextDisabled("(unknown)");
			}

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(entry.clipSuffix.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(entry.replacementPath.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(entry.subModName.c_str());

			ImGui::TableNextColumn();
			if (entry.conditionsPassed) {
				ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "YES");
			} else {
				ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "NO");
			}
		}

		ImGui::EndTable();
	}
}
