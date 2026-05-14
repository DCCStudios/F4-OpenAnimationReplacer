#pragma once

#include "UI/UIWindow.h"

class UIAnimationEventLog : public UIWindow
{
public:
	UIAnimationEventLog() : UIWindow(WindowID::kAnimationEventLog, "Animation Event Log") {}

protected:
	void DrawContents() override;
	ImVec2 GetDefaultSize() const override { return ImVec2(500, 350); }

private:
	char filterText[128]{};
};
