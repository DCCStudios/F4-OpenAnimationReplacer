#ifdef COMMONLIB_OPTION_TOML

#	include "REX/TTomlSetting.h"

#	include <toml.hpp>

toml::value* toml_recurse_table(toml::value* a_result, toml::value& a_value, const std::string& a_section, bool a_create)
{
	if (a_result && a_result->is_table()) {
		for (auto& value : a_result->as_table()) {
			if (value.first == a_section) {
				return std::addressof(value.second);
			}
		}
		if (a_create) {
			(*a_result)[a_section] = toml::table{};
			return std::addressof((*a_result)[a_section]);
		}
	} else if (a_value.is_table()) {
		for (auto& value : a_value.as_table()) {
			if (value.first == a_section) {
				return std::addressof(value.second);
			}
		}
		if (a_create) {
			a_value[a_section] = toml::table{};
			return std::addressof(a_value[a_section]);
		}
	}
	return a_result;
}

namespace REX::Impl
{
	template <class T>
	void TomlSettingLoadEx(void* a_data, std::vector<std::string> a_section, std::string_view a_key, T& a_value, T& a_valueDefault)
	{
		const auto& data = static_cast<toml::value*>(a_data);
		if (a_section.empty()) {
			auto& path = (*data);
			a_value = toml::find_or<T>(path, a_key.data(), a_valueDefault);
			return;
		} else if (a_section.size() == 1) {
			auto& path = (*data)[a_section.front()];
			a_value = toml::find_or<T>(path, a_key.data(), a_valueDefault);
			return;
		} else {
			toml::value* path{ nullptr };
			for (auto& section : a_section) {
				path = toml_recurse_table(path, *data, section, false);
			}
			if (path) {
				a_value = toml::find_or<T>(*path, a_key.data(), a_valueDefault);
				return;
			}
		}
		a_value = a_valueDefault;
	}

	template <>
	void TomlSettingLoad<bool>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, bool& a_value, bool& a_valueDefault)
	{
		TomlSettingLoadEx<bool>(a_data, a_section, a_key, a_value, a_valueDefault);
	}

	template <>
	void TomlSettingLoad<float>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, float& a_value, float& a_valueDefault)
	{
		TomlSettingLoadEx<float>(a_data, a_section, a_key, a_value, a_valueDefault);
	}

	template <>
	void TomlSettingLoad<double>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, double& a_value, double& a_valueDefault)
	{
		TomlSettingLoadEx<double>(a_data, a_section, a_key, a_value, a_valueDefault);
	}

	template <>
	void TomlSettingLoad<std::uint8_t>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, std::uint8_t& a_value, std::uint8_t& a_valueDefault)
	{
		TomlSettingLoadEx<std::uint8_t>(a_data, a_section, a_key, a_value, a_valueDefault);
	}

	template <>
	void TomlSettingLoad<std::uint16_t>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, std::uint16_t& a_value, std::uint16_t& a_valueDefault)
	{
		TomlSettingLoadEx<std::uint16_t>(a_data, a_section, a_key, a_value, a_valueDefault);
	}

	template <>
	void TomlSettingLoad<std::uint32_t>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, std::uint32_t& a_value, std::uint32_t& a_valueDefault)
	{
		TomlSettingLoadEx<std::uint32_t>(a_data, a_section, a_key, a_value, a_valueDefault);
	}

	template <>
	void TomlSettingLoad<std::int8_t>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, std::int8_t& a_value, std::int8_t& a_valueDefault)
	{
		TomlSettingLoadEx<std::int8_t>(a_data, a_section, a_key, a_value, a_valueDefault);
	}

	template <>
	void TomlSettingLoad<std::int16_t>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, std::int16_t& a_value, std::int16_t& a_valueDefault)
	{
		TomlSettingLoadEx<std::int16_t>(a_data, a_section, a_key, a_value, a_valueDefault);
	}

	template <>
	void TomlSettingLoad<std::int32_t>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, std::int32_t& a_value, std::int32_t& a_valueDefault)
	{
		TomlSettingLoadEx<std::int32_t>(a_data, a_section, a_key, a_value, a_valueDefault);
	}

	template <>
	void TomlSettingLoad<std::string>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, std::string& a_value, std::string& a_valueDefault)
	{
		TomlSettingLoadEx<std::string>(a_data, a_section, a_key, a_value, a_valueDefault);
	}

	template <class T>
	void TomlSettingSaveEx(void* a_data, std::vector<std::string> a_section, std::string_view a_key, T& a_value)
	{
		auto& data = *static_cast<toml::value*>(a_data);
		if (a_section.empty()) {
			data[a_key.data()] = a_value;
		} else if (a_section.size() == 1) {
			data[a_section.front()][a_key.data()] = a_value;
		} else {
			toml::value* path{ nullptr };
			for (auto& section : a_section) {
				path = toml_recurse_table(path, data, section, true);
			}
			if (path) {
				(*path)[a_key.data()] = a_value;
			}
		}
	}

	template <>
	void TomlSettingSave<bool>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, bool& a_value)
	{
		TomlSettingSaveEx<bool>(a_data, a_section, a_key, a_value);
	}

	template <>
	void TomlSettingSave<float>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, float& a_value)
	{
		TomlSettingSaveEx<float>(a_data, a_section, a_key, a_value);
	}

	template <>
	void TomlSettingSave<double>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, double& a_value)
	{
		TomlSettingSaveEx<double>(a_data, a_section, a_key, a_value);
	}

	template <>
	void TomlSettingSave<std::uint8_t>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, std::uint8_t& a_value)
	{
		TomlSettingSaveEx<std::uint8_t>(a_data, a_section, a_key, a_value);
	}

	template <>
	void TomlSettingSave<std::uint16_t>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, std::uint16_t& a_value)
	{
		TomlSettingSaveEx<std::uint16_t>(a_data, a_section, a_key, a_value);
	}

	template <>
	void TomlSettingSave<std::uint32_t>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, std::uint32_t& a_value)
	{
		TomlSettingSaveEx<std::uint32_t>(a_data, a_section, a_key, a_value);
	}

	template <>
	void TomlSettingSave<std::int8_t>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, std::int8_t& a_value)
	{
		TomlSettingSaveEx<std::int8_t>(a_data, a_section, a_key, a_value);
	}

	template <>
	void TomlSettingSave<std::int16_t>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, std::int16_t& a_value)
	{
		TomlSettingSaveEx<std::int16_t>(a_data, a_section, a_key, a_value);
	}

	template <>
	void TomlSettingSave<std::int32_t>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, std::int32_t& a_value)
	{
		TomlSettingSaveEx<std::int32_t>(a_data, a_section, a_key, a_value);
	}

	template <>
	void TomlSettingSave<std::string>(void* a_data, std::vector<std::string> a_section, std::string_view a_key, std::string& a_value)
	{
		TomlSettingSaveEx<std::string>(a_data, a_section, a_key, a_value);
	}
}
#endif
