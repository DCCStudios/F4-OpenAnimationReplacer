#pragma once

#include "UI/UIWindow.h"

class UIDebugOverlay : public UIWindow
{
public:
	UIDebugOverlay() : UIWindow(WindowID::kDebugOverlay, "Active Replacements") {
		independent = true;
	}

protected:
	void DrawContents() override;
	ImVec2 GetDefaultSize() const override { return ImVec2(750, 450); }
};
