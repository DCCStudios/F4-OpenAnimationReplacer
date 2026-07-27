#pragma once

#ifdef COMMONLIB_OPTION_INI

#	include "REX/FSettingStore.h"
#	include "REX/TSingleton.h"

namespace REX
{
	class FIniSettingStore :
		public FSettingStore,
		public TSingleton<FIniSettingStore>
	{
	public:
		virtual void Load() override;
		virtual void Save() override;
	};
}

namespace REX::INI
{
	using SettingStore [[deprecated("Renamed to 'REX::FIniSettingStore'")]] = FIniSettingStore;
}

#endif
