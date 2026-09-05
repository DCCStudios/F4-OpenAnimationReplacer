#pragma once

#include "UI/UIWindow.h"
#include "AnimationLog.h"

class UIAnimationLog : public UIWindow
{
public:
	UIAnimationLog() : UIWindow(WindowID::kAnimationLog, "Animation Log") {
		independent = true;
	}

protected:
	// The log collects only once a log window has been opened (perf: the
	// feed costs per activation/event on every actor). Stays on afterwards
	// so reopening shows history.
	void OnOpen() override { AnimationLog::GetSingleton()->SetEnabled(true); }
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
