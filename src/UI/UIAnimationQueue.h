#pragma once

#include "UI/UIWindow.h"

class UIAnimationQueue : public UIWindow
{
public:
	UIAnimationQueue() : UIWindow(WindowID::kAnimationQueue, "##AnimationQueue")
	{
		independent = true;
	}

	bool ShouldDrawOverlay() const override { return ShouldDraw(); }

protected:
	bool ShouldDraw() const override;
	void DrawContents() override;
	ImGuiWindowFlags GetWindowFlags() const override;
	ImVec2 GetDefaultSize() const override { return ImVec2(240, 60); }

private:
	static constexpr float kFadeTime = 1.0f;
	mutable float lingerTimer{ 0.f };
};
