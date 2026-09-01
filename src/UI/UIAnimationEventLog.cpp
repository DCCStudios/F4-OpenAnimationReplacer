#include "UI/Localization.h"
#include "UI/UIAnimationEventLog.h"
#include "UI/UICommon.h"
#include "AnimationLog.h"

#include <imgui.h>

void UIAnimationEventLog::DrawContents()
{
	ImGui::SetNextItemWidth(200);
	ImGui::InputTextWithHint(UICommon::StableID("##evtFilter"), UICommon::T("Filter events..."), filterText, sizeof(filterText));
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
	if (ImGui::Button(UICommon::T("Clear"))) {
		AnimationLog::GetSingleton()->ClearAnimEvents();
	}

	ImGui::Separator();

	auto& entries = AnimationLog::GetSingleton()->GetAnimEventEntries();

	ImGui::BeginChild(UICommon::StableID("EventEntries"), ImVec2(0, 0), false);

	for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
		const auto& e = *it;

		if (filterText[0] != '\0' &&
			!UICommon::FuzzyMatch(filterText, e.originalAnim.c_str()) &&
			!UICommon::FuzzyMatch(filterText, e.refrName.c_str()) &&
			!UICommon::FuzzyMatch(filterText, e.sourceAnim.c_str())) {
			continue;
		}

		auto elapsed = std::chrono::steady_clock::now() - e.timestamp;
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

		ImVec4 timingColor;
		if (ms < 500) timingColor = UICommon::Colors::TimingShort;
		else if (ms < 2000) timingColor = UICommon::Colors::TimingMedium;
		else timingColor = UICommon::Colors::TimingLong;

		ImGui::TextColored(timingColor, UICommon::T("[%lldms]"), ms);
		ImGui::SameLine();
		ImGui::Text(UICommon::T("%s (0x%08X):"), e.refrName.c_str(), e.refrFormID);
		ImGui::SameLine();
		ImGui::TextColored(UICommon::Colors::LogEvent, UICommon::T("%s"), e.originalAnim.c_str());

		// Source animation. A leading '~' marks the engine-event fallback (most
		// recently activated clip on the actor) rather than an exact attribution.
		if (!e.sourceAnim.empty()) {
			const bool guessed = (e.sourceAnim[0] == '~');
			const char* shown = guessed ? e.sourceAnim.c_str() + 1 : e.sourceAnim.c_str();
			ImGui::SameLine();
			ImGui::TextDisabled(guessed ? UICommon::T("from ~%s") : UICommon::T("from %s"), shown);
			if (guessed && ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s", UICommon::T(
					"Best guess: engine-fired events carry no clip identity, so this is the most "
					"recently activated clip on this actor. Events OAR fires itself are attributed exactly."));
			}
		}
	}

	ImGui::EndChild();
}
