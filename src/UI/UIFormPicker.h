#pragma once

#include <string>
#include <vector>

namespace UIFormPicker
{
	bool DrawFormPicker(
		const char* a_label,
		std::string& a_pluginName,
		uint32_t& a_localFormID,
		RE::ENUM_FORM_ID a_formType,
		bool& a_dirty);

	// Multi-type variant: the form dropdown lists the union of the given
	// types (item-style conditions: weapons + armor + ammo + ...).
	bool DrawFormPicker(
		const char* a_label,
		std::string& a_pluginName,
		uint32_t& a_localFormID,
		const std::vector<RE::ENUM_FORM_ID>& a_formTypes,
		bool& a_dirty);

	bool DrawKeywordPicker(
		const char* a_label,
		std::string& a_editorID,
		bool& a_dirty);
}
