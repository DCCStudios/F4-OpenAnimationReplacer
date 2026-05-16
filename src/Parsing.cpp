#include "Parsing.h"
#include "ReplacerMods.h"
#include "ReplacementAnimation.h"
#include "Conditions.h"
#include "BaseConditions.h"
#include "OpenAnimationReplacer.h"
#include "Settings.h"
#include "Variants.h"
#include "Utils.h"

namespace Parsing
{
	static constexpr std::string_view kOarFolderName = "OpenAnimationReplacer";

	static std::filesystem::path ExtractPathAfterMeshes(const std::filesystem::path& a_fullPath)
	{
		auto pathStr = a_fullPath.string();
		std::string lower = pathStr;
		std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		for (auto sep : { "meshes\\", "meshes/" }) {
			auto pos = lower.rfind(sep);
			if (pos != std::string::npos) {
				auto after = pathStr.substr(pos + strlen(sep));
				if (!after.empty()) return std::filesystem::path(after);
			}
		}

		return a_fullPath.filename();
	}

	static void ScanDirectoryForOAR(const std::filesystem::path& a_searchRoot,
		const std::filesystem::path& a_meshesBase, int& a_modCount, int& a_subModCount, int& a_animCount)
	{
		if (!std::filesystem::exists(a_searchRoot)) return;

		try {
			for (auto& entry : std::filesystem::recursive_directory_iterator(a_searchRoot,
				std::filesystem::directory_options::skip_permission_denied))
			{
				if (!entry.is_directory()) continue;
				if (entry.path().filename().string() != kOarFolderName) continue;

				auto meshesPrefix = ExtractPathAfterMeshes(entry.path().parent_path());

				for (auto& modEntry : std::filesystem::directory_iterator(entry.path(),
					std::filesystem::directory_options::skip_permission_denied))
				{
					if (!modEntry.is_directory()) continue;

					try {
						auto mod = ParseReplacerMod(modEntry.path(), meshesPrefix);
						if (mod) {
							for (const auto& sub : mod->GetSubMods()) {
								a_subModCount++;
								a_animCount += static_cast<int>(sub->GetReplacementAnimations().size());
							}
							a_modCount++;
							OpenAnimationReplacer::GetSingleton()->AddReplacerMod(std::move(mod));
						}
					} catch (const std::exception& e) {
						logger::error("[OAR] Error parsing mod at '{}': {}", modEntry.path().string(), e.what());
					}
				}
			}
		} catch (const std::filesystem::filesystem_error& e) {
			logger::error("[OAR] Filesystem error scanning '{}': {}", a_searchRoot.string(), e.what());
		}
	}

	void ParseAllMods()
	{
		RegisterAllConditions();

		auto* oar = OpenAnimationReplacer::GetSingleton();
		oar->isLoading.store(true);
		oar->loadingPhase = "Parsing mods...";
		oar->loadingParsedAnims.store(0);
		oar->loadingLoadedAnims.store(0);
		oar->loadingComplete.store(false);

		auto meshesPath = std::filesystem::current_path() / "Data" / "Meshes";

		if (!std::filesystem::exists(meshesPath)) {
			logger::warn("[OAR] Meshes directory not found at '{}'", meshesPath.string());
			oar->isLoading.store(false);
			return;
		}

		int modCount = 0;
		int subModCount = 0;
		int animCount = 0;

		auto start = std::chrono::high_resolution_clock::now();

		auto actorsPath = meshesPath / "actors";
		if (std::filesystem::exists(actorsPath)) {
			try {
				for (auto& raceDir : std::filesystem::directory_iterator(actorsPath,
					std::filesystem::directory_options::skip_permission_denied))
				{
					if (!raceDir.is_directory()) continue;

					// 3P: actors/<race>/animations/
					auto animsDir = raceDir.path() / "animations";
					if (std::filesystem::exists(animsDir)) {
						ScanDirectoryForOAR(animsDir, meshesPath, modCount, subModCount, animCount);
					}

					// 1P: actors/<race>/_1stperson/
					auto firstPersonDir = raceDir.path() / "_1stperson";
					if (std::filesystem::exists(firstPersonDir)) {
						ScanDirectoryForOAR(firstPersonDir, meshesPath, modCount, subModCount, animCount);
					}

					// Fallback paths some mods use
					auto charsDir = raceDir.path() / "character" / "animations";
					if (std::filesystem::exists(charsDir)) {
						ScanDirectoryForOAR(charsDir, meshesPath, modCount, subModCount, animCount);
					}

				}
			} catch (const std::filesystem::filesystem_error& e) {
				logger::error("[OAR] Filesystem error scanning actors: {}", e.what());
			}
		}

		if (modCount == 0) {
			logger::info("[OAR] No mods found via targeted scan, falling back to full Meshes scan");
			ScanDirectoryForOAR(meshesPath, meshesPath, modCount, subModCount, animCount);
		}

		auto elapsed = std::chrono::high_resolution_clock::now() - start;
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

		oar->loadingTotalAnims.store(animCount);
		oar->loadingParsedAnims.store(animCount);
		oar->loadingPhase = "Loading animations...";

		logger::info("[OAR] Parsed {} mods, {} submods, {} replacement animations in {}ms",
			modCount, subModCount, animCount, ms);
	}

	std::unique_ptr<ReplacerMod> ParseReplacerMod(const std::filesystem::path& a_modPath,
		const std::filesystem::path& a_meshesPrefix)
	{
		auto modName = a_modPath.filename().string();
		auto mod = std::make_unique<ReplacerMod>(modName, a_modPath);

		auto configPath = a_modPath / "config.json";
		if (std::filesystem::exists(configPath)) {
			ParseModConfig(mod.get(), configPath);
		}

		for (auto& subEntry : std::filesystem::directory_iterator(a_modPath,
			std::filesystem::directory_options::skip_permission_denied))
		{
			if (!subEntry.is_directory()) continue;

			try {
				auto subMod = ParseSubMod(subEntry.path(), a_meshesPrefix, mod.get());
				if (subMod) {
					mod->AddSubMod(std::move(subMod));
				}
			} catch (const std::exception& e) {
				logger::error("[OAR] Error parsing submod at '{}': {}", subEntry.path().string(), e.what());
			}
		}

		mod->SortSubMods();
		return mod;
	}

	std::unique_ptr<SubMod> ParseSubMod(const std::filesystem::path& a_subModPath,
		const std::filesystem::path& a_meshesPrefix, ReplacerMod* a_parentMod)
	{
		auto subModName = a_subModPath.filename().string();

		int32_t priority = 0;
		try {
			priority = std::stoi(subModName);
		} catch (...) {}

		auto subMod = std::make_unique<SubMod>(subModName, priority, a_subModPath);

		auto configPath = a_subModPath / "config.json";
		if (std::filesystem::exists(configPath)) {
			ParseSubModConfig(subMod.get(), configPath);
		}

		auto userPath = a_subModPath / "user.json";
		if (std::filesystem::exists(userPath)) {
			ParseUserConfig(subMod.get(), userPath);
			subMod->hasUserConfig = true;
		}

		for (auto& fileEntry : std::filesystem::recursive_directory_iterator(a_subModPath,
			std::filesystem::directory_options::skip_permission_denied))
		{
			if (!fileEntry.is_regular_file()) continue;

			auto ext = fileEntry.path().extension().string();
			std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (ext != ".hkx") continue;

			auto relativePath = std::filesystem::relative(fileEntry.path(), a_subModPath);
			auto originalPath = a_meshesPrefix / relativePath;

		auto normalizedOriginal = Utils::NormalizePath(originalPath);

		auto afterMeshes = ExtractPathAfterMeshes(fileEntry.path());
		auto afterMeshesStr = afterMeshes.string();
		std::string normalizedReplacement;
		{
			auto lowerAfter = afterMeshesStr;
			std::ranges::transform(lowerAfter, lowerAfter.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			auto animPos = lowerAfter.find("animations\\");
			if (animPos == std::string::npos) animPos = lowerAfter.find("animations/");
			if (animPos != std::string::npos) {
				normalizedReplacement = afterMeshesStr.substr(animPos);
			} else {
				normalizedReplacement = afterMeshesStr;
			}
			std::ranges::replace(normalizedReplacement, '/', '\\');
		}

		auto replacement = std::make_unique<ReplacementAnimation>(
			normalizedOriginal, normalizedReplacement, -1, subMod.get());

		subMod->AddReplacementAnimation(replacement.get());

		ReplacementAnimFileInfo fileInfo;
		fileInfo.originalPath = normalizedOriginal;
		fileInfo.replacementPath = normalizedReplacement;
		fileInfo.absoluteDiskPath = fileEntry.path().string();
		fileInfo.parentSubMod = subMod.get();
		OpenAnimationReplacer::GetSingleton()->AddReplacementFileInfo(normalizedOriginal, fileInfo);

		OpenAnimationReplacer::GetSingleton()->AddOwnedAnimation(std::move(replacement));
		OpenAnimationReplacer::GetSingleton()->loadingParsedAnims.fetch_add(1);
		}

		return subMod;
	}

	void ParseModConfig(ReplacerMod* a_mod, const std::filesystem::path& a_configPath)
	{
		auto json = LoadJsonFile(a_configPath);
		if (json.is_null()) return;

		if (json.contains("name")) a_mod->SetName(json["name"].get<std::string>());
		if (json.contains("author")) a_mod->SetAuthor(json["author"].get<std::string>());
		if (json.contains("description")) a_mod->SetDescription(json["description"].get<std::string>());
	}

	void ParseSubModConfig(SubMod* a_subMod, const std::filesystem::path& a_configPath)
	{
		auto json = LoadJsonFile(a_configPath);
		if (json.is_null()) return;

		if (json.contains("name")) a_subMod->SetName(json["name"].get<std::string>());
		if (json.contains("description")) a_subMod->SetDescription(json["description"].get<std::string>());
		if (json.contains("priority")) a_subMod->SetPriority(json["priority"].get<int32_t>());
		if (json.contains("disabled")) a_subMod->SetDisabled(json["disabled"].get<bool>());
		if (json.contains("interruptible")) a_subMod->SetInterruptible(json["interruptible"].get<bool>());
		if (json.contains("replaceOnLoop")) a_subMod->SetReplaceOnLoop(json["replaceOnLoop"].get<bool>());
		if (json.contains("replaceOnEcho")) a_subMod->SetReplaceOnEcho(json["replaceOnEcho"].get<bool>());
		if (json.contains("keepRandomResultsOnLoop")) a_subMod->SetKeepRandomResultsOnLoop(json["keepRandomResultsOnLoop"].get<bool>());
		if (json.contains("shareRandomResults")) a_subMod->SetShareRandomResults(json["shareRandomResults"].get<bool>());
		if (json.contains("replaceAnnotations")) a_subMod->SetReplaceAnnotations(json["replaceAnnotations"].get<bool>());

		if (json.contains("customBlendTimeOnInterrupt"))
			a_subMod->customBlendTimeOnInterrupt = json["customBlendTimeOnInterrupt"].get<float>();
		if (json.contains("customBlendTimeOnLoop"))
			a_subMod->customBlendTimeOnLoop = json["customBlendTimeOnLoop"].get<float>();
		if (json.contains("customBlendTimeOnEcho"))
			a_subMod->customBlendTimeOnEcho = json["customBlendTimeOnEcho"].get<float>();
		if (json.contains("deactivationDelay"))
			a_subMod->deactivationDelay = json["deactivationDelay"].get<float>();
		if (json.contains("playOnceFullBody"))
			a_subMod->playOnceFullBody = json["playOnceFullBody"].get<bool>();
		if (json.contains("eventsOnStart") && json["eventsOnStart"].is_array())
			a_subMod->eventsOnStart = json["eventsOnStart"].get<std::vector<std::string>>();
		if (json.contains("eventsOnEnd") && json["eventsOnEnd"].is_array())
			a_subMod->eventsOnEnd = json["eventsOnEnd"].get<std::vector<std::string>>();

		if (json.contains("requiredProjectName"))
			a_subMod->requiredProjectName = json["requiredProjectName"].get<std::string>();
		if (json.contains("overrideAnimationsFolder"))
			a_subMod->overrideAnimationsFolder = json["overrideAnimationsFolder"].get<std::string>();

		if (json.contains("conditions") && json["conditions"].is_array()) {
			auto condSet = std::make_unique<ConditionSet>();
			for (const auto& condJson : json["conditions"]) {
				auto cond = CreateConditionFromJson(condJson);
				if (cond) condSet->AddCondition(std::move(cond));
			}
			a_subMod->SetConditionSet(std::move(condSet));
		}

		// Partial body animation layering configuration
		if (json.contains("trackFilter") && json["trackFilter"].is_object()) {
			auto& tf = json["trackFilter"];
			a_subMod->trackFilter.enabled = tf.value("enabled", false);
			if (tf.contains("mode")) {
				std::string mode = tf["mode"].get<std::string>();
				if (mode == "override")
					a_subMod->trackFilter.mode = SubMod::TrackFilter::Mode::Override;
				else
					a_subMod->trackFilter.mode = SubMod::TrackFilter::Mode::Additive;
			}
			a_subMod->trackFilter.weight = tf.value("weight", 1.0f);
			a_subMod->trackFilter.blendInTime = tf.value("blendInTime", 0.0f);
			a_subMod->trackFilter.blendOutTime = tf.value("blendOutTime", 0.0f);
			a_subMod->trackFilter.includeChildren = tf.value("includeChildren", true);
			if (tf.contains("bones") && tf["bones"].is_array()) {
				for (const auto& b : tf["bones"]) {
					a_subMod->trackFilter.boneNames.push_back(b.get<std::string>());
				}
			}
			a_subMod->trackFilter.excludeChildren = tf.value("excludeChildren", true);
			if (tf.contains("excludeBones") && tf["excludeBones"].is_array()) {
				for (const auto& b : tf["excludeBones"]) {
					a_subMod->trackFilter.excludeBoneNames.push_back(b.get<std::string>());
				}
			}
			if (a_subMod->trackFilter.enabled) {
				logger::info("[OAR] SubMod '{}' has trackFilter: mode={} weight={:.2f} bones={} exclude={}",
					a_subMod->GetName(),
					a_subMod->trackFilter.mode == SubMod::TrackFilter::Mode::Override ? "override" : "additive",
					a_subMod->trackFilter.weight,
					a_subMod->trackFilter.boneNames.size(),
					a_subMod->trackFilter.excludeBoneNames.size());
			}
		}
	}

	void ParseUserConfig(SubMod* a_subMod, const std::filesystem::path& a_userConfigPath)
	{
		auto json = LoadJsonFile(a_userConfigPath);
		if (json.is_null()) return;

		if (json.contains("priority")) a_subMod->SetPriority(json["priority"].get<int32_t>());
		if (json.contains("disabled")) a_subMod->SetDisabled(json["disabled"].get<bool>());
		if (json.contains("interruptible")) a_subMod->SetInterruptible(json["interruptible"].get<bool>());
		if (json.contains("replaceOnLoop")) a_subMod->SetReplaceOnLoop(json["replaceOnLoop"].get<bool>());
		if (json.contains("replaceOnEcho")) a_subMod->SetReplaceOnEcho(json["replaceOnEcho"].get<bool>());
		if (json.contains("replaceAnnotations")) a_subMod->SetReplaceAnnotations(json["replaceAnnotations"].get<bool>());
		if (json.contains("deactivationDelay"))
			a_subMod->deactivationDelay = json["deactivationDelay"].get<float>();
		if (json.contains("playOnceFullBody"))
			a_subMod->playOnceFullBody = json["playOnceFullBody"].get<bool>();
		if (json.contains("eventsOnStart") && json["eventsOnStart"].is_array())
			a_subMod->eventsOnStart = json["eventsOnStart"].get<std::vector<std::string>>();
		if (json.contains("eventsOnEnd") && json["eventsOnEnd"].is_array())
			a_subMod->eventsOnEnd = json["eventsOnEnd"].get<std::vector<std::string>>();

		if (json.contains("conditions") && json["conditions"].is_array()) {
			auto condSet = std::make_unique<ConditionSet>();
			for (const auto& condJson : json["conditions"]) {
				auto cond = CreateConditionFromJson(condJson);
				if (cond) condSet->AddCondition(std::move(cond));
			}
			a_subMod->SetConditionSet(std::move(condSet));
		}
	}

	nlohmann::json LoadJsonFile(const std::filesystem::path& a_path)
	{
		try {
			std::ifstream file(a_path);
			if (!file.is_open()) {
				logger::warn("[OAR] Failed to open JSON file: {}", a_path.string());
				return nlohmann::json();
			}
			return nlohmann::json::parse(file, nullptr, false, true);
		} catch (const std::exception& e) {
			logger::error("[OAR] Error parsing JSON '{}': {}", a_path.string(), e.what());
			return nlohmann::json();
		}
	}

	void SaveJsonFile(const std::filesystem::path& a_path, const nlohmann::json& a_json)
	{
		try {
			std::filesystem::create_directories(a_path.parent_path());
			std::ofstream file(a_path);
			if (!file.is_open()) {
				logger::error("[OAR] Failed to open JSON file for writing: {}", a_path.string());
				return;
			}
			file << a_json.dump(4);
			logger::info("[OAR] Saved config to '{}'", a_path.string());
		} catch (const std::exception& e) {
			logger::error("[OAR] Error saving JSON '{}': {}", a_path.string(), e.what());
		}
	}
}
