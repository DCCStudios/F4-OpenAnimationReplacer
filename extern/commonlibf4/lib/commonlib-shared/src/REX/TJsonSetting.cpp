#ifdef COMMONLIB_OPTION_JSON

#	include "REX/TJsonSetting.h"

#	include <glaze/glaze.hpp>

namespace REX::Impl
{
	template <class T>
	void JsonSettingLoadEx(void* a_data, std::string_view a_path, T& a_value, T& a_valueDefault)
	{
		const auto& json = *static_cast<glz::generic*>(a_data);
		if (a_path[0] != '/') {
			const auto path = std::format("/{}"sv, a_path);
			a_value = glz::get<T>(json, path).value_or(a_valueDefault);
		} else {
			a_value = glz::get<T>(json, a_path).value_or(a_valueDefault);
		}
	}

	template <>
	void JsonSettingLoad<bool>(void* a_data, std::string_view a_path, bool& a_value, bool& a_valueDefault)
	{
		JsonSettingLoadEx<bool>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<double>(void* a_data, std::string_view a_path, double& a_value, double& a_valueDefault)
	{
		JsonSettingLoadEx<double>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::uint8_t>(void* a_data, std::string_view a_path, std::uint8_t& a_value, std::uint8_t& a_valueDefault)
	{
		JsonSettingLoadEx<std::uint8_t>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::uint16_t>(void* a_data, std::string_view a_path, std::uint16_t& a_value, std::uint16_t& a_valueDefault)
	{
		JsonSettingLoadEx<std::uint16_t>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::uint32_t>(void* a_data, std::string_view a_path, std::uint32_t& a_value, std::uint32_t& a_valueDefault)
	{
		JsonSettingLoadEx<std::uint32_t>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::int8_t>(void* a_data, std::string_view a_path, std::int8_t& a_value, std::int8_t& a_valueDefault)
	{
		JsonSettingLoadEx<std::int8_t>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::int16_t>(void* a_data, std::string_view a_path, std::int16_t& a_value, std::int16_t& a_valueDefault)
	{
		JsonSettingLoadEx<std::int16_t>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::int32_t>(void* a_data, std::string_view a_path, std::int32_t& a_value, std::int32_t& a_valueDefault)
	{
		JsonSettingLoadEx<std::int32_t>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::string>(void* a_data, std::string_view a_path, std::string& a_value, std::string& a_valueDefault)
	{
		JsonSettingLoadEx<std::string>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::vector<bool>>(void* a_data, std::string_view a_path, std::vector<bool>& a_value, std::vector<bool>& a_valueDefault)
	{
		JsonSettingLoadEx<std::vector<bool>>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::vector<double>>(void* a_data, std::string_view a_path, std::vector<double>& a_value, std::vector<double>& a_valueDefault)
	{
		JsonSettingLoadEx<std::vector<double>>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::vector<std::uint8_t>>(void* a_data, std::string_view a_path, std::vector<std::uint8_t>& a_value, std::vector<std::uint8_t>& a_valueDefault)
	{
		JsonSettingLoadEx<std::vector<std::uint8_t>>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::vector<std::uint16_t>>(void* a_data, std::string_view a_path, std::vector<std::uint16_t>& a_value, std::vector<std::uint16_t>& a_valueDefault)
	{
		JsonSettingLoadEx<std::vector<std::uint16_t>>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::vector<std::uint32_t>>(void* a_data, std::string_view a_path, std::vector<std::uint32_t>& a_value, std::vector<std::uint32_t>& a_valueDefault)
	{
		JsonSettingLoadEx<std::vector<std::uint32_t>>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::vector<std::int8_t>>(void* a_data, std::string_view a_path, std::vector<std::int8_t>& a_value, std::vector<std::int8_t>& a_valueDefault)
	{
		JsonSettingLoadEx<std::vector<std::int8_t>>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::vector<std::int16_t>>(void* a_data, std::string_view a_path, std::vector<std::int16_t>& a_value, std::vector<std::int16_t>& a_valueDefault)
	{
		JsonSettingLoadEx<std::vector<std::int16_t>>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::vector<std::int32_t>>(void* a_data, std::string_view a_path, std::vector<std::int32_t>& a_value, std::vector<std::int32_t>& a_valueDefault)
	{
		JsonSettingLoadEx<std::vector<std::int32_t>>(a_data, a_path, a_value, a_valueDefault);
	}

	template <>
	void JsonSettingLoad<std::vector<std::string>>(void* a_data, std::string_view a_path, std::vector<std::string>& a_value, std::vector<std::string>& a_valueDefault)
	{
		JsonSettingLoadEx<std::vector<std::string>>(a_data, a_path, a_value, a_valueDefault);
	}

	template <class T>
	void JsonSettingSaveEx(void* a_data, std::string_view a_path, T& a_value)
	{
		auto& json = *static_cast<glz::generic*>(a_data);
		if (a_path[0] != '/') {
			const auto path = std::format("/{}"sv, a_path);
			glz::set(json, path, a_value);
		} else {
			glz::set(json, a_path, a_value);
		}
	}

	template <>
	void JsonSettingSave<bool>(void* a_data, std::string_view a_path, bool& a_value)
	{
		JsonSettingSaveEx<bool>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<double>(void* a_data, std::string_view a_path, double& a_value)
	{
		JsonSettingSaveEx<double>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::uint8_t>(void* a_data, std::string_view a_path, std::uint8_t& a_value)
	{
		JsonSettingSaveEx<std::uint8_t>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::uint16_t>(void* a_data, std::string_view a_path, std::uint16_t& a_value)
	{
		JsonSettingSaveEx<std::uint16_t>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::uint32_t>(void* a_data, std::string_view a_path, std::uint32_t& a_value)
	{
		JsonSettingSaveEx<std::uint32_t>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::int8_t>(void* a_data, std::string_view a_path, std::int8_t& a_value)
	{
		JsonSettingSaveEx<std::int8_t>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::int16_t>(void* a_data, std::string_view a_path, std::int16_t& a_value)
	{
		JsonSettingSaveEx<std::int16_t>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::int32_t>(void* a_data, std::string_view a_path, std::int32_t& a_value)
	{
		JsonSettingSaveEx<std::int32_t>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::string>(void* a_data, std::string_view a_path, std::string& a_value)
	{
		JsonSettingSaveEx<std::string>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::vector<bool>>(void* a_data, std::string_view a_path, std::vector<bool>& a_value)
	{
		JsonSettingSaveEx<std::vector<bool>>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::vector<double>>(void* a_data, std::string_view a_path, std::vector<double>& a_value)
	{
		JsonSettingSaveEx<std::vector<double>>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::vector<std::uint8_t>>(void* a_data, std::string_view a_path, std::vector<std::uint8_t>& a_value)
	{
		JsonSettingSaveEx<std::vector<std::uint8_t>>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::vector<std::uint16_t>>(void* a_data, std::string_view a_path, std::vector<std::uint16_t>& a_value)
	{
		JsonSettingSaveEx<std::vector<std::uint16_t>>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::vector<std::uint32_t>>(void* a_data, std::string_view a_path, std::vector<std::uint32_t>& a_value)
	{
		JsonSettingSaveEx<std::vector<std::uint32_t>>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::vector<std::int8_t>>(void* a_data, std::string_view a_path, std::vector<std::int8_t>& a_value)
	{
		JsonSettingSaveEx<std::vector<std::int8_t>>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::vector<std::int16_t>>(void* a_data, std::string_view a_path, std::vector<std::int16_t>& a_value)
	{
		JsonSettingSaveEx<std::vector<std::int16_t>>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::vector<std::int32_t>>(void* a_data, std::string_view a_path, std::vector<std::int32_t>& a_value)
	{
		JsonSettingSaveEx<std::vector<std::int32_t>>(a_data, a_path, a_value);
	}

	template <>
	void JsonSettingSave<std::vector<std::string>>(void* a_data, std::string_view a_path, std::vector<std::string>& a_value)
	{
		JsonSettingSaveEx<std::vector<std::string>>(a_data, a_path, a_value);
	}
}

#endif
