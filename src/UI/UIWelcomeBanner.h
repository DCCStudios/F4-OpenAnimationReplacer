#pragma once

#include "UI/UIWindow.h"

#include <chrono>

class UIWelcomeBanner : public UIWindow
{
public:
	UIWelcomeBanner() : UIWindow(WindowID::kWelcomeBanner, "##WelcomeBanner")
	{
		// MUST be independent: UIManager::RenderFrame only consults IsIndependent()
		// windows when deciding whether to keep the ImGui frame alive with no menu
		// open (it skips non-independent ones). Without this the banner's
		// ShouldDrawOverlay() is never checked and it never renders — matching the
		// progress bar / debug overlay / anim log, which all set this.
		independent = true;
	}

	void Show();

protected:
	bool ShouldDraw() const override;
	// Keep the render frame alive on our own while showing: UIManager::RenderFrame
	// (since PR #8's c63cb079) skips the whole ImGui frame unless an independent
	// window reports visible via IsOpen()/ShouldDrawOverlay(). Show() sets `active`
	// but not `isOpen`, and without this override the base returns false, so the
	// banner never rendered after that change. Mirror UIAnimationQueue's overlay.
	bool ShouldDrawOverlay() const override { return active; }
	// The base fades the whole window (background + text) by this value, so the
	// banner never renders as an opaque box with invisible text. Computed from a
	// wall clock (NOT accumulated ImGui DeltaTime) so the fade is correct
	// regardless of the render cadence in the independent-overlay path.
	float GetWindowAlpha() const override;
	void DrawContents() override;
	ImGuiWindowFlags GetWindowFlags() const override;
	ImVec2 GetDefaultSize() const override { return ImVec2(400, 80); }
	void OnOpen() override;

private:
	static constexpr float kDisplayDuration = 16.0f;
	static constexpr float kFadeInTime = 0.5f;
	static constexpr float kFadeOutTime = 2.0f;

	float ElapsedSeconds() const;

	std::chrono::steady_clock::time_point startTime{};
	bool active{ false };
	bool loggedDiag{ false };
};
