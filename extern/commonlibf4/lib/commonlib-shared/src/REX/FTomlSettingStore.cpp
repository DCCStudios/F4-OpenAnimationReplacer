#ifdef COMMONLIB_OPTION_TOML

#	include "REX/FTomlSettingStore.h"
#	include "REX/ISetting.h"

#	include <toml.hpp>

bool toml_recurse_mark_implicit(toml::value& a_value)
{
	for (auto& kv : a_value.as_table()) {
		if (kv.second.is_table()) {
			if (!toml_recurse_mark_implicit(kv.second)) {
				continue;
			}
			kv.second.as_table_fmt().fmt = toml::table_format::implicit;
		} else {
			return false;
		}
	}
	return true;
}

namespace REX
{
	void FTomlSettingStore::Load()
	{
		if (auto result = toml::try_parse(m_fileBase.data()); result.is_ok()) {
			for (auto& setting : m_settings) {
				setting->Load(&result.unwrap(), true);
			}
		}

		if (auto result = toml::try_parse(m_fileUser.data()); result.is_ok()) {
			for (auto& setting : m_settings) {
				setting->Load(&result.unwrap(), false);
			}
		}
	}

	void FTomlSettingStore::Save()
	{
		toml::value output{};
		if (auto result = toml::try_parse(m_fileBase.data()); result.is_ok()) {
			output = result.unwrap();
		}

		for (auto setting : m_settings) {
			setting->Save(&output);
		}

		toml_recurse_mark_implicit(output);
		std::ofstream file{ m_fileBase.data(), std::ios::trunc };
		file << toml::format(output);
	}
}

#endif
