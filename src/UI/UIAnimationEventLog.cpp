#include "UI/UIAnimationEventLog.h"
#include "UI/UICommon.h"
#include "AnimationLog.h"

#include <imgui.h>

void UIAnimationEventLog::DrawContents()
{
	ImGui::SetNextItemWidth(200);
	ImGui::InputTextWithHint("##evtFilter", "Filter events...", filterText, sizeof(filterText));
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
	if (ImGui::Button("Clear")) {
		AnimationLog::GetSingleton()->ClearAnimEvents();
	}

	ImGui::Separator();

	auto& entries = AnimationLog::GetSingleton()->GetAnimEventEntries();

	ImGui::BeginChild("EventEntries", ImVec2(0, 0), false);

	for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
		const auto& e = *it;

		if (filterText[0] != '\0' &&
			!UICommon::FuzzyMatch(filterText, e.originalAnim.c_str()) &&
			!UICommon::FuzzyMatch(filterText, e.refrName.c_str())) {
			continue;
		}

		auto elapsed = std::chrono::steady_clock::now() - e.timestamp;
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

		ImVec4 timingColor;
		if (ms < 500) timingColor = UICommon::Colors::TimingShort;
		else if (ms < 2000) timingColor = UICommon::Colors::TimingMedium;
		else timingColor = UICommon::Colors::TimingLong;

		ImGui::TextColored(timingColor, "[%lldms]", ms);
		ImGui::SameLine();
		ImGui::Text("%s (0x%08X):", e.refrName.c_str(), e.refrFormID);
		ImGui::SameLine();
		ImGui::TextColored(UICommon::Colors::LogEvent, "%s", e.originalAnim.c_str());
	}

	ImGui::EndChild();
}
