#include "OpenAnimationReplacer.h"
#include "ReplacerMods.h"
#include "ReplacementAnimation.h"
#include "Settings.h"

void OpenAnimationReplacer::AddReplacerMod(std::unique_ptr<ReplacerMod> a_mod)
{
	WriteLocker lock(modsMutex);
	replacerMods.push_back(std::move(a_mod));
}

void OpenAnimationReplacer::ClearAllMods()
{
	{
		WriteLocker lock(modsMutex);
		replacerMods.clear();
	}
	{
		WriteLocker lock(projectDataMutex);
		projectDataMap.clear();
	}
	{
		WriteLocker lock(ownedAnimsMutex);
		ownedAnimations.clear();
	}
	{
		WriteLocker lock(pathMapMutex);
		animPathToReplacementsMap.clear();
	}
}

ReplacementAnimation* OpenAnimationReplacer::GetReplacementAnimation(
	RE::hkbClipGenerator* a_clipGen,
	int16_t a_originalIndex,
	RE::TESObjectREFR* a_refr)
{
	if (!a_clipGen || !a_refr) return nullptr;
	if (!a_clipGen->animationName.data()) return nullptr;

	auto clipName = Utils::NormalizeAnimName(a_clipGen->animationName.data());
	if (clipName.empty()) return nullptr;

	auto* projectData = GetReplacerProjectData(nullptr);

	ReadLocker lock(modsMutex);
	for (const auto& mod : replacerMods) {
		for (const auto& subMod : mod->GetSubMods()) {
			if (subMod->IsDisabled()) continue;
			if (!subMod->EvaluateConditions(a_refr, a_clipGen)) continue;

			for (auto* anim : subMod->GetReplacementAnimations()) {
				if (!anim || anim->GetBindingIndex() < 0) continue;

				const auto& origPath = anim->GetOriginalPath();

				if (clipName == origPath) return anim;

				if (clipName.ends_with(origPath)) {
					auto pos = clipName.size() - origPath.size();
					if (pos == 0 || clipName[pos - 1] == '\\') return anim;
				}
				if (origPath.ends_with(clipName)) {
					auto pos = origPath.size() - clipName.size();
					if (pos == 0 || origPath[pos - 1] == '\\') return anim;
				}
			}
		}
	}

	return nullptr;
}

void OpenAnimationReplacer::AddReplacementFileInfo(const std::string& a_normalizedOrigPath,
	const ReplacementAnimFileInfo& a_info)
{
	WriteLocker lock(pathMapMutex);
	animPathToReplacementsMap[a_normalizedOrigPath].push_back(a_info);
}

bool OpenAnimationReplacer::HasReplacementsForPath(const std::string& a_normalizedPath) const
{
	ReadLocker lock(pathMapMutex);
	return animPathToReplacementsMap.contains(a_normalizedPath);
}

static bool PathMatchesAnimation(const std::string& a_animPath, const std::string& a_searchPath)
{
	if (a_animPath == a_searchPath) return true;

	if (a_animPath.ends_with(a_searchPath)) {
		auto pos = a_animPath.size() - a_searchPath.size();
		if (pos == 0 || a_animPath[pos - 1] == '\\') return true;
	}
	if (a_searchPath.ends_with(a_animPath)) {
		auto pos = a_searchPath.size() - a_animPath.size();
		if (pos == 0 || a_searchPath[pos - 1] == '\\') return true;
	}
	return false;
}

bool OpenAnimationReplacer::CreateReplacementAnimations(
	[[maybe_unused]] const char* a_animationPath,
	RE::hkbCharacterStringData* a_stringData)
{
	if (!a_stringData) return false;

	ReadLocker pathLock(pathMapMutex);
	if (animPathToReplacementsMap.empty()) return false;

	struct HkArrayAccessor {
		RE::hkbCharacterStringData::FileNameMeshNamePair* data;
		int32_t size;
		int32_t capacityAndFlags;

		int32_t capacity() const { return capacityAndFlags & 0x3FFFFFFF; }
		int32_t flags() const { return capacityAndFlags & ~0x3FFFFFFF; }
	};

	using Pair = RE::hkbCharacterStringData::FileNameMeshNamePair;
	auto& bundleArr = reinterpret_cast<HkArrayAccessor&>(a_stringData->animationNames);

	int32_t originalCount = bundleArr.size;

	int32_t neededSlots = 0;
	for (auto& [mapKey, replacementInfos] : animPathToReplacementsMap) {
		neededSlots += static_cast<int32_t>(replacementInfos.size());
	}
	if (neededSlots == 0) return false;

	logger::info("[OAR] CreateReplacementAnimations: injecting {} replacements (from {} map entries) into stringData with {} existing entries",
		neededSlots, animPathToReplacementsMap.size(), originalCount);

	if (Settings::GetSingleton()->bVerboseLogging) {
		for (auto& [key, vec] : animPathToReplacementsMap) {
			logger::info("[OAR]   mapKey='{}' ({} replacements)", key, vec.size());
		}
	}

	if (!bundleArr.data || bundleArr.capacity() == 0) {
		int32_t newCapacity = neededSlots + 16;
		auto* newBuf = static_cast<Pair*>(_aligned_malloc(
			static_cast<size_t>(newCapacity) * sizeof(Pair), 16));
		if (!newBuf) {
			logger::error("[OAR] Failed to allocate new animationNames array");
			return false;
		}
		memset(newBuf, 0, static_cast<size_t>(newCapacity) * sizeof(Pair));
		bundleArr.data = newBuf;
		bundleArr.size = 0;
		bundleArr.capacityAndFlags = newCapacity;
		originalCount = 0;
		logger::info("[OAR]   Created new animationNames array with capacity {}", newCapacity);
	}

	int32_t availableSlots = bundleArr.capacity() - bundleArr.size;
	if (availableSlots < neededSlots) {
		int32_t newCapacity = originalCount + neededSlots + 64;
		auto* newBuf = static_cast<Pair*>(_aligned_malloc(
			static_cast<size_t>(newCapacity) * sizeof(Pair), 16));
		if (!newBuf) {
			logger::error("[OAR] Failed to grow animationNames array");
			return false;
		}
		if (bundleArr.data && originalCount > 0) {
			memcpy(newBuf, bundleArr.data, static_cast<size_t>(originalCount) * sizeof(Pair));
		}
		memset(newBuf + originalCount, 0, static_cast<size_t>(newCapacity - originalCount) * sizeof(Pair));

		logger::info("[OAR]   Grew animationNames: {}/{} -> capacity {} ({:X})",
			originalCount, bundleArr.capacity(), newCapacity, reinterpret_cast<uintptr_t>(newBuf));

		bundleArr.data = newBuf;
		bundleArr.capacityAndFlags = newCapacity | bundleArr.flags();
	}

	auto& projectData = GetOrCreateReplacerProjectData(a_stringData);
	int32_t injectedCount = 0;

	for (auto& [mapKey, replacementInfos] : animPathToReplacementsMap) {
		for (auto& info : replacementInfos) {
			if (bundleArr.size >= bundleArr.capacity()) {
				logger::error("[OAR] animationNames at capacity limit ({}), skipping", bundleArr.size);
				continue;
			}

			int16_t newIndex = static_cast<int16_t>(bundleArr.size);
			auto& newEntry = bundleArr.data[bundleArr.size];
			newEntry.fileName.stringAndFlag = _strdup(info.replacementPath.c_str());
			newEntry.meshName.stringAndFlag = "*";
			bundleArr.size++;

			auto replacement = std::make_unique<ReplacementAnimation>(
				info.originalPath,
				info.replacementPath,
				newIndex,
				info.parentSubMod
			);
			info.parentSubMod->AddReplacementAnimation(replacement.get());

			auto& replacements = projectData.GetOrCreateReplacements(static_cast<int16_t>(0));
			replacements.AddReplacement(replacement.get());
			projectData.SetOriginalIndex(newIndex, static_cast<int16_t>(0));

			AddOwnedAnimation(std::move(replacement));
			injectedCount++;

			logger::info("[OAR]   Injected '{}' at index {} (original: '{}')",
				info.replacementPath, newIndex, info.originalPath);
		}
	}

	logger::info("[OAR] CreateReplacementAnimations done: {} -> {} entries ({} injected)",
		originalCount, bundleArr.size, injectedCount);
	return injectedCount > 0;
}

ReplacerProjectData* OpenAnimationReplacer::GetReplacerProjectData(RE::hkbCharacterStringData* a_stringData)
{
	if (!a_stringData) return nullptr;
	ReadLocker lock(projectDataMutex);
	auto it = projectDataMap.find(a_stringData);
	return it != projectDataMap.end() ? it->second.get() : nullptr;
}

ReplacerProjectData& OpenAnimationReplacer::GetOrCreateReplacerProjectData(RE::hkbCharacterStringData* a_stringData)
{
	WriteLocker lock(projectDataMutex);
	auto& ptr = projectDataMap[a_stringData];
	if (!ptr) {
		ptr = std::make_unique<ReplacerProjectData>();
	}
	return *ptr;
}

void OpenAnimationReplacer::AddOwnedAnimation(std::unique_ptr<ReplacementAnimation> a_anim)
{
	WriteLocker lock(ownedAnimsMutex);
	ownedAnimations.push_back(std::move(a_anim));
}

size_t OpenAnimationReplacer::GetTotalReplacementCount() const
{
	ReadLocker lock(modsMutex);
	size_t count = 0;
	for (const auto& mod : replacerMods) {
		for (const auto& sub : mod->GetSubMods()) {
			count += sub->GetReplacementAnimations().size();
		}
	}
	return count;
}
