#pragma once

class ReplacerMod;
class SubMod;

namespace OAR::BA2
{
	class Index;
}

namespace Parsing
{
	void ParseAllMods();

	std::unique_ptr<ReplacerMod> ParseReplacerMod(const std::filesystem::path& a_modPath,
		const std::filesystem::path& a_meshesPrefix,
		const OAR::BA2::Index* a_archiveIndex = nullptr);

	std::unique_ptr<SubMod> ParseSubMod(const std::filesystem::path& a_subModPath,
		const std::filesystem::path& a_meshesPrefix, ReplacerMod* a_parentMod,
		const OAR::BA2::Index* a_archiveIndex = nullptr);

	void ParseModConfig(ReplacerMod* a_mod, const std::filesystem::path& a_configPath);
	void ParseSubModConfig(SubMod* a_subMod, const std::filesystem::path& a_configPath);
	void ParseUserConfig(SubMod* a_subMod, const std::filesystem::path& a_userConfigPath);

	nlohmann::json LoadJsonFile(const std::filesystem::path& a_path);
	void SaveJsonFile(const std::filesystem::path& a_path, const nlohmann::json& a_json);
}
