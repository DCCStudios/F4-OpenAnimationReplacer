#include "Utils.h"

namespace Utils
{
	std::string ToLower(std::string_view a_str)
	{
		std::string result(a_str);
		std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return result;
	}

	std::string NormalizePath(const std::filesystem::path& a_path)
	{
		auto normalized = a_path.lexically_normal().string();
		std::ranges::replace(normalized, '/', '\\');
		return ToLower(normalized);
	}

	std::string NormalizeAnimName(const char* a_name)
	{
		if (!a_name) return {};
		std::string result(a_name);
		std::ranges::replace(result, '/', '\\');
		return ToLower(result);
	}

	RE::Actor* GetActorFromRef(RE::TESObjectREFR* a_ref)
	{
		if (!a_ref) return nullptr;
		return a_ref->As<RE::Actor>();
	}

	static RE::TESFile* FindModByName(RE::TESDataHandler* a_handler, const std::string& a_name)
	{
		for (auto* file : a_handler->compiledFileCollection.files) {
			if (file && _stricmp(file->GetFilename().data(), a_name.c_str()) == 0)
				return file;
		}
		for (auto* file : a_handler->compiledFileCollection.smallFiles) {
			if (file && _stricmp(file->GetFilename().data(), a_name.c_str()) == 0)
				return file;
		}
		return nullptr;
	}

	RE::TESForm* LookupForm(uint32_t a_formID, const std::string& a_plugin)
	{
		if (a_plugin.empty()) {
			return RE::TESForm::GetFormByID(a_formID);
		}

		auto* handler = RE::TESDataHandler::GetSingleton();
		if (!handler) return nullptr;

		auto* modInfo = FindModByName(handler, a_plugin);
		if (!modInfo) {
			logger::warn("[OAR] Plugin '{}' not loaded", a_plugin);
			return nullptr;
		}

		bool isLight = false;
		for (auto* smallFile : handler->compiledFileCollection.smallFiles) {
			if (smallFile == modInfo) {
				isLight = true;
				break;
			}
		}

		uint32_t fullID = isLight
			? (0xFE000000 | (static_cast<uint32_t>(modInfo->GetSmallFileCompileIndex()) << 12) | (a_formID & 0xFFF))
			: ((static_cast<uint32_t>(modInfo->GetCompileIndex()) << 24) | (a_formID & 0x00FFFFFF));

		return RE::TESForm::GetFormByID(fullID);
	}
}
