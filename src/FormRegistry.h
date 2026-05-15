#pragma once

#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "RE/Bethesda/TESForms.h"

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

	mutable std::shared_mutex pluginMutex;
	std::vector<std::string> cachedPlugins;
	bool pluginsCached{ false };

	static const std::vector<FormEntry> s_empty;
};
