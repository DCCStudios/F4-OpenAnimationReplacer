#include "UI/Localization.h"
#include "UI/UIDebugOverlay.h"
#include "UI/UICommon.h"
#include "ActiveReplacementTracker.h"
#include "ReplacerMods.h"

#include <imgui.h>
#include <algorithm>
#include <cstdio>

void UIDebugOverlay::DrawContents()
{
	auto* tracker = ActiveReplacementTracker::GetSingleton();
	// This window is drawn every frame it is visible, so flag live view: Update()
	// then bumps active clips every frame and PurgeStale() switches to a tight
	// eviction window, so the list reflects what is actually replacing RIGHT NOW
	// (a clip that stops — condition flips, weapon holstered — drops within a
	// couple of frames instead of lingering up to 30s).
	tracker->SetLiveViewActive();
	tracker->PurgeStale();
	auto snapshot = tracker->GetSnapshot();

	// Actor filter (mirrors the Animation Log's "Filter by Actor"): show only the
	// rows for one actor by FormID. "Player" fills in 0x14.
	ImGui::Checkbox(UICommon::T("Filter by Actor"), &showOnlyActor);
	if (showOnlyActor) {
		ImGui::SameLine();
		if (targetFormIDBuf[0] == '\0') {
			snprintf(targetFormIDBuf, sizeof(targetFormIDBuf), "0x%X", targetFormID);
		}
		ImGui::SetNextItemWidth(120);
		if (ImGui::InputText(UICommon::StableID("##arTargetID"), targetFormIDBuf, sizeof(targetFormIDBuf))) {
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

		if (targetFormID != 0) {
			std::erase_if(snapshot, [&](const ActiveReplacementEntry& e) {
				return e.actorFormID != targetFormID;
			});
		}
	}

	ImGui::Text(UICommon::T("Active Replacements: %zu"), snapshot.size());
	ImGui::Separator();

	if (snapshot.empty()) {
		ImGui::TextDisabled(UICommon::T("No animations currently replaced."));
		return;
	}

	std::sort(snapshot.begin(), snapshot.end(), [](const auto& a, const auto& b) {
		if (a.actorFormID != b.actorFormID) return a.actorFormID < b.actorFormID;
		return a.clipSuffix < b.clipSuffix;
	});

	if (ImGui::BeginTable(UICommon::StableID("##ActiveReplacements"), 5,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
		ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {

		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn(UICommon::T("Actor"), ImGuiTableColumnFlags_WidthFixed, 140.f);
		ImGui::TableSetupColumn(UICommon::T("Clip"), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn(UICommon::T("Replacement"), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn(UICommon::T("SubMod"), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn(UICommon::T("Active"), ImGuiTableColumnFlags_WidthFixed, 50.f);
		ImGui::TableHeadersRow();

		for (auto& entry : snapshot) {
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			if (entry.actorFormID != 0) {
				ImGui::TextWrapped(UICommon::T("%s [%08X]"), entry.actorName.c_str(), entry.actorFormID);
			} else {
				ImGui::TextDisabled(UICommon::T("(unknown)"));
			}

			// Wrap long clip names / paths at the column edge so the full text is
			// always visible (rows grow vertically instead of clipping).
			ImGui::TableNextColumn();
			ImGui::TextWrapped(UICommon::T("%s"), entry.clipSuffix.c_str());
			if (!entry.fullPath.empty()) {
				// Full resolved on-disk path (from the subgraph resolution)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.60f, 1.0f));
				ImGui::TextWrapped(UICommon::T("%s"), entry.fullPath.c_str());
				ImGui::PopStyleColor();
			}

			ImGui::TableNextColumn();
			ImGui::TextWrapped(UICommon::T("%s"), entry.replacementPath.c_str());

			ImGui::TableNextColumn();
			ImGui::TextWrapped(UICommon::T("%s"), entry.subModName.c_str());
			// Other submods whose conditions also pass for this clip but lost to
			// the winner on priority — shown dim and tagged so it's clear they are
			// matching-but-overridden, not replacing.
			for (const auto& overridden : entry.overriddenSubMods) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.55f, 0.30f, 1.0f));
				ImGui::TextWrapped(UICommon::T("%s (overridden)"), overridden.c_str());
				ImGui::PopStyleColor();
			}

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
				ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), UICommon::T("YES"));
			} else {
				ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), UICommon::T("NO"));
			}
		}

		ImGui::EndTable();
	}
}
