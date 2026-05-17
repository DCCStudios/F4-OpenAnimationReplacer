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

	// Mirrors ExtractAnimSuffix in Hooks.cpp: extracts the cache key from a full normalized path
	static std::string PathToAnimSuffix(const std::string& a_normalizedPath)
	{
		auto lower = a_normalizedPath;
		std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::ranges::replace(lower, '/', '\\');

		auto pos = lower.find("animations\\");
		if (pos != std::string::npos) {
			auto suffix = lower.substr(pos + 11);
			auto dot = suffix.rfind('.');
			if (dot != std::string::npos) suffix = suffix.substr(0, dot);
			return suffix;
		}

		auto dot = lower.rfind('.');
		if (dot != std::string::npos) lower = lower.substr(0, dot);
		auto lastSep = lower.rfind('\\');
		if (lastSep != std::string::npos && lastSep > 0) {
			auto prevSep = lower.rfind('\\', lastSep - 1);
			if (prevSep != std::string::npos)
				return lower.substr(prevSep + 1);
		}
		return lower;
	}

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

		struct HkxFileEntry {
			std::filesystem::path diskPath;
			std::filesystem::path relativePath;  // relative to submod
			std::string filename;
			std::string stem;
			std::string baseKey;  // lowercase dir\baseStem (variant suffix stripped)
			int variantIndex;     // -1 = not a variant candidate, 0+ = variant N
			bool isBase;          // true if no _N suffix detected
		};

		// First pass: collect all .hkx files
		std::vector<HkxFileEntry> allFiles;
		std::unordered_set<std::string> baseKeys;  // keys that have a base file (no _N suffix)

		for (auto& fileEntry : std::filesystem::recursive_directory_iterator(a_subModPath,
			std::filesystem::directory_options::skip_permission_denied))
		{
			if (!fileEntry.is_regular_file()) continue;

			auto ext = fileEntry.path().extension().string();
			std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (ext != ".hkx") continue;

			auto relativePath = std::filesystem::relative(fileEntry.path(), a_subModPath);
			std::string stem = relativePath.stem().string();
			std::string relDir = relativePath.parent_path().string();

			// Try to detect _N variant suffix
			int variantIdx = -1;
			std::string baseStem = stem;
			bool isBase = true;
			{
				auto underscorePos = stem.rfind('_');
				if (underscorePos != std::string::npos && underscorePos + 1 < stem.size()) {
					auto numPart = stem.substr(underscorePos + 1);
					bool allDigits = !numPart.empty() && std::all_of(numPart.begin(), numPart.end(), ::isdigit);
					if (allDigits) {
						variantIdx = std::stoi(numPart);
						baseStem = stem.substr(0, underscorePos);
						isBase = false;
					}
				}
			}

			// Construct base key: relDir\baseStem (normalized, lowercase)
			std::string baseKey;
			if (!relDir.empty() && relDir != ".") {
				baseKey = relDir + "\\" + baseStem;
			} else {
				baseKey = baseStem;
			}
			std::ranges::transform(baseKey, baseKey.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::ranges::replace(baseKey, '/', '\\');

			if (isBase) baseKeys.insert(baseKey);

			HkxFileEntry hfe;
			hfe.diskPath = fileEntry.path();
			hfe.relativePath = relativePath;
			hfe.filename = relativePath.filename().string();
			hfe.stem = stem;
			hfe.baseKey = baseKey;
			hfe.variantIndex = variantIdx;
			hfe.isBase = isBase;
			allFiles.push_back(std::move(hfe));
		}

		// Second pass: group files — only treat _N files as variants if a base exists
		std::map<std::string, std::vector<HkxFileEntry*>> variantGroups;
		std::vector<HkxFileEntry*> standaloneFiles;

		for (auto& f : allFiles) {
			if (f.isBase) {
				// This is a base file, always part of its own group
				variantGroups[f.baseKey].push_back(&f);
			} else {
				// This has _N suffix — only group it if a matching base exists
				if (baseKeys.count(f.baseKey)) {
					variantGroups[f.baseKey].push_back(&f);
				} else {
					// No matching base → treat as standalone
					standaloneFiles.push_back(&f);
				}
			}
		}

		// Helper lambda to register a single file as a ReplacementAnimation
		auto registerSingleFile = [&](HkxFileEntry* f) {
			auto originalPath = a_meshesPrefix / f->relativePath;
			auto normalizedOriginal = Utils::NormalizePath(originalPath);

			auto afterMeshes = ExtractPathAfterMeshes(f->diskPath);
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

			ReplacementAnimFileInfo fileInfo;
			fileInfo.originalPath = normalizedOriginal;
			fileInfo.replacementPath = normalizedReplacement;
			fileInfo.absoluteDiskPath = f->diskPath.string();
			fileInfo.parentSubMod = subMod.get();
			fileInfo.replacementAnim = replacement.get();
			OpenAnimationReplacer::GetSingleton()->AddReplacementFileInfo(normalizedOriginal, fileInfo);

			subMod->AddReplacementAnimation(replacement.get());
			OpenAnimationReplacer::GetSingleton()->AddOwnedAnimation(std::move(replacement));
			OpenAnimationReplacer::GetSingleton()->loadingParsedAnims.fetch_add(1);
		};

		// Register standalone files (no variant grouping)
		for (auto* f : standaloneFiles) {
			registerSingleFile(f);
		}

		// Third pass: register each variant group
		for (auto& [baseKey, files] : variantGroups) {
			if (files.size() == 1) {
				// Single file in group (just the base, no variants found)
				registerSingleFile(files[0]);
				continue;
			}

			// Sort: base (isBase=true or variantIndex=-1 meaning no suffix) first, then by variant index
			std::ranges::sort(files, [](const HkxFileEntry* a, const HkxFileEntry* b) {
				if (a->isBase != b->isBase) return a->isBase;  // base first
				return a->variantIndex < b->variantIndex;
			});

			auto* baseFile = files[0];

			// Compute normalizedOriginal from the base file's actual path
			auto originalPath = a_meshesPrefix / baseFile->relativePath;
			auto normalizedOriginal = Utils::NormalizePath(originalPath);

			// Compute normalizedReplacement for the base file
			auto afterMeshes = ExtractPathAfterMeshes(baseFile->diskPath);
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

			// Create variants for all files in the group
			auto variants = std::make_unique<Variants>();

			for (size_t i = 0; i < files.size(); ++i) {
				auto* f = files[i];

				// Compute a unique cache path for this variant
				// Base (index 0) uses normalizedOriginal, variants get "__v{N}" appended
				std::string variantCachePath;
				if (i == 0) {
					variantCachePath = normalizedOriginal;
				} else {
					auto dotPos = normalizedOriginal.rfind('.');
					std::string noExt = (dotPos != std::string::npos) ? normalizedOriginal.substr(0, dotPos) : normalizedOriginal;
					variantCachePath = noExt + "__v" + std::to_string(f->variantIndex) + ".hkx";
				}

				VariantEntry entry;
				entry.filename = f->filename;
				entry.cacheSuffix = PathToAnimSuffix(variantCachePath);
				entry.weight = 1.0f;
				entry.bindingIndex = static_cast<int16_t>(i);
				variants->AddVariant(std::move(entry));

				// Register each variant file with the cache system
				ReplacementAnimFileInfo fileInfo;
				fileInfo.originalPath = variantCachePath;
				fileInfo.replacementPath = normalizedReplacement;
				fileInfo.absoluteDiskPath = f->diskPath.string();
				fileInfo.parentSubMod = subMod.get();
				fileInfo.replacementAnim = replacement.get();
				OpenAnimationReplacer::GetSingleton()->AddReplacementFileInfo(variantCachePath, fileInfo);
			}

			// Apply variant config from SubMod (parsed from JSON)
			variants->SetMode(subMod->variantMode);
			if (!subMod->variantWeights.empty()) {
				for (auto& ve : variants->GetEntriesMutable()) {
					auto it = subMod->variantWeights.find(ve.filename);
					if (it != subMod->variantWeights.end()) {
						ve.weight = it->second;
					}
				}
			}

			replacement->SetVariants(std::move(variants));

			logger::info("[OAR-Variants] Grouped {} variants for '{}' in submod '{}' (mode={})",
				files.size(), normalizedOriginal, subMod->GetName(),
				subMod->variantMode == VariantMode::kSequential ? "sequential" : "random");

			subMod->AddReplacementAnimation(replacement.get());
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

	// Variant animation configuration
	if (json.contains("variants") && json["variants"].is_object()) {
		auto& vj = json["variants"];
		if (vj.contains("enabled"))
			a_subMod->variantsEnabled = vj["enabled"].get<bool>();
		if (vj.contains("mode")) {
			std::string modeStr = vj["mode"].get<std::string>();
			if (modeStr == "sequential")
				a_subMod->variantMode = VariantMode::kSequential;
			else
				a_subMod->variantMode = VariantMode::kRandom;
		}
		if (vj.contains("rerollPolicy")) {
			std::string policyStr = vj["rerollPolicy"].get<std::string>();
			if (policyStr == "whileActive")
				a_subMod->variantRerollPolicy = VariantRerollPolicy::kWhileActive;
			else
				a_subMod->variantRerollPolicy = VariantRerollPolicy::kOnEachPlay;
		}
		if (vj.contains("weights") && vj["weights"].is_object()) {
			for (auto& [filename, weight] : vj["weights"].items()) {
				if (weight.is_number())
					a_subMod->variantWeights[filename] = weight.get<float>();
			}
		}
	}

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

	// Variant animation configuration (user override)
	if (json.contains("variants") && json["variants"].is_object()) {
		auto& vj = json["variants"];
		if (vj.contains("enabled"))
			a_subMod->variantsEnabled = vj["enabled"].get<bool>();
		if (vj.contains("mode")) {
			std::string modeStr = vj["mode"].get<std::string>();
			if (modeStr == "sequential")
				a_subMod->variantMode = VariantMode::kSequential;
			else
				a_subMod->variantMode = VariantMode::kRandom;
		}
		if (vj.contains("rerollPolicy")) {
			std::string policyStr = vj["rerollPolicy"].get<std::string>();
			if (policyStr == "whileActive")
				a_subMod->variantRerollPolicy = VariantRerollPolicy::kWhileActive;
			else
				a_subMod->variantRerollPolicy = VariantRerollPolicy::kOnEachPlay;
		}
		if (vj.contains("weights") && vj["weights"].is_object()) {
			for (auto& [filename, weight] : vj["weights"].items()) {
				if (weight.is_number())
					a_subMod->variantWeights[filename] = weight.get<float>();
			}
		}
	}

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
