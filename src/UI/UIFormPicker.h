#pragma once

#include <string>
#include <vector>
#include "RE/Bethesda/TESForms.h"

namespace UIFormPicker
{
	bool DrawFormPicker(
		const char* a_label,
		std::string& a_pluginName,
		uint32_t& a_localFormID,
		RE::ENUM_FORM_ID a_formType,
		bool& a_dirty);

	bool DrawKeywordPicker(
		const char* a_label,
		std::string& a_editorID,
		bool& a_dirty);
}
