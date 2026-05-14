#include "UI/UIAnimationLog.h"
#include "UI/UICommon.h"
#include "AnimationLog.h"

#include <imgui.h>

uint32_t UIAnimationLog::GetConsoleTargetFormID() const
{
	return targetFormID;
}

void UIAnimationLog::DrawContents()
{
	ImGui::Checkbox("Activate", &showActivate); ImGui::SameLine();
	ImGui::Checkbox("Replace", &showReplace); ImGui::SameLine();
	ImGui::Checkbox("Loop", &showLoop); ImGui::SameLine();
	ImGui::Checkbox("Echo", &showEcho); ImGui::SameLine();

	ImGui::Checkbox("Filter by Actor", &showOnlyConsoleTarget);

	uint32_t consoleTargetID = 0;
	if (showOnlyConsoleTarget) {
		ImGui::SameLine();
		if (targetFormIDBuf[0] == '\0') {
			snprintf(targetFormIDBuf, sizeof(targetFormIDBuf), "0x%X", targetFormID);
		}
		ImGui::SetNextItemWidth(120);
		if (ImGui::InputText("##targetID", targetFormIDBuf, sizeof(targetFormIDBuf))) {
			try { targetFormID = std::stoul(targetFormIDBuf, nullptr, 16); } catch (...) {}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Enter actor FormID (e.g. 0x14 for player)");
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Player")) {
			targetFormID = 0x14;
			snprintf(targetFormIDBuf, sizeof(targetFormIDBuf), "0x14");
		}
		consoleTargetID = targetFormID;
	}

	ImGui::SetNextItemWidth(200);
	ImGui::InputTextWithHint("##logFilter", "Filter...", filterText, sizeof(filterText));

	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 130);
	if (ImGui::Button("Clear")) {
		AnimationLog::GetSingleton()->Clear();
	}
	ImGui::SameLine();
	if (ImGui::Button("Scroll to End")) scrollToBottom = true;

	ImGui::Separator();

	auto& entries = AnimationLog::GetSingleton()->GetEntries();

	ImGui::BeginChild("LogEntries", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
		const auto& e = *it;

		switch (e.type) {
		case AnimationLog::EventType::kActivate: if (!showActivate) continue; break;
		case AnimationLog::EventType::kReplace:  if (!showReplace) continue; break;
		case AnimationLog::EventType::kLoop:     if (!showLoop) continue; break;
		case AnimationLog::EventType::kEcho:     if (!showEcho) continue; break;
		default: break;
		}

		if (showOnlyConsoleTarget && consoleTargetID != 0 && e.refrFormID != consoleTargetID) continue;

		if (filterText[0] != '\0') {
			if (!UICommon::FuzzyMatch(filterText, e.originalAnim.c_str()) &&
				!UICommon::FuzzyMatch(filterText, e.replacementAnim.c_str()) &&
				!UICommon::FuzzyMatch(filterText, e.refrName.c_str()) &&
				!UICommon::FuzzyMatch(filterText, e.subModName.c_str())) {
				continue;
			}
		}

		ImVec4 typeColor;
		const char* typeStr;
		bool isReplaced = false;
		switch (e.type) {
		case AnimationLog::EventType::kActivate:
			typeColor = UICommon::Colors::LogActivate;
			typeStr = !e.replacementAnim.empty() ? "Activate [Replaced]" : "Activate";
			isReplaced = !e.replacementAnim.empty();
			break;
		case AnimationLog::EventType::kReplace:
			typeColor = UICommon::Colors::LogReplace; typeStr = "Replace"; isReplaced = true; break;
		case AnimationLog::EventType::kLoop:
			typeColor = UICommon::Colors::LogLoop; typeStr = "Loop"; break;
		case AnimationLog::EventType::kEcho:
			typeColor = UICommon::Colors::LogEcho; typeStr = "Echo"; break;
		default:
			typeColor = UICommon::Colors::LogEvent; typeStr = "???"; break;
		}

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.17f, 1.0f));
		float entryHeight = isReplaced ? 60.0f : 40.0f;
		ImGui::BeginChild(ImGui::GetID(static_cast<const void*>(&e)), ImVec2(0, entryHeight), true);

		ImGui::TextColored(typeColor, "%s", typeStr);

		std::string origShort = UICommon::ShortenAnimPath(e.originalAnim);
		ImGui::SameLine(200);
		ImGui::Text("Name: %s", origShort.c_str());

		if (!e.refrName.empty()) {
			ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 200);
			ImGui::TextColored(UICommon::Colors::Disabled, "%s (0x%08X)", e.refrName.c_str(), e.refrFormID);
		}

		if (isReplaced && !e.replacementAnim.empty()) {
			std::string replShort = UICommon::ShortenAnimPath(e.replacementAnim);
			if (!e.subModName.empty()) {
				ImGui::Text("Mod: %s", e.subModName.c_str());
				ImGui::SameLine(200);
			}
			ImGui::TextColored(UICommon::Colors::AccentBlue, "Path: %s", replShort.c_str());
		}

		ImGui::EndChild();
		ImGui::PopStyleColor();
	}

	if (scrollToBottom) {
		ImGui::SetScrollHereY(1.0f);
		scrollToBottom = false;
	}

	ImGui::EndChild();
}
