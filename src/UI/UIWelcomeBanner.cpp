#include "UI/UIWelcomeBanner.h"
#include "UI/UICommon.h"
#include "Settings.h"

#include <imgui.h>

static const char* DIKCodeToName(uint32_t a_dik)
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

void UIWelcomeBanner::Show()
{
	if (!Settings::GetSingleton()->bShowWelcomeBanner) return;
	active = true;
	displayTimer = 0.f;
}

bool UIWelcomeBanner::ShouldDraw() const
{
	return active;
}

ImGuiWindowFlags UIWelcomeBanner::GetWindowFlags() const
{
	return ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
	       ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize |
	       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
}

void UIWelcomeBanner::OnOpen()
{
	displayTimer = 0.f;
	active = true;
}

void UIWelcomeBanner::DrawContents()
{
	displayTimer += ImGui::GetIO().DeltaTime;
	if (displayTimer >= kDisplayDuration) {
		active = false;
		return;
	}

	float alpha = 1.0f;
	// Fade in during first 0.5s
	if (displayTimer < 0.5f) {
		alpha = displayTimer / 0.5f;
	}
	// Fade out during last 2s
	else if (displayTimer > kDisplayDuration - 2.0f) {
		alpha = (kDisplayDuration - displayTimer) / 2.0f;
	}

	auto viewport = ImGui::GetMainViewport();
	ImGui::SetWindowPos(ImVec2(
		viewport->WorkPos.x + (viewport->WorkSize.x - ImGui::GetWindowWidth()) * 0.5f,
		viewport->WorkPos.y + 40.f));

	auto* settings = Settings::GetSingleton();
	std::string keyStr;
	if (settings->bRequireShift) keyStr += "Shift+";
	keyStr += DIKCodeToName(settings->iToggleKey);

	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

	float textW = ImGui::CalcTextSize("Open Animation Replacer").x;
	ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textW) * 0.5f);
	ImGui::TextColored(UICommon::Colors::AccentBlue, "Open Animation Replacer");

	std::string hotkeyMsg = std::format("Press {} to open the editor", keyStr);
	float msgW = ImGui::CalcTextSize(hotkeyMsg.c_str()).x;
	ImGui::SetCursorPosX((ImGui::GetWindowWidth() - msgW) * 0.5f);
	ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "%s", hotkeyMsg.c_str());

	ImGui::PopStyleVar();
}
