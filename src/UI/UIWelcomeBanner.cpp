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
	if (displayTimer > kDisplayDuration - 1.5f) {
		alpha = (kDisplayDuration - displayTimer) / 1.5f;
	}

	auto viewport = ImGui::GetMainViewport();
	ImGui::SetWindowPos(ImVec2(
		viewport->WorkPos.x + (viewport->WorkSize.x - ImGui::GetWindowWidth()) * 0.5f,
		viewport->WorkPos.y + 40.f));

	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
	ImGui::TextColored(UICommon::Colors::AccentBlue, "Open Animation Replacer v1.0.0");
	ImGui::TextColored(UICommon::Colors::Disabled, "Press Shift+O to toggle the UI");
	ImGui::PopStyleVar();
}
