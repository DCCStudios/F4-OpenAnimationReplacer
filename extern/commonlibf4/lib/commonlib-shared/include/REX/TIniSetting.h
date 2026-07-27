#pragma once

#ifdef COMMONLIB_OPTION_INI

#	include "REX/FIniSettingStore.h"
#	include "REX/TSetting.h"

namespace REX::Impl
{
	template <class T>
	void IniSettingLoad(void* a_data, std::string_view a_section, std::string_view a_key, T& a_value, T& a_valueDefault);

	template <class T>
	void IniSettingSave(void* a_data, std::string_view a_section, std::string_view a_key, T& a_value);
}

namespace REX
{
	template <class T, class S = FIniSettingStore>
	class TIniSetting :
		public TSetting<T>
	{
	public:
		TIniSetting(std::string_view a_section, std::string_view a_key, T a_default) :
			TSetting<T>(a_default),
			m_section(a_section),
			m_key(a_key)
		{
			S::GetSingleton()->Add(this);
		}

		TIniSetting(std::string_view a_key, T a_default) :
			TSetting<T>(a_default),
			m_section(),
			m_key(a_key)
		{
			S::GetSingleton()->Add(this);
		}

	public:
		virtual void Load(void* a_data, bool a_isBase) override
		{
			if (a_isBase) {
				Impl::IniSettingLoad<T>(a_data, m_section, m_key, this->m_valueDefault, this->m_valueDefault);
				this->SetValue(this->m_valueDefault);
			} else {
				Impl::IniSettingLoad<T>(a_data, m_section, m_key, this->m_value, this->m_valueDefault);
			}
		}

		virtual void Save(void* a_data) override
		{
			Impl::IniSettingSave<T>(a_data, m_section, m_key, this->m_value);
		}

	private:
		std::string_view m_section;
		std::string_view m_key;
	};
}

// DEPRECATED
namespace REX::INI
{
	template <class T, class S = FIniSettingStore>
	using Setting [[deprecated("Renamed to 'REX::TIniSetting'")]] = TIniSetting<T, S>;

	template <class S = FIniSettingStore>
	using Bool = TIniSetting<bool, S>;

	template <class S = FIniSettingStore>
	using F32 = TIniSetting<float, S>;

	template <class S = FIniSettingStore>
	using F64 = TIniSetting<double, S>;

	template <class S = FIniSettingStore>
	using I8 = TIniSetting<std::int8_t, S>;

	template <class S = FIniSettingStore>
	using I16 = TIniSetting<std::int16_t, S>;

	template <class S = FIniSettingStore>
	using I32 = TIniSetting<std::int32_t, S>;

	template <class S = FIniSettingStore>
	using U8 = TIniSetting<std::uint8_t, S>;

	template <class S = FIniSettingStore>
	using U16 = TIniSetting<std::uint16_t, S>;

	template <class S = FIniSettingStore>
	using U32 = TIniSetting<std::uint32_t, S>;

	template <class S = FIniSettingStore>
	using Str = TIniSetting<std::string, S>;
}

#endif
