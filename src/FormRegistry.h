#pragma once

#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct FormEntry
{
	uint32_t fullFormID{ 0 };
	uint32_t localFormID{ 0 };
	std::string editorID;
	std::string fullName;
	std::string displayString;

	void BuildDisplayString();
};

class FormRegistry
{
public:
	static FormRegistry* GetSingleton();

	std::vector<std::string> GetLoadedPlugins();
	const std::vector<FormEntry>& GetFormsForPlugin(const std::string& a_pluginName, RE::ENUM_FORM_ID a_formType);
	// Merged, sorted union of several form types from one plugin (item-style
	// conditions where the "relevant type" is a set: weapons, armor, ammo...).
	// Cached per (plugin, type set) like the single-type variant.
	const std::vector<FormEntry>& GetFormsForPluginMulti(const std::string& a_pluginName,
		const std::vector<RE::ENUM_FORM_ID>& a_formTypes);
	void InvalidateCache();

private:
	FormRegistry() = default;

	using CacheKey = std::pair<std::string, uint8_t>;
	struct CacheKeyHash
	{
		size_t operator()(const CacheKey& k) const
		{
			auto h1 = std::hash<std::string>{}(k.first);
			auto h2 = std::hash<uint8_t>{}(k.second);
			return h1 ^ (h2 << 16);
		}
	};

	mutable std::shared_mutex cacheMutex;
	std::unordered_map<CacheKey, std::vector<FormEntry>, CacheKeyHash> formCache;
	// Multi-type cache: key = plugin + '\x01' + one byte per type (sorted).
	std::unordered_map<std::string, std::vector<FormEntry>> multiFormCache;

	mutable std::shared_mutex pluginMutex;
	std::vector<std::string> cachedPlugins;
	bool pluginsCached{ false };

	static const std::vector<FormEntry> s_empty;
};
