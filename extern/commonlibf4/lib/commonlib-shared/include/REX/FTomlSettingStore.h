#pragma once

#ifdef COMMONLIB_OPTION_TOML

#	include "REX/FSettingStore.h"
#	include "REX/TSingleton.h"

namespace REX
{
	class FTomlSettingStore :
		public FSettingStore,
		public TSingleton<FTomlSettingStore>
	{
	public:
		virtual void Load() override;
		virtual void Save() override;
	};
}

namespace REX::TOML
{
	using SettingStore [[deprecated("Renamed to 'REX::FTomlSettingStore")]] = FTomlSettingStore;
}

#endif
