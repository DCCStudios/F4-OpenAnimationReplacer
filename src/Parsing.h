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

	// JSON-core variants: parse from an already-loaded json value. The path-taking
	// functions above are thin wrappers (read file -> parse -> call core). Used
	// directly when the config bytes come from inside a BA2 archive.
	void ParseModConfigFromJson(ReplacerMod* a_mod, const nlohmann::json& a_json);
	void ParseSubModConfigFromJson(SubMod* a_subMod, const nlohmann::json& a_json);
	void ParseUserConfigFromJson(SubMod* a_subMod, const nlohmann::json& a_json);

	nlohmann::json LoadJsonFile(const std::filesystem::path& a_path);
	void SaveJsonFile(const std::filesystem::path& a_path, const nlohmann::json& a_json);
}
