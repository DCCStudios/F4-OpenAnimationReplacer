#pragma once

#ifdef COMMONLIB_OPTION_JSON

#	include "REX/FJsonSettingStore.h"
#	include "REX/TSetting.h"

namespace REX::Impl
{
	template <class T>
	void JsonSettingLoad(void* a_data, std::string_view a_path, T& a_value, T& a_valueDefault);

	template <class T>
	void JsonSettingSave(void* a_data, std::string_view a_path, T& a_value);
}

namespace REX
{
	template <class T, class S = FJsonSettingStore>
	class TJsonSetting :
		public TSetting<T>
	{
	public:
		TJsonSetting(std::string_view a_path, T a_default) :
			TSetting<T>(a_default),
			m_path(a_path)
		{
			S::GetSingleton()->Add(this);
		}

	public:
		virtual void Load(void* a_data, bool a_isBase) override
		{
			if (a_isBase) {
				Impl::JsonSettingLoad<T>(a_data, m_path, this->m_valueDefault, this->m_valueDefault);
				this->SetValue(this->m_valueDefault);
			} else {
				Impl::JsonSettingLoad<T>(a_data, m_path, this->m_value, this->m_valueDefault);
			}
		}

		virtual void Save(void* a_data) override
		{
			Impl::JsonSettingSave<T>(a_data, m_path, this->m_value);
		}

	private:
		std::string_view m_path;
	};
}

// DEPRECATED
namespace REX::JSON
{
	template <class T, class S = FJsonSettingStore>
	using Setting [[deprecated("Renamed to 'REX::TJsonSetting")]] = TJsonSetting<T, S>;

	template <class S = FJsonSettingStore>
	using Bool = TJsonSetting<bool, S>;

	//template <class S = FJsonSettingStore>
	//using F32 = TJsonSetting<float, S>;

	template <class S = FJsonSettingStore>
	using F64 = TJsonSetting<double, S>;

	template <class S = FJsonSettingStore>
	using I8 = TJsonSetting<std::int8_t, S>;

	template <class S = FJsonSettingStore>
	using I16 = TJsonSetting<std::int16_t, S>;

	template <class S = FJsonSettingStore>
	using I32 = TJsonSetting<std::int32_t, S>;

	template <class S = FJsonSettingStore>
	using U8 = TJsonSetting<std::uint8_t, S>;

	template <class S = FJsonSettingStore>
	using U16 = TJsonSetting<std::uint16_t, S>;

	template <class S = FJsonSettingStore>
	using U32 = TJsonSetting<std::uint32_t, S>;

	template <class S = FJsonSettingStore>
	using Str = TJsonSetting<std::string, S>;

	template <class T, class S = FJsonSettingStore>
	using SettingA = TJsonSetting<std::vector<T>, S>;

	template <class S = FJsonSettingStore>
	using BoolA = SettingA<bool, S>;

	//template <class S = FJsonSettingStore>
	//using F32A = SettingA<float, S>;

	template <class S = FJsonSettingStore>
	using F64A = SettingA<double, S>;

	template <class S = FJsonSettingStore>
	using I8A = SettingA<std::int8_t, S>;

	template <class S = FJsonSettingStore>
	using I16A = SettingA<std::int16_t, S>;

	template <class S = FJsonSettingStore>
	using I32A = SettingA<std::int32_t, S>;

	template <class S = FJsonSettingStore>
	using U8A = SettingA<std::uint8_t, S>;

	template <class S = FJsonSettingStore>
	using U16A = SettingA<std::uint16_t, S>;

	template <class S = FJsonSettingStore>
	using U32A = SettingA<std::uint32_t, S>;

	template <class S = FJsonSettingStore>
	using StrA = SettingA<std::string, S>;
}

#endif
