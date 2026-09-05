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
	// Both visibility queries run the main-menu gate (Tick) so the ImGui frame is
	// only kept alive while the banner is actually on screen.
	bool ShouldDraw() const override { return Tick(); }
	bool ShouldDrawOverlay() const override { return Tick(); }
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
	// Give up waiting for the main menu after this long (a save launched straight
	// from the command line never shows it). Bounds the per-frame menu query.
	static constexpr float kMaxWaitForMainMenu = 300.0f;

	// Main-menu gate, mirrors F4SE Menu Framework's WelcomeBanner: the banner is
	// a title-screen hint. It draws ONLY while "MainMenu" is open, its clock
	// starts on the first frame the menu is actually up (not at kGameDataReady,
	// which is several seconds before the menu is visible), and it finishes the
	// instant the menu closes so it never floats over the 3D world. Called from
	// the const visibility queries on the render thread, hence the mutable state.
	bool Tick() const;
	void Finish() const;
	float ElapsedSeconds() const;

	mutable std::chrono::steady_clock::time_point armTime{};
	mutable std::chrono::steady_clock::time_point startTime{};
	mutable bool active{ false };
	mutable bool started{ false };
	mutable bool loggedDiag{ false };
};
