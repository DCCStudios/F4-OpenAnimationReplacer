#include "UI/Localization.h"
#include "UI/UIWelcomeBanner.h"
#include "UI/UICommon.h"
#include "Settings.h"

#include <cstring>

#include <imgui.h>

void UIWelcomeBanner::Show()
{
	if (!Settings::GetSingleton()->bShowWelcomeBanner) return;
	active = true;
	startTime = std::chrono::steady_clock::now();
	loggedDiag = false;
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
	active = true;
	startTime = std::chrono::steady_clock::now();
	loggedDiag = false;
}

float UIWelcomeBanner::ElapsedSeconds() const
{
	if (startTime == std::chrono::steady_clock::time_point{}) {
		return 0.f;
	}
	return std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
}

float UIWelcomeBanner::GetWindowAlpha() const
{
	if (!active) return 0.f;
	const float elapsed = ElapsedSeconds();
	if (elapsed < kFadeInTime) {
		return elapsed / kFadeInTime;
	}
	if (elapsed > kDisplayDuration - kFadeOutTime) {
		const float a = (kDisplayDuration - elapsed) / kFadeOutTime;
		return a < 0.f ? 0.f : a;
	}
	return 1.f;
}

void UIWelcomeBanner::DrawContents()
{
	const float elapsed = ElapsedSeconds();
	if (elapsed >= kDisplayDuration) {
		active = false;
		return;
	}

	auto viewport = ImGui::GetMainViewport();
	ImGui::SetWindowPos(ImVec2(
		viewport->WorkPos.x + (viewport->WorkSize.x - ImGui::GetWindowWidth()) * 0.5f,
		viewport->WorkPos.y + 40.f));

	auto* settings = Settings::GetSingleton();
	std::string keyStr;
	if (settings->bRequireShift) keyStr += "Shift+";
	keyStr += UICommon::DIKCodeToName(settings->iToggleKey);

	const char* titleText = UICommon::T("Open Animation Replacer");
	std::string hotkeyMsg = std::vformat(UICommon::T("Press {} to open the editor"), std::make_format_args(keyStr));

	// One-shot diagnostic so the log gives ground truth for what the banner is
	// actually drawing (fires once per Show()).
	if (!loggedDiag) {
		loggedDiag = true;
		logger::info("[OAR-Banner] Draw: active={} elapsed={:.3f} alpha={:.3f} winW={:.1f} title='{}' (len {}) hotkey='{}' (len {})",
			active, elapsed, GetWindowAlpha(), ImGui::GetWindowWidth(),
			titleText ? titleText : "(null)", titleText ? std::strlen(titleText) : 0u,
			hotkeyMsg, hotkeyMsg.size());
	}

	// No per-text alpha here: UIWindow::TryDraw fades the whole window (background
	// + content) via GetWindowAlpha() before Begin(), so the box and text fade
	// together instead of leaving an opaque box with invisible text.
	float textW = ImGui::CalcTextSize(titleText).x;
	ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textW) * 0.5f);
	ImGui::TextColored(UICommon::Colors::AccentBlue, "%s", titleText);

	float msgW = ImGui::CalcTextSize(hotkeyMsg.c_str()).x;
	ImGui::SetCursorPosX((ImGui::GetWindowWidth() - msgW) * 0.5f);
	ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "%s", hotkeyMsg.c_str());
}
