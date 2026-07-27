#pragma once

namespace RE
{
	enum class QuestFlag : std::int32_t
	{
		kEnabled = 0x1,
		kCompleted = 0x2,
		kAddIdleToHello = 0x4,
		kAllowRepeatStages = 0x8,
		kStartsEnabled = 0x10,
		kDisplayedInHUD = 0x20,
		kFailed = 0x40,
		kStageWait = 0x80,
		kRunOnce = 0x100,
		kExcludedFromExport = 0x200,
		kWarnOnAliasFillFailure = 0x400,
		kActive = 0x800,
		kRepeatsConditions = 0x1000,
		kKeepInstance = 0x2000,
		kWantDormant = 0x4000,
		kHasDialogueData = 0x8000,
		kNonSaveFlags = 0x970C
	};
}
