#ifdef COMMONLIB_OPTION_INI

#	include "REX/TIniSetting.h"

#	include <SimpleIni.h>
#	undef ERROR

namespace REX::Impl
{
	template <>
	void IniSettingLoad<bool>(void* a_data, std::string_view a_section, std::string_view a_key, bool& a_value, bool& a_valueDefault)
	{
		const auto data = static_cast<CSimpleIniA*>(a_data);
		a_value = data->GetBoolValue(a_section.data(), a_key.data(), a_valueDefault);
	}

	template <>
	void IniSettingLoad<float>(void* a_data, std::string_view a_section, std::string_view a_key, float& a_value, float& a_valueDefault)
	{
		const auto data = static_cast<CSimpleIniA*>(a_data);
		a_value = static_cast<float>(data->GetDoubleValue(a_section.data(), a_key.data(), a_valueDefault));
	}

	template <>
	void IniSettingLoad<double>(void* a_data, std::string_view a_section, std::string_view a_key, double& a_value, double& a_valueDefault)
	{
		const auto data = static_cast<CSimpleIniA*>(a_data);
		a_value = data->GetDoubleValue(a_section.data(), a_key.data(), a_valueDefault);
	}

	template <>
	void IniSettingLoad<std::uint8_t>(void* a_data, std::string_view a_section, std::string_view a_key, std::uint8_t& a_value, std::uint8_t& a_valueDefault)
	{
		const auto data = static_cast<CSimpleIniA*>(a_data);
		a_value = static_cast<std::uint8_t>(data->GetLongValue(a_section.data(), a_key.data(), a_valueDefault));
	}

	template <>
	void IniSettingLoad<std::uint16_t>(void* a_data, std::string_view a_section, std::string_view a_key, std::uint16_t& a_value, std::uint16_t& a_valueDefault)
	{
		const auto data = static_cast<CSimpleIniA*>(a_data);
		a_value = static_cast<std::uint16_t>(data->GetLongValue(a_section.data(), a_key.data(), a_valueDefault));
	}

	template <>
	void IniSettingLoad<std::uint32_t>(void* a_data, std::string_view a_section, std::string_view a_key, std::uint32_t& a_value, std::uint32_t& a_valueDefault)
	{
		const auto data = static_cast<CSimpleIniA*>(a_data);
		a_value = static_cast<std::uint32_t>(data->GetLongValue(a_section.data(), a_key.data(), a_valueDefault));
	}

	template <>
	void IniSettingLoad<std::int8_t>(void* a_data, std::string_view a_section, std::string_view a_key, std::int8_t& a_value, std::int8_t& a_valueDefault)
	{
		const auto data = static_cast<CSimpleIniA*>(a_data);
		a_value = static_cast<std::int8_t>(data->GetLongValue(a_section.data(), a_key.data(), a_valueDefault));
	}

	template <>
	void IniSettingLoad<std::int16_t>(void* a_data, std::string_view a_section, std::string_view a_key, std::int16_t& a_value, std::int16_t& a_valueDefault)
	{
		const auto data = static_cast<CSimpleIniA*>(a_data);
		a_value = static_cast<std::int16_t>(data->GetLongValue(a_section.data(), a_key.data(), a_valueDefault));
	}

	template <>
	void IniSettingLoad<std::int32_t>(void* a_data, std::string_view a_section, std::string_view a_key, std::int32_t& a_value, std::int32_t& a_valueDefault)
	{
		const auto data = static_cast<CSimpleIniA*>(a_data);
		a_value = static_cast<std::int32_t>(data->GetLongValue(a_section.data(), a_key.data(), a_valueDefault));
	}

	template <>
	void IniSettingLoad<std::string>(void* a_data, std::string_view a_section, std::string_view a_key, std::string& a_value, std::string& a_valueDefault)
	{
		const auto data = static_cast<CSimpleIniA*>(a_data);
		a_value = data->GetValue(a_section.data(), a_key.data(), a_valueDefault.c_str());
	}

	template <>
	void IniSettingSave<bool>(void* a_data, std::string_view a_section, std::string_view a_key, bool& a_value)
	{
		auto& data = *static_cast<CSimpleIniA*>(a_data);
		data.SetBoolValue(a_section.data(), a_key.data(), a_value);
	}

	template <>
	void IniSettingSave<float>(void* a_data, std::string_view a_section, std::string_view a_key, float& a_value)
	{
		auto& data = *static_cast<CSimpleIniA*>(a_data);
		data.SetDoubleValue(a_section.data(), a_key.data(), a_value);
	}

	template <>
	void IniSettingSave<double>(void* a_data, std::string_view a_section, std::string_view a_key, double& a_value)
	{
		auto& data = *static_cast<CSimpleIniA*>(a_data);
		data.SetDoubleValue(a_section.data(), a_key.data(), a_value);
	}

	template <>
	void IniSettingSave<std::uint8_t>(void* a_data, std::string_view a_section, std::string_view a_key, std::uint8_t& a_value)
	{
		auto& data = *static_cast<CSimpleIniA*>(a_data);
		data.SetLongValue(a_section.data(), a_key.data(), a_value);
	}

	template <>
	void IniSettingSave<std::uint16_t>(void* a_data, std::string_view a_section, std::string_view a_key, std::uint16_t& a_value)
	{
		auto& data = *static_cast<CSimpleIniA*>(a_data);
		data.SetLongValue(a_section.data(), a_key.data(), a_value);
	}

	template <>
	void IniSettingSave<std::uint32_t>(void* a_data, std::string_view a_section, std::string_view a_key, std::uint32_t& a_value)
	{
		auto& data = *static_cast<CSimpleIniA*>(a_data);
		data.SetLongValue(a_section.data(), a_key.data(), a_value);
	}

	template <>
	void IniSettingSave<std::int8_t>(void* a_data, std::string_view a_section, std::string_view a_key, std::int8_t& a_value)
	{
		auto& data = *static_cast<CSimpleIniA*>(a_data);
		data.SetLongValue(a_section.data(), a_key.data(), a_value);
	}

	template <>
	void IniSettingSave<std::int16_t>(void* a_data, std::string_view a_section, std::string_view a_key, std::int16_t& a_value)
	{
		auto& data = *static_cast<CSimpleIniA*>(a_data);
		data.SetLongValue(a_section.data(), a_key.data(), a_value);
	}

	template <>
	void IniSettingSave<std::int32_t>(void* a_data, std::string_view a_section, std::string_view a_key, std::int32_t& a_value)
	{
		auto& data = *static_cast<CSimpleIniA*>(a_data);
		data.SetLongValue(a_section.data(), a_key.data(), a_value);
	}

	template <>
	void IniSettingSave<std::string>(void* a_data, std::string_view a_section, std::string_view a_key, std::string& a_value)
	{
		auto& data = *static_cast<CSimpleIniA*>(a_data);
		data.SetValue(a_section.data(), a_key.data(), a_value.c_str());
	}
}
#endif
