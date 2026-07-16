#include "UI/UIWelcomeBanner.h"
#include "UI/UICommon.h"
#include "Settings.h"

#include <imgui.h>

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
	keyStr += UICommon::DIKCodeToName(settings->iToggleKey);

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
