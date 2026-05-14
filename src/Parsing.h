#pragma once

class ReplacerMod;
class SubMod;

namespace Parsing
{
	void ParseAllMods();

	std::unique_ptr<ReplacerMod> ParseReplacerMod(const std::filesystem::path& a_modPath,
		const std::filesystem::path& a_meshesPrefix);

	std::unique_ptr<SubMod> ParseSubMod(const std::filesystem::path& a_subModPath,
		const std::filesystem::path& a_meshesPrefix, ReplacerMod* a_parentMod);

	void ParseModConfig(ReplacerMod* a_mod, const std::filesystem::path& a_configPath);
	void ParseSubModConfig(SubMod* a_subMod, const std::filesystem::path& a_configPath);
	void ParseUserConfig(SubMod* a_subMod, const std::filesystem::path& a_userConfigPath);

	nlohmann::json LoadJsonFile(const std::filesystem::path& a_path);
	void SaveJsonFile(const std::filesystem::path& a_path, const nlohmann::json& a_json);
}
