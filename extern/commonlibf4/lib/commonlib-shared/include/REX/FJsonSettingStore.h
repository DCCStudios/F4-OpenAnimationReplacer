#pragma once

#ifdef COMMONLIB_OPTION_JSON

#	include "REX/FSettingStore.h"
#	include "REX/TSingleton.h"

namespace REX
{
	class FJsonSettingStore :
		public FSettingStore,
		public TSingleton<FJsonSettingStore>
	{
	public:
		virtual void Load() override;
		virtual void Save() override;
	};
}

namespace REX::JSON
{
	using SettingStore [[deprecated("Renamed to 'REX::FJsonSettingStore")]] = FJsonSettingStore;
}

#endif
