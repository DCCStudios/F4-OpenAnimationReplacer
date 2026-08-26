#include "FormRegistry.h"

#include <unordered_set>


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

	// IsForm matches REFERENCE FormIDs, and placed references are not in the
	// type-keyed base-form arrays — but the one reference nearly every config
	// targets is the player. Offer it explicitly so the IsForm dropdown is
	// useful instead of empty.
	if (a_formType == RE::ENUM_FORM_ID::kACHR) {
		std::string lowerPlugin = a_pluginName;
		std::ranges::transform(lowerPlugin, lowerPlugin.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (lowerPlugin == "fallout4.esm") {
			FormEntry player;
			player.fullFormID = 0x14;
			player.localFormID = 0x14;
			player.fullName = "Player";
			player.editorID = "PlayerRef";
			player.BuildDisplayString();
			a_out.push_back(std::move(player));
		}
	}

	auto& arr = dh->formArrays[idx];
	for (auto* form : arr) {
		if (!form || form->IsDeleted()) continue;

		auto* file = form->GetFile();
		if (!file) continue;
		if (a_pluginName != file->GetFilename()) continue;

		FormEntry entry;
		entry.fullFormID = form->GetFormID();
		// ESL-flagged (small) files use 12-bit local IDs under the 0xFExxx
		// prefix — masking those with 0x00FFFFFF kept the small-file index in
		// the "local" ID, and a dropdown pick from an ESL plugin stored an ID
		// LookupForm can never resolve.
		entry.localFormID = (entry.fullFormID & 0xFE000000u) == 0xFE000000u
			? (entry.fullFormID & 0x00000FFFu)
			: (entry.fullFormID & 0x00FFFFFFu);

		const char* eid = form->GetFormEditorID();
		if (eid && eid[0]) entry.editorID = eid;

		if (auto* fullName = form->As<RE::TESFullName>()) {
			const char* name = fullName->GetFullName();
			if (name && name[0]) entry.fullName = name;
		}

		entry.BuildDisplayString();
		a_out.push_back(std::move(entry));
	}

	// Placed references (ACHR) are not reliably present in the type-keyed
	// form arrays — the IsForm dropdown came up empty for every plugin except
	// the synthetic Player entry above. Enumerate the global all-forms map
	// instead, filtered by type + plugin, displaying each reference by its
	// BASE actor's name. One-time cost per (plugin, type); the caller caches.
	if (a_formType == RE::ENUM_FORM_ID::kACHR) {
		std::unordered_set<uint32_t> seen;
		for (auto& e : a_out) seen.insert(e.fullFormID);

		constexpr size_t kMaxRefEntries = 8000;
		auto [allForms, allFormsLock] = RE::TESForm::GetAllForms();
		if (allForms) {
			allFormsLock.get().lock_read();
			for (auto& [id, form] : *allForms) {
				if (a_out.size() >= kMaxRefEntries) break;
				if (!form || form->GetFormType() != RE::ENUM_FORM_ID::kACHR) continue;
				if (form->IsDeleted()) continue;
				if (seen.contains(form->GetFormID())) continue;

				auto* file = form->GetFile();
				if (!file || a_pluginName != file->GetFilename()) continue;

				FormEntry entry;
				entry.fullFormID = form->GetFormID();
				entry.localFormID = (entry.fullFormID & 0xFE000000u) == 0xFE000000u
					? (entry.fullFormID & 0x00000FFFu)
					: (entry.fullFormID & 0x00FFFFFFu);

				if (auto* ref = form->As<RE::TESObjectREFR>()) {
					if (auto* base = ref->data.objectReference) {
						const char* baseEid = base->GetFormEditorID();
						if (baseEid && baseEid[0]) entry.editorID = baseEid;
						if (auto* fullName = base->As<RE::TESFullName>()) {
							const char* name = fullName->GetFullName();
							if (name && name[0]) entry.fullName = name;
						}
					}
				}

				entry.BuildDisplayString();
				seen.insert(entry.fullFormID);
				a_out.push_back(std::move(entry));
			}
			allFormsLock.get().unlock_read();
		}
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

const std::vector<FormEntry>& FormRegistry::GetFormsForPluginMulti(
	const std::string& a_pluginName, const std::vector<RE::ENUM_FORM_ID>& a_formTypes)
{
	if (a_formTypes.empty()) return s_empty;
	if (a_formTypes.size() == 1) return GetFormsForPlugin(a_pluginName, a_formTypes.front());

	std::string key = a_pluginName;
	key.push_back('\x01');
	{
		std::vector<uint8_t> bytes;
		bytes.reserve(a_formTypes.size());
		for (auto t : a_formTypes) bytes.push_back(static_cast<uint8_t>(t));
		std::ranges::sort(bytes);
		for (auto b : bytes) key.push_back(static_cast<char>(b));
	}

	{
		std::shared_lock lock(cacheMutex);
		auto it = multiFormCache.find(key);
		if (it != multiFormCache.end()) return it->second;
	}

	std::unique_lock lock(cacheMutex);
	auto it = multiFormCache.find(key);
	if (it != multiFormCache.end()) return it->second;

	auto& vec = multiFormCache[key];
	for (auto t : a_formTypes) {
		CollectFormsOfType(a_pluginName, t, vec);
	}
	// CollectFormsOfType sorts cumulatively on each call, so vec is sorted.
	return vec;
}

void FormRegistry::InvalidateCache()
{
	{
		std::unique_lock lock(cacheMutex);
		formCache.clear();
		multiFormCache.clear();
	}
	{
		std::unique_lock lock(pluginMutex);
		cachedPlugins.clear();
		pluginsCached = false;
	}
}
