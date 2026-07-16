#include "UI/UICommon.h"
#include "BaseConditions.h"
#include "Conditions.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace UICommon
{
	void DrawComparisonOperatorCombo(const char* a_label, ComparisonOperator& a_op)
	{
		static const char* names[] = { "==", "!=", ">", ">=", "<", "<=" };
		int current = static_cast<int>(a_op);
		if (ImGui::Combo(a_label, &current, names, IM_ARRAYSIZE(names))) {
			a_op = static_cast<ComparisonOperator>(current);
		}
	}

	void HelpMarker(const char* a_desc)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::BeginItemTooltip()) {
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(a_desc);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	bool DrawFormPicker(const char* a_label, FormComponent& a_form)
	{
		bool changed = false;
		ImGui::PushID(a_label);

		char pluginBuf[256];
		strncpy_s(pluginBuf, a_form.pluginName.c_str(), sizeof(pluginBuf) - 1);
		if (ImGui::InputText("Plugin", pluginBuf, sizeof(pluginBuf))) {
			a_form.pluginName = pluginBuf;
			changed = true;
		}

		char formIDBuf[32];
		snprintf(formIDBuf, sizeof(formIDBuf), "0x%X", a_form.localFormID);
		if (ImGui::InputText("Form ID", formIDBuf, sizeof(formIDBuf))) {
			try {
				a_form.localFormID = std::stoul(formIDBuf, nullptr, 16);
				changed = true;
			} catch (...) {}
		}

		ImGui::PopID();
		return changed;
	}

	void TextUnformattedColored(const ImVec4& a_color, const char* a_text)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, a_color);
		ImGui::TextUnformatted(a_text);
		ImGui::PopStyleColor();
	}

	void TextUnformattedDisabled(const char* a_text)
	{
		TextUnformattedColored(Colors::Disabled, a_text);
	}

	void DrawConditionEvalResult(bool a_result, bool a_hasValue)
	{
		ImGui::SameLine();
		float rightEdge = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX();
		ImGui::SetCursorPosX(rightEdge - 15.f);
		ImVec2 indicatorCenter = ImGui::GetCursorScreenPos();
		const float offset = ImGui::GetTextLineHeightWithSpacing() * 0.25f;
		indicatorCenter.y += offset * 2.f;
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		if (!a_hasValue) {
			drawList->AddText(ImVec2(indicatorCenter.x - offset, indicatorCenter.y - offset),
				ImGui::GetColorU32(ImVec4(0.5f, 0.5f, 0.5f, 1.0f)), "?");
		} else if (a_result) {
			const ImVec2 points[3] = {
				ImVec2(indicatorCenter.x - offset, indicatorCenter.y),
				ImVec2(indicatorCenter.x - offset * 0.5f, indicatorCenter.y + offset),
				ImVec2(indicatorCenter.x + offset, indicatorCenter.y - offset)
			};
			drawList->AddPolyline(points, 3,
				ImGui::GetColorU32(Colors::Success),
				ImDrawFlags_None, 2.f);
		} else {
			ImU32 failCol = ImGui::GetColorU32(Colors::Failure);
			drawList->AddLine(
				ImVec2(indicatorCenter.x - offset, indicatorCenter.y - offset),
				ImVec2(indicatorCenter.x + offset, indicatorCenter.y + offset), failCol, 2.f);
			drawList->AddLine(
				ImVec2(indicatorCenter.x - offset, indicatorCenter.y + offset),
				ImVec2(indicatorCenter.x + offset, indicatorCenter.y - offset), failCol, 2.f);
		}
	}

	void DrawWarningIcon(const char* a_tooltip)
	{
		ImGui::TextColored(Colors::Warning, "!");
		if (ImGui::IsItemHovered() && a_tooltip) {
			ImGui::SetTooltip("%s", a_tooltip);
		}
	}

	bool ButtonWithConfirmationModal(const char* a_buttonLabel, const char* a_modalTitle, const char* a_message)
	{
		bool confirmed = false;

		if (ImGui::Button(a_buttonLabel)) {
			ImGui::OpenPopup(a_modalTitle);
		}

		if (ImGui::BeginPopupModal(a_modalTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::TextUnformatted(a_message);
			ImGui::Separator();

			if (ImGui::Button("Yes", ImVec2(120, 0))) {
				confirmed = true;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("No", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		return confirmed;
	}

	void DrawTreeLine(float a_indentX, float a_topY, float a_bottomY)
	{
		auto* drawList = ImGui::GetWindowDrawList();
		ImU32 col = ImGui::ColorConvertFloat4ToU32(Colors::TreeLine);
		float x = a_indentX + 8.f;
		drawList->AddLine(ImVec2(x, a_topY), ImVec2(x, a_bottomY), col, 1.0f);
		drawList->AddLine(ImVec2(x, a_bottomY), ImVec2(x + 10.f, a_bottomY), col, 1.0f);
	}

	float FirstColumnWidth(float a_windowWidth, float a_percent)
	{
		return a_windowWidth * a_percent;
	}

	void ApplyOARStyle()
	{
		auto& style = ImGui::GetStyle();
		style.WindowRounding = 4.0f;
		style.FrameRounding = 3.0f;
		style.GrabRounding = 2.0f;
		style.ScrollbarRounding = 3.0f;
		style.TabRounding = 3.0f;
		style.WindowPadding = ImVec2(8, 8);
		style.FramePadding = ImVec2(5, 3);
		style.ItemSpacing = ImVec2(8, 4);
		style.IndentSpacing = 20.0f;
		style.ScrollbarSize = 14.0f;

		auto& colors = style.Colors;
		colors[ImGuiCol_WindowBg]          = ImVec4(0.10f, 0.10f, 0.12f, 0.96f);
		colors[ImGuiCol_ChildBg]           = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
		colors[ImGuiCol_Border]            = ImVec4(0.25f, 0.25f, 0.30f, 0.50f);
		colors[ImGuiCol_FrameBg]           = ImVec4(0.16f, 0.16f, 0.20f, 0.54f);
		colors[ImGuiCol_FrameBgHovered]    = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
		colors[ImGuiCol_FrameBgActive]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
		colors[ImGuiCol_TitleBg]           = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
		colors[ImGuiCol_TitleBgActive]     = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
		colors[ImGuiCol_MenuBarBg]         = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
		colors[ImGuiCol_Header]            = ImVec4(0.22f, 0.22f, 0.26f, 0.80f);
		colors[ImGuiCol_HeaderHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.60f);
		colors[ImGuiCol_HeaderActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
		colors[ImGuiCol_Tab]               = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
		colors[ImGuiCol_TabHovered]        = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
		colors[ImGuiCol_TabSelected]       = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
		colors[ImGuiCol_Button]            = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
		colors[ImGuiCol_ButtonHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
		colors[ImGuiCol_ButtonActive]      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_Separator]         = ImVec4(0.30f, 0.30f, 0.35f, 0.50f);
		colors[ImGuiCol_ResizeGrip]        = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
		colors[ImGuiCol_ResizeGripActive]  = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	}

	std::string ShortenAnimPath(const std::string& a_path)
	{
		std::string result = a_path;

		auto toLower = [](std::string s) {
			for (auto& c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
			return s;
		};

		std::string lower = toLower(result);

		static const char* prefixes[] = {
			"data\\meshes\\actors\\character\\",
			"data/meshes/actors/character/",
			"data\\meshes\\",
			"data/meshes/",
			"meshes\\actors\\character\\",
			"meshes/actors/character/",
			"meshes\\",
			"meshes/",
			"..\\..\\",
			"../../",
			"..\\",
			"../",
		};

		for (const auto* prefix : prefixes) {
			std::string lp = toLower(std::string(prefix));
			if (lower.starts_with(lp)) {
				result = result.substr(lp.size());
				break;
			}
		}

		for (auto& c : result) {
			if (c == '/') c = '\\';
		}

		return result;
	}

	bool FuzzyMatch(const char* a_pattern, const char* a_str)
	{
		if (!a_pattern || !a_str) return false;
		if (*a_pattern == '\0') return true;

		const char* p = a_pattern;
		const char* s = a_str;

		while (*p && *s) {
			char pc = static_cast<char>(tolower(static_cast<unsigned char>(*p)));
			char sc = static_cast<char>(tolower(static_cast<unsigned char>(*s)));
			if (pc == sc) p++;
			s++;
		}

		return *p == '\0';
	}

	const char* DIKCodeToName(std::uint32_t a_dik)
	{
		switch (a_dik) {
		case 0x01: return "Escape";
		case 0x02: return "1"; case 0x03: return "2"; case 0x04: return "3";
		case 0x05: return "4"; case 0x06: return "5"; case 0x07: return "6";
		case 0x08: return "7"; case 0x09: return "8"; case 0x0A: return "9";
		case 0x0B: return "0";
		case 0x0C: return "-"; case 0x0D: return "=";
		case 0x0E: return "Backspace"; case 0x0F: return "Tab";
		case 0x10: return "Q"; case 0x11: return "W"; case 0x12: return "E";
		case 0x13: return "R"; case 0x14: return "T"; case 0x15: return "Y";
		case 0x16: return "U"; case 0x17: return "I"; case 0x18: return "O";
		case 0x19: return "P"; case 0x1A: return "["; case 0x1B: return "]";
		case 0x1C: return "Enter";
		case 0x1E: return "A"; case 0x1F: return "S"; case 0x20: return "D";
		case 0x21: return "F"; case 0x22: return "G"; case 0x23: return "H";
		case 0x24: return "J"; case 0x25: return "K"; case 0x26: return "L";
		case 0x27: return ";"; case 0x28: return "'"; case 0x29: return "`";
		case 0x2B: return "\\";
		case 0x2C: return "Z"; case 0x2D: return "X"; case 0x2E: return "C";
		case 0x2F: return "V"; case 0x30: return "B"; case 0x31: return "N";
		case 0x32: return "M"; case 0x33: return ","; case 0x34: return ".";
		case 0x35: return "/";
		case 0x37: return "Numpad*"; case 0x39: return "Space";
		case 0x3B: return "F1"; case 0x3C: return "F2"; case 0x3D: return "F3";
		case 0x3E: return "F4"; case 0x3F: return "F5"; case 0x40: return "F6";
		case 0x41: return "F7"; case 0x42: return "F8"; case 0x43: return "F9";
		case 0x44: return "F10"; case 0x57: return "F11"; case 0x58: return "F12";
		case 0x47: return "Numpad7"; case 0x48: return "Numpad8"; case 0x49: return "Numpad9";
		case 0x4B: return "Numpad4"; case 0x4C: return "Numpad5"; case 0x4D: return "Numpad6";
		case 0x4F: return "Numpad1"; case 0x50: return "Numpad2"; case 0x51: return "Numpad3";
		case 0x52: return "Numpad0"; case 0x53: return "Numpad.";
		case 0xC7: return "Home"; case 0xC8: return "Up"; case 0xC9: return "PgUp";
		case 0xCB: return "Left"; case 0xCD: return "Right";
		case 0xCF: return "End"; case 0xD0: return "Down"; case 0xD1: return "PgDn";
		case 0xD2: return "Insert"; case 0xD3: return "Delete";
		default: return "?";
		}
	}
}
