#include "UI/Localization.h"
#include "UI/UIWelcomeBanner.h"
#include "UI/UICommon.h"
#include "Settings.h"

#include <cstring>

#include <imgui.h>

void UIWelcomeBanner::Show()
{
	if (!Settings::GetSingleton()->bShowWelcomeBanner) return;
	if (active) return;  // already armed or showing
	active = true;
	started = false;  // clock starts on the first frame the main menu is up
	armTime = std::chrono::steady_clock::now();
	startTime = {};
	loggedDiag = false;
}

void UIWelcomeBanner::OnOpen()
{
	Show();
}

void UIWelcomeBanner::Finish() const
{
	active = false;
	started = false;
}

bool UIWelcomeBanner::Tick() const
{
	if (!active) return false;

	static const RE::BSFixedString kMainMenu("MainMenu");
	auto* ui = RE::UI::GetSingleton();
	const bool mainMenuOpen = ui && ui->GetMenuOpen(kMainMenu);
	if (!mainMenuOpen) {
		if (started) {
			// The main menu was up and just closed (the player loaded in): done.
			Finish();
			return false;
		}
		// Still in the pre-menu load. Keep waiting, but not forever.
		const float waited = std::chrono::duration<float>(std::chrono::steady_clock::now() - armTime).count();
		if (waited > kMaxWaitForMainMenu) {
			logger::info("[OAR-Banner] Main menu never appeared within {:.0f}s; banner cancelled", kMaxWaitForMainMenu);
			Finish();
		}
		return false;
	}

	if (!started) {
		started = true;
		startTime = std::chrono::steady_clock::now();
	}
	if (ElapsedSeconds() >= kDisplayDuration) {
		Finish();  // held its full duration on the main menu
		return false;
	}
	return true;
}

ImGuiWindowFlags UIWelcomeBanner::GetWindowFlags() const
{
	return ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
	       ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize |
	       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
}

float UIWelcomeBanner::ElapsedSeconds() const
{
	if (!started || startTime == std::chrono::steady_clock::time_point{}) {
		return 0.f;
	}
	return std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
}

float UIWelcomeBanner::GetWindowAlpha() const
{
	if (!active || !started) return 0.f;
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
	// Tick() (via ShouldDraw) already gated on the main menu and the duration
	// before Begin(); nothing to re-check here.
	const float elapsed = ElapsedSeconds();

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
