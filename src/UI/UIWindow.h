#pragma once

#include <imgui.h>

enum class WindowID : int32_t
{
	kMain = 0,
	kAnimationLog,
	kAnimationEventLog,
	kWelcomeBanner,
	kAnimationQueue,
	kDebugOverlay,
	kErrorBanner,
	kCount
};

class UIWindow
{
public:
	UIWindow(WindowID a_id, const char* a_title) : windowID(a_id), title(a_title) {}
	virtual ~UIWindow() = default;

	void TryDraw();
	bool IsOpen() const { return isOpen; }
	void SetOpen(bool a_open);
	void Toggle() { SetOpen(!isOpen); }

	WindowID GetID() const { return windowID; }
	const char* GetTitle() const { return title; }
	bool IsIndependent() const { return independent; }
	virtual bool ShouldDrawOverlay() const { return false; }

protected:
	virtual bool ShouldDraw() const { return isOpen; }
	virtual void DrawContents() = 0;
	virtual ImGuiWindowFlags GetWindowFlags() const { return ImGuiWindowFlags_None; }
	virtual ImVec2 GetDefaultSize() const { return ImVec2(800, 600); }
	virtual void OnOpen() {}
	virtual void OnClose() {}
	// Per-frame window opacity, applied uniformly to BOTH the window background
	// and its content. Pushed before Begin() so the background rect (drawn inside
	// Begin) fades together with the text — a fading overlay must never show an
	// opaque box with invisible text. Default fully opaque; overlays that fade
	// (e.g. the welcome banner) override this.
	virtual float GetWindowAlpha() const { return 1.0f; }

	WindowID windowID;
	const char* title;
	bool isOpen{ false };
	bool independent{ false };
	// Tracks the scale used for this window's current pixel dimensions. A live
	// settings change can then preserve the user's layout while resizing it by
	// exactly the ratio needed for the new combined text and DPI scale.
	float appliedUIScale{ 0.0f };
};
