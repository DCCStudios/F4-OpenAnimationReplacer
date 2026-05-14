#pragma once

#include <imgui.h>

enum class WindowID : int32_t
{
	kMain = 0,
	kAnimationLog,
	kAnimationEventLog,
	kWelcomeBanner,
	kAnimationQueue,
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

	WindowID windowID;
	const char* title;
	bool isOpen{ false };
	bool independent{ false };
};
