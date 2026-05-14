#pragma once

class ReadLocker
{
public:
	explicit ReadLocker(std::shared_mutex& a_mutex) : _lock(a_mutex) {}
private:
	std::shared_lock<std::shared_mutex> _lock;
};

class WriteLocker
{
public:
	explicit WriteLocker(std::shared_mutex& a_mutex) : _lock(a_mutex) {}
private:
	std::unique_lock<std::shared_mutex> _lock;
};

namespace Utils
{
	std::string ToLower(std::string_view a_str);
	std::string NormalizePath(const std::filesystem::path& a_path);
	std::string NormalizeAnimName(const char* a_name);
	RE::Actor* GetActorFromRef(RE::TESObjectREFR* a_ref);
	RE::TESForm* LookupForm(uint32_t a_formID, const std::string& a_plugin);
}
