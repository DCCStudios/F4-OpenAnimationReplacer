#ifdef COMMONLIB_OPTION_JSON

#	include "REX/FJsonSettingStore.h"
#	include "REX/ISetting.h"

#	include <glaze/glaze.hpp>

namespace REX
{
	void FJsonSettingStore::Load()
	{
		if (std::filesystem::exists(m_fileBase)) {
			glz::generic result{};
			if (!glz::read_file_json(result, m_fileBase, std::string{})) {
				for (auto setting : m_settings) {
					setting->Load(&result, true);
				}
			}
		}

		if (std::filesystem::exists(m_fileUser)) {
			glz::generic result{};
			if (!glz::read_file_json(result, m_fileUser, std::string{})) {
				for (auto setting : m_settings) {
					setting->Load(&result, false);
				}
			}
		}
	}

	struct save_opts : glz::opts
	{
		static constexpr bool prettify = true;
		std::uint8_t          indentation_width = 4;
	};

	void FJsonSettingStore::Save()
	{
		glz::generic output{};
		if (std::filesystem::exists(m_fileBase)) {
			(void)glz::read_file_json(output, m_fileBase, std::string{});
		}

		for (auto& setting : m_settings) {
			setting->Save(&output);
		}

		(void)glz::write_file_json<save_opts{}>(output, m_fileBase, std::string{});
	}
}

#endif
