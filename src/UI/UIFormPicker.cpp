#include "UIFormPicker.h"
#include "FormRegistry.h"
#include <imgui.h>
#include <algorithm>

namespace UIFormPicker
{
	static bool FilteredCombo(
		const char* a_label,
		const char* a_preview,
		const std::vector<std::string>& a_items,
		int& a_selectedIdx,
		char* a_filterBuf,
		size_t a_filterBufSize)
	{
		bool changed = false;
		if (ImGui::BeginCombo(a_label, a_preview)) {
			ImGui::SetNextItemWidth(-1);
			ImGui::InputText("##filter", a_filterBuf, a_filterBufSize);

			std::string filter(a_filterBuf);
			std::ranges::transform(filter, filter.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			for (int i = 0; i < static_cast<int>(a_items.size()); ++i) {
				if (!filter.empty()) {
					std::string lower = a_items[i];
					std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					if (lower.find(filter) == std::string::npos) continue;
				}

				bool selected = (i == a_selectedIdx);
				if (ImGui::Selectable(a_items[i].c_str(), selected)) {
					a_selectedIdx = i;
					changed = true;
				}
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	bool DrawFormPicker(
		const char* a_label,
		std::string& a_pluginName,
		uint32_t& a_localFormID,
		RE::ENUM_FORM_ID a_formType,
		bool& a_dirty)
	{
		bool changed = false;
		auto* registry = FormRegistry::GetSingleton();
		auto plugins = registry->GetLoadedPlugins();

		ImGui::PushID(a_label);

		// Plugin dropdown
		int pluginIdx = -1;
		for (int i = 0; i < static_cast<int>(plugins.size()); ++i) {
			if (plugins[i] == a_pluginName) { pluginIdx = i; break; }
		}

		const char* pluginPreview = pluginIdx >= 0 ? plugins[pluginIdx].c_str() : a_pluginName.c_str();

		static std::unordered_map<std::string, std::array<char, 128>> s_pluginFilters;
		auto& pluginFilter = s_pluginFilters[a_label];

		ImGui::SetNextItemWidth(200);
		if (FilteredCombo("Plugin", pluginPreview, plugins, pluginIdx, pluginFilter.data(), pluginFilter.size())) {
			if (pluginIdx >= 0 && pluginIdx < static_cast<int>(plugins.size())) {
				a_pluginName = plugins[pluginIdx];
				a_dirty = true;
				changed = true;
			}
		}

		// Form dropdown (only if plugin is selected and form type is valid)
		if (!a_pluginName.empty() && static_cast<uint8_t>(a_formType) != static_cast<uint8_t>(RE::ENUM_FORM_ID::kNONE)) {
			auto& forms = registry->GetFormsForPlugin(a_pluginName, a_formType);

			if (!forms.empty()) {
				std::vector<std::string> displayStrings;
				displayStrings.reserve(forms.size());
				for (auto& f : forms) displayStrings.push_back(f.displayString);

				int formIdx = -1;
				for (int i = 0; i < static_cast<int>(forms.size()); ++i) {
					if (forms[i].localFormID == a_localFormID) { formIdx = i; break; }
				}

				std::string formPreview = formIdx >= 0 ? forms[formIdx].displayString : std::format("0x{:X}", a_localFormID);

				static std::unordered_map<std::string, std::array<char, 128>> s_formFilters;
				auto& formFilter = s_formFilters[a_label];

				ImGui::SameLine();
				ImGui::SetNextItemWidth(300);
				if (FilteredCombo("Form", formPreview.c_str(), displayStrings, formIdx, formFilter.data(), formFilter.size())) {
					if (formIdx >= 0 && formIdx < static_cast<int>(forms.size())) {
						a_localFormID = forms[formIdx].localFormID;
						a_dirty = true;
						changed = true;
					}
				}
			}
		}

		// Manual fallback inputs
		ImGui::SameLine();
		char plugBuf[256]{};
		strncpy_s(plugBuf, a_pluginName.c_str(), sizeof(plugBuf) - 1);
		ImGui::SetNextItemWidth(150);
		if (ImGui::InputText("Plugin##manual", plugBuf, sizeof(plugBuf))) {
			a_pluginName = plugBuf;
			a_dirty = true;
			changed = true;
		}

		ImGui::SameLine();
		char formBuf[32]{};
		snprintf(formBuf, sizeof(formBuf), "0x%X", a_localFormID);
		ImGui::SetNextItemWidth(90);
		if (ImGui::InputText("ID##manual", formBuf, sizeof(formBuf))) {
			try { a_localFormID = std::stoul(formBuf, nullptr, 16); } catch (...) {}
			a_dirty = true;
			changed = true;
		}

		ImGui::PopID();
		return changed;
	}

	bool DrawKeywordPicker(
		const char* a_label,
		std::string& a_editorID,
		bool& a_dirty)
	{
		bool changed = false;
		auto* registry = FormRegistry::GetSingleton();
		auto plugins = registry->GetLoadedPlugins();

		ImGui::PushID(a_label);

		// Persistent state per label
		static std::unordered_map<std::string, std::string> s_selectedPlugin;
		static std::unordered_map<std::string, std::array<char, 128>> s_kwPluginFilters;
		static std::unordered_map<std::string, std::array<char, 128>> s_kwFormFilters;

		auto& selectedPlugin = s_selectedPlugin[a_label];
		auto& kwPluginFilter = s_kwPluginFilters[a_label];
		auto& kwFormFilter = s_kwFormFilters[a_label];

		// Plugin selector for keyword browsing
		int pluginIdx = -1;
		for (int i = 0; i < static_cast<int>(plugins.size()); ++i) {
			if (plugins[i] == selectedPlugin) { pluginIdx = i; break; }
		}

		const char* pluginPreview = pluginIdx >= 0 ? plugins[pluginIdx].c_str() : "(select plugin)";

		ImGui::SetNextItemWidth(200);
		if (FilteredCombo("Plugin##kw", pluginPreview, plugins, pluginIdx, kwPluginFilter.data(), kwPluginFilter.size())) {
			if (pluginIdx >= 0 && pluginIdx < static_cast<int>(plugins.size())) {
				selectedPlugin = plugins[pluginIdx];
			}
		}

		// Keyword dropdown from selected plugin
		if (!selectedPlugin.empty()) {
			auto& forms = registry->GetFormsForPlugin(selectedPlugin, RE::ENUM_FORM_ID::kKYWD);

			if (!forms.empty()) {
				std::vector<std::string> displayStrings;
				displayStrings.reserve(forms.size());
				for (auto& f : forms) displayStrings.push_back(f.displayString);

				int formIdx = -1;
				for (int i = 0; i < static_cast<int>(forms.size()); ++i) {
					if (forms[i].editorID == a_editorID) { formIdx = i; break; }
				}

				std::string formPreview = formIdx >= 0 ? forms[formIdx].displayString : a_editorID;

				ImGui::SameLine();
				ImGui::SetNextItemWidth(350);
				if (FilteredCombo("Keyword", formPreview.c_str(), displayStrings, formIdx, kwFormFilter.data(), kwFormFilter.size())) {
					if (formIdx >= 0 && formIdx < static_cast<int>(forms.size())) {
						a_editorID = forms[formIdx].editorID;
						a_dirty = true;
						changed = true;
					}
				}
			}
		}

		// Manual EditorID input
		ImGui::SameLine();
		char buf[256]{};
		strncpy_s(buf, a_editorID.c_str(), sizeof(buf) - 1);
		ImGui::SetNextItemWidth(200);
		if (ImGui::InputText("EditorID##manual", buf, sizeof(buf))) {
			a_editorID = buf;
			a_dirty = true;
			changed = true;
		}

		ImGui::PopID();
		return changed;
	}
}
