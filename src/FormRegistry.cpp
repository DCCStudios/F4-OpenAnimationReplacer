#include "FormRegistry.h"

#include "RE/Bethesda/TESDataHandler.h"
#include "RE/Bethesda/TESFile.h"
#include "RE/Bethesda/TESForms.h"
#include "RE/Bethesda/FormComponents.h"

const std::vector<FormEntry> FormRegistry::s_empty{};

void FormEntry::BuildDisplayString()
{
	if (!fullName.empty() && !editorID.empty()) {
		displayString = std::format("{} [{}] (0x{:X})", fullName, editorID, localFormID);
	} else if (!editorID.empty()) {
		displayString = std::format("{} (0x{:X})", editorID, localFormID);
	} else if (!fullName.empty()) {
		displayString = std::format("{} (0x{:X})", fullName, localFormID);
	} else {
		displayString = std::format("0x{:X}", localFormID);
	}
}

FormRegistry* FormRegistry::GetSingleton()
{
	static FormRegistry instance;
	return &instance;
}

std::vector<std::string> FormRegistry::GetLoadedPlugins()
{
	{
		std::shared_lock lock(pluginMutex);
		if (pluginsCached) return cachedPlugins;
	}

	std::unique_lock lock(pluginMutex);
	if (pluginsCached) return cachedPlugins;

	cachedPlugins.clear();
	auto* dh = RE::TESDataHandler::GetSingleton();
	if (!dh) return cachedPlugins;

	for (auto* file : dh->compiledFileCollection.files) {
		if (file) {
			auto name = std::string(file->GetFilename());
			if (!name.empty()) cachedPlugins.push_back(name);
		}
	}
	for (auto* file : dh->compiledFileCollection.smallFiles) {
		if (file) {
			auto name = std::string(file->GetFilename());
			if (!name.empty()) cachedPlugins.push_back(name);
		}
	}

	std::ranges::sort(cachedPlugins, [](const std::string& a, const std::string& b) {
		auto la = a, lb = b;
		std::ranges::transform(la, la.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::ranges::transform(lb, lb.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return la < lb;
	});

	pluginsCached = true;
	return cachedPlugins;
}

static void CollectFormsOfType(
	const std::string& a_pluginName,
	RE::ENUM_FORM_ID a_formType,
	std::vector<FormEntry>& a_out)
{
	auto* dh = RE::TESDataHandler::GetSingleton();
	if (!dh) return;

	auto idx = static_cast<std::size_t>(a_formType);
	if (idx >= static_cast<std::size_t>(RE::ENUM_FORM_ID::kTotal)) return;

	auto& arr = dh->formArrays[idx];
	for (auto* form : arr) {
		if (!form || form->IsDeleted()) continue;

		auto* file = form->GetFile();
		if (!file) continue;
		if (a_pluginName != file->GetFilename()) continue;

		FormEntry entry;
		entry.fullFormID = form->GetFormID();
		entry.localFormID = entry.fullFormID & 0x00FFFFFF;

		const char* eid = form->GetFormEditorID();
		if (eid && eid[0]) entry.editorID = eid;

		if (auto* fullName = form->As<RE::TESFullName>()) {
			const char* name = fullName->GetFullName();
			if (name && name[0]) entry.fullName = name;
		}

		entry.BuildDisplayString();
		a_out.push_back(std::move(entry));
	}

	std::ranges::sort(a_out, [](const FormEntry& a, const FormEntry& b) {
		if (!a.editorID.empty() && !b.editorID.empty()) return a.editorID < b.editorID;
		if (!a.editorID.empty()) return true;
		if (!b.editorID.empty()) return false;
		if (!a.fullName.empty() && !b.fullName.empty()) return a.fullName < b.fullName;
		return a.fullFormID < b.fullFormID;
	});
}

const std::vector<FormEntry>& FormRegistry::GetFormsForPlugin(
	const std::string& a_pluginName, RE::ENUM_FORM_ID a_formType)
{
	CacheKey key{ a_pluginName, static_cast<uint8_t>(a_formType) };

	{
		std::shared_lock lock(cacheMutex);
		auto it = formCache.find(key);
		if (it != formCache.end()) return it->second;
	}

	std::unique_lock lock(cacheMutex);
	auto it = formCache.find(key);
	if (it != formCache.end()) return it->second;

	auto& vec = formCache[key];
	CollectFormsOfType(a_pluginName, a_formType, vec);
	return vec;
}

void FormRegistry::InvalidateCache()
{
	{
		std::unique_lock lock(cacheMutex);
		formCache.clear();
	}
	{
		std::unique_lock lock(pluginMutex);
		cachedPlugins.clear();
		pluginsCached = false;
	}
}
