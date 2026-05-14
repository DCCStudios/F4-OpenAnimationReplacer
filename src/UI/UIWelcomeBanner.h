#pragma once

#include "UI/UIWindow.h"

class UIWelcomeBanner : public UIWindow
{
public:
	UIWelcomeBanner() : UIWindow(WindowID::kWelcomeBanner, "##WelcomeBanner") {}

	void Show();

protected:
	bool ShouldDraw() const override;
	void DrawContents() override;
	ImGuiWindowFlags GetWindowFlags() const override;
	ImVec2 GetDefaultSize() const override { return ImVec2(400, 80); }
	void OnOpen() override;

private:
	static constexpr float kDisplayDuration = 8.0f;
	float displayTimer{ 0.f };
	bool active{ false };
};
