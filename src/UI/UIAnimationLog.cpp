#include "UI/Localization.h"
#include "UI/UIAnimationLog.h"
#include "UI/UICommon.h"
#include "AnimationLog.h"

#include <imgui.h>
#include <algorithm>
#include <format>

uint32_t UIAnimationLog::GetConsoleTargetFormID() const
{
	return targetFormID;
}

void UIAnimationLog::DrawContents()
{
	ImGui::Checkbox(UICommon::T("Activate"), &showActivate); ImGui::SameLine();
	ImGui::Checkbox(UICommon::T("Replace"), &showReplace); ImGui::SameLine();
	ImGui::Checkbox(UICommon::T("Loop"), &showLoop); ImGui::SameLine();
	ImGui::Checkbox(UICommon::T("Echo"), &showEcho); ImGui::SameLine();

	// Perspective filters: which animation graph the clip came from.
	// Entries whose graph could not be classified are always shown.
	ImGui::Checkbox(UICommon::T("1st Person"), &showFirstPerson); ImGui::SameLine();
	ImGui::Checkbox(UICommon::T("3rd Person"), &showThirdPerson); ImGui::SameLine();

	ImGui::Checkbox(UICommon::T("Filter by Actor"), &showOnlyConsoleTarget);

	uint32_t consoleTargetID = 0;
	if (showOnlyConsoleTarget) {
		ImGui::SameLine();
		if (targetFormIDBuf[0] == '\0') {
			snprintf(targetFormIDBuf, sizeof(targetFormIDBuf), "0x%X", targetFormID);
		}
		ImGui::SetNextItemWidth(120);
		if (ImGui::InputText(UICommon::StableID("##targetID"), targetFormIDBuf, sizeof(targetFormIDBuf))) {
			try { targetFormID = std::stoul(targetFormIDBuf, nullptr, 16); } catch (...) {}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(UICommon::T("Enter actor FormID (e.g. 0x14 for player)"));
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(UICommon::T("Player"))) {
			targetFormID = 0x14;
			snprintf(targetFormIDBuf, sizeof(targetFormIDBuf), "0x14");
		}
		consoleTargetID = targetFormID;
	}

	ImGui::SetNextItemWidth(200);
	ImGui::InputTextWithHint(UICommon::StableID("##logFilter"), UICommon::T("Filter..."), filterText, sizeof(filterText));

	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 130);
	if (ImGui::Button(UICommon::T("Clear"))) {
		AnimationLog::GetSingleton()->Clear();
	}
	ImGui::SameLine();
	if (ImGui::Button(UICommon::T("Scroll to End"))) scrollToBottom = true;

	ImGui::Separator();

	auto& entries = AnimationLog::GetSingleton()->GetEntries();

	ImGui::BeginChild(UICommon::StableID("LogEntries"), ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
		const auto& e = *it;

		switch (e.type) {
		case AnimationLog::EventType::kActivate: if (!showActivate) continue; break;
		case AnimationLog::EventType::kReplace:  if (!showReplace) continue; break;
		case AnimationLog::EventType::kLoop:     if (!showLoop) continue; break;
		case AnimationLog::EventType::kEcho:     if (!showEcho) continue; break;
		default: break;
		}

		// Perspective filter: 1st person = path contains "_1stperson";
		// everything else (including unclassified) counts as 3rd person.
		if (e.perspective == AnimationLog::Perspective::kFirstPerson) {
			if (!showFirstPerson) continue;
		} else {
			if (!showThirdPerson) continue;
		}

		if (showOnlyConsoleTarget && consoleTargetID != 0 && e.refrFormID != consoleTargetID) continue;

		if (filterText[0] != '\0') {
			if (!UICommon::FuzzyMatch(filterText, e.originalAnim.c_str()) &&
				!UICommon::FuzzyMatch(filterText, e.replacementAnim.c_str()) &&
				!UICommon::FuzzyMatch(filterText, e.fullPath.c_str()) &&
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
			typeStr = !e.replacementAnim.empty() ? UICommon::T("Activate [Replaced]") : UICommon::T("Activate");
			isReplaced = !e.replacementAnim.empty();
			break;
		case AnimationLog::EventType::kReplace:
			typeColor = UICommon::Colors::LogReplace; typeStr = UICommon::T("Replace"); isReplaced = true; break;
		case AnimationLog::EventType::kLoop:
			typeColor = UICommon::Colors::LogLoop; typeStr = UICommon::T("Loop"); break;
		case AnimationLog::EventType::kEcho:
			typeColor = UICommon::Colors::LogEcho; typeStr = UICommon::T("Echo"); break;
		default:
			typeColor = UICommon::Colors::LogEvent; typeStr = UICommon::T("???"); break;
		}

		// Perspective tag shown next to the event type (unclassified counts as 3rd)
		const char* perspStr =
			e.perspective == AnimationLog::Perspective::kFirstPerson ? UICommon::T("[1st]") : UICommon::T("[3rd]");

		// Compose the full-text lines up front so we can measure their wrapped
		// heights and size the entry child exactly — nothing gets clipped.
		std::string nameLine = std::string(UICommon::T("Name: ")) + e.originalAnim;
		std::string pathLine;
		if (!e.fullPath.empty()) {
			pathLine = std::string(UICommon::T("Path: ")) + e.fullPath;
		}
		std::string modLine;
		std::string replLine;
		if (isReplaced && !e.replacementAnim.empty()) {
			if (!e.subModName.empty()) {
				modLine = std::string(UICommon::T("Mod: ")) + e.subModName;
			}
			replLine = std::string(UICommon::T("Replacement: ")) + e.replacementAnim;
		}

		const auto& style = ImGui::GetStyle();
		// Wrap width = inner width of the entry child (its padding on both sides)
		float wrapWidth = ImGui::GetContentRegionAvail().x - style.WindowPadding.x * 2.0f;
		if (wrapWidth < 100.0f) wrapWidth = 100.0f;

		float entryHeight = style.WindowPadding.y * 2.0f + ImGui::GetTextLineHeight();  // header row
		auto addLineHeight = [&](const std::string& a_line) {
			if (a_line.empty()) return;
			entryHeight += style.ItemSpacing.y +
				ImGui::CalcTextSize(a_line.c_str(), nullptr, false, wrapWidth).y;
		};
		addLineHeight(nameLine);
		addLineHeight(pathLine);
		addLineHeight(modLine);
		addLineHeight(replLine);

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.17f, 1.0f));
		ImGui::BeginChild(ImGui::GetID(static_cast<const void*>(&e)), ImVec2(0, entryHeight), true);

		// Header: event type, perspective tag, actor right-aligned
		ImGui::TextColored(typeColor, UICommon::T("%s"), typeStr);
		if (perspStr) {
			ImGui::SameLine();
			ImGui::TextColored(UICommon::Colors::Disabled, UICommon::T("%s"), perspStr);
		}
		if (!e.refrName.empty()) {
			std::string actorText = std::format("{} (0x{:08X})", e.refrName, e.refrFormID);
			float actorWidth = ImGui::CalcTextSize(actorText.c_str()).x;
			// Right edge in local coords = cursor + remaining width on this line
			ImGui::SameLine();
			float rightEdge = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
			// Only right-align if it won't overlap the type/perspective tags
			float actorPos = std::max(rightEdge - actorWidth, ImGui::GetCursorPosX() + style.ItemSpacing.x);
			// SameLine, not SetCursorPosX — see UICommon::DrawConditionEvalResult
			// for why (avoids ImGui's SetCursorPos-boundary debug log spam).
			ImGui::SameLine(actorPos);
			ImGui::TextColored(UICommon::Colors::Disabled, UICommon::T("%s"), actorText.c_str());
		}

		// Full animation name + resolved on-disk path, wrapped so long paths
		// are always fully visible.
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextUnformatted(nameLine.c_str());
		if (!pathLine.empty()) {
			ImGui::TextColored(UICommon::Colors::Disabled, UICommon::T("%s"), pathLine.c_str());
		}
		if (!modLine.empty()) {
			ImGui::TextUnformatted(modLine.c_str());
		}
		if (!replLine.empty()) {
			ImGui::TextColored(UICommon::Colors::AccentBlue, UICommon::T("%s"), replLine.c_str());
		}
		ImGui::PopTextWrapPos();

		ImGui::EndChild();
		ImGui::PopStyleColor();
	}

	if (scrollToBottom) {
		ImGui::SetScrollHereY(1.0f);
		scrollToBottom = false;
	}

	ImGui::EndChild();
}
