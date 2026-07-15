#pragma once

#include "UI/UIWindow.h"

class UIAnimationLog : public UIWindow
{
public:
	UIAnimationLog() : UIWindow(WindowID::kAnimationLog, "Animation Log") {
		independent = true;
	}

protected:
	void DrawContents() override;
	ImVec2 GetDefaultSize() const override { return ImVec2(700, 400); }

private:
	bool showActivate{ true };
	bool showReplace{ true };
	bool showLoop{ true };
	bool showEcho{ true };
	bool showFirstPerson{ true };
	bool showThirdPerson{ true };
	bool showOnlyConsoleTarget{ false };
	bool scrollToBottom{ false };
	uint32_t targetFormID{ 0 };
	char filterText[128]{};
	char targetFormIDBuf[32]{};

	uint32_t GetConsoleTargetFormID() const;
};
