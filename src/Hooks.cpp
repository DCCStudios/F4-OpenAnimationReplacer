#include "Hooks.h"
#include "Offsets.h"
#include "HavokTypes.h"
#include "Settings.h"
#include "ActiveClip.h"
#include "OpenAnimationReplacer.h"
#include "AnimationCache.h"
#include <set>
#include <unordered_map>

static std::atomic<bool> s_gameFullyLoaded{ false };
static std::atomic<bool> s_hasActiveReplacements{ false };

void SetGameFullyLoaded(bool a_loaded) { s_gameFullyLoaded.store(a_loaded); }
void SetHasActiveReplacements(bool a_has) { s_hasActiveReplacements.store(a_has); }
bool HasActiveReplacements() { return s_hasActiveReplacements.load(); }

static std::shared_mutex s_characterCacheMutex;
static std::unordered_map<RE::hkbCharacter*, RE::TESObjectREFR*> s_characterCache;
static std::unordered_set<RE::hkbCharacter*> s_mainBodyCharacters;

static RE::hkbCharacterStringData* s_capturedStringData{ nullptr };
static std::string s_capturedAnimPath;
static std::mutex s_capturedMutex;
static std::vector<void*> s_capturedGraphs;
static std::mutex s_capturedGraphsMutex;

void RegisterActorCharacter(RE::TESObjectREFR* a_refr)
{
	if (!a_refr) return;
	RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
	if (!a_refr->GetAnimationGraphManagerImpl(manager) || !manager) return;
	std::unique_lock lock(s_characterCacheMutex);
	for (uint32_t i = 0; i < manager->graph.size(); i++) {
		auto* character = &manager->graph[i]->character;
		s_characterCache[character] = a_refr;
		if (i == 0) {
			s_mainBodyCharacters.insert(character);
		}
	}
}

void ClearCharacterCache()
{
	std::unique_lock lock(s_characterCacheMutex);
	s_characterCache.clear();
	s_mainBodyCharacters.clear();
}

namespace
{
	RE::TESObjectREFR* GetRefrFromContext(const RE::hkbContext* a_context)
	{
		if (!a_context) return nullptr;
		auto* character = a_context->character;
		if (!character) return nullptr;

		std::shared_lock lock(s_characterCacheMutex);

		if (s_mainBodyCharacters.find(character) == s_mainBodyCharacters.end())
			return nullptr;

		auto it = s_characterCache.find(character);
		if (it != s_characterCache.end()) {
			auto* refr = it->second;
			if (refr && refr->As<RE::Actor>()) return refr;
		}

		return nullptr;
	}

	static std::shared_mutex s_nameLookupMutex;
	static std::unordered_map<std::string, std::string> s_suffixToReplacementPath;
	static std::vector<std::unique_ptr<std::string>> s_persistentStrings;
	static bool s_lookupBuilt = false;

	static std::string ExtractAnimSuffix(const std::string& a_path)
	{
		auto lower = a_path;
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

	static void BuildNameLookup()
	{
		std::unique_lock lock(s_nameLookupMutex);
		if (s_lookupBuilt) return;

		auto* oar = OpenAnimationReplacer::GetSingleton();
		const auto& pathMap = oar->GetPathToSubModsMap();

		for (auto& [mapKey, replacementInfos] : pathMap) {
			auto suffix = ExtractAnimSuffix(mapKey);
			if (suffix.empty()) continue;

			for (auto& info : replacementInfos) {
				s_suffixToReplacementPath[suffix] = info.replacementPath;
				logger::info("[OAR] NameLookup: suffix='{}' -> '{}'", suffix, info.replacementPath);
				break;
			}
		}

		s_lookupBuilt = true;
		logger::info("[OAR] Built name lookup with {} entries", s_suffixToReplacementPath.size());
	}

	static void PreloadReplacementAnimations()
	{
		auto* oar = OpenAnimationReplacer::GetSingleton();
		auto* cache = AnimationCache::GetSingleton();
		const auto& pathMap = oar->GetPathToSubModsMap();

		int loaded = 0;
		int failed = 0;

		for (auto& [mapKey, replacementInfos] : pathMap) {
			auto suffix = ExtractAnimSuffix(mapKey);
			if (suffix.empty()) continue;

			for (auto& info : replacementInfos) {
				if (info.absoluteDiskPath.empty()) {
					logger::warn("[OAR-Preload] No absolute path for suffix '{}'", suffix);
					failed++;
					continue;
				}

				if (cache->LoadAnimation(suffix, info.absoluteDiskPath)) {
					loaded++;
				} else {
					failed++;
				}
				break;
			}
		}

		logger::info("[OAR-Preload] Pre-loaded {} animations ({} failed), cache size: {}",
			loaded, failed, cache->GetCacheSize());
	}

	void hkbClipGenerator_Activate(RE::hkbClipGenerator* a_this, const RE::hkbContext* a_context)
	{
		Hooks::ClipGeneratorHooks::_Activate(a_this, a_context);

		if (!s_gameFullyLoaded.load() || !s_hasActiveReplacements.load() || !a_this) {
			return;
		}

		if (!s_lookupBuilt) BuildNameLookup();

		const char* clipName = a_this->animationName.data();
		if (!clipName || reinterpret_cast<uintptr_t>(clipName) < 0x10000) {
			return;
		}

		auto* cache = AnimationCache::GetSingleton();

		auto* currentAnim = a_this->GetAnimation();
		if (currentAnim && cache->GetGameAnimVtable() == 0) {
			auto vtbl = *reinterpret_cast<uintptr_t*>(currentAnim);
			cache->SetVtableFromGame(vtbl);
			logger::info("[OAR] Captured game hkaAnimation vtable: {:X}", vtbl);
		}

		auto suffix = ExtractAnimSuffix(std::string(clipName));

		std::shared_lock lock(s_nameLookupMutex);
		auto it = s_suffixToReplacementPath.find(suffix);
		if (it == s_suffixToReplacementPath.end()) {
			lock.unlock();
			return;
		}
		lock.unlock();

		auto* replacement = cache->GetCachedAnimation(suffix);
		if (!replacement) return;

		auto repVtbl = *reinterpret_cast<uintptr_t*>(replacement);
		if (repVtbl < 0x7FF000000000ull || repVtbl > 0x7FFF00000000ull) return;

		auto** animSlot = a_this->GetAnimationSlot();
		if (!animSlot || !*animSlot) return;

		static int s_matchLog = 0;
		if (s_matchLog < 20) {
			logger::info("[OAR] ClipGen Activate (SWAP): clip='{}' suffix='{}'", clipName, suffix);
			s_matchLog++;
		}

		*animSlot = replacement;
	}

	void hkbClipGenerator_Update(RE::hkbClipGenerator* a_this, const RE::hkbContext* a_context, float a_timestep)
	{
		Hooks::ClipGeneratorHooks::_Update(a_this, a_context, a_timestep);

		if (!s_gameFullyLoaded.load() || !s_hasActiveReplacements.load() || !a_this) return;

		auto* cache = AnimationCache::GetSingleton();

		auto* currentAnim = a_this->GetAnimation();
		if (currentAnim && cache->GetGameAnimVtable() == 0) {
			auto vtbl = *reinterpret_cast<uintptr_t*>(currentAnim);
			cache->SetVtableFromGame(vtbl);
			logger::info("[OAR] Captured game hkaAnimation vtable: {:X} (from Update)", vtbl);
		}

		if (!s_lookupBuilt) return;

		const char* clipName = a_this->animationName.data();
		if (!clipName || reinterpret_cast<uintptr_t>(clipName) < 0x10000) return;

		auto suffix = ExtractAnimSuffix(std::string(clipName));

		std::shared_lock lock(s_nameLookupMutex);
		auto it = s_suffixToReplacementPath.find(suffix);
		if (it == s_suffixToReplacementPath.end()) return;
		lock.unlock();

		auto** animSlot = a_this->GetAnimationSlot();
		if (!animSlot || !*animSlot) return;

		auto* gameAnim = *animSlot;

		// Build (or get cached) runtime clone: game struct layout + our data
		auto* replacement = cache->GetOrBuildRuntimeAnim(suffix, gameAnim);
		if (!replacement) {
			// Fallback to direct parsed animation (old path)
			replacement = cache->GetCachedAnimation(suffix);
		}
		if (!replacement) return;

		// Validate vtable
		auto repVtbl = *reinterpret_cast<uintptr_t*>(replacement);
		if (repVtbl < 0x7FF000000000ull || repVtbl > 0x7FFF00000000ull) return;

		// Validate critical data pointer
		auto* repBytes = reinterpret_cast<uint8_t*>(replacement);
		auto ptrData = *reinterpret_cast<uintptr_t*>(repBytes + 0x98);
		if (ptrData == 0) {
			static int s_nullLog = 0;
			if (s_nullLog < 5) {
				logger::error("[OAR] BLOCKED SWAP: replacement has NULL m_data! +0x98={:X}", ptrData);
				s_nullLog++;
			}
			return;
		}

		if (*animSlot == replacement) return;

		// Extensive diagnostic logging to verify struct layout correctness
		static int s_swapLog = 0;
		if (s_swapLog < 3) {
			auto* origBytes = reinterpret_cast<uint8_t*>(*animSlot);
			logger::info("[OAR] Swapping clip '{}' (suffix='{}') slot={:X} -> {:X}",
				clipName, suffix,
				reinterpret_cast<uintptr_t>(*animSlot),
				reinterpret_cast<uintptr_t>(replacement));

			// Dump scalar fields from GAME animation to verify layout
			logger::info("[OAR]   GAME scalars: +0x10(type)={} +0x14(dur)={:.3f} +0x18(trk)={} +0x1C(flt)={}",
				*reinterpret_cast<int32_t*>(origBytes + 0x10),
				*reinterpret_cast<float*>(origBytes + 0x14),
				*reinterpret_cast<int32_t*>(origBytes + 0x18),
				*reinterpret_cast<int32_t*>(origBytes + 0x1C));
			logger::info("[OAR]   GAME scalars: +0x38(frames)={} +0x3C(blocks)={} +0x44(maskSz)={}",
				*reinterpret_cast<int32_t*>(origBytes + 0x38),
				*reinterpret_cast<int32_t*>(origBytes + 0x3C),
				*reinterpret_cast<int32_t*>(origBytes + 0x44));
			logger::info("[OAR]   GAME ptrs: +0x58={:X} +0x68={:X} +0x78={:X} +0x88={:X} +0x98={:X}",
				*reinterpret_cast<uintptr_t*>(origBytes + 0x58),
				*reinterpret_cast<uintptr_t*>(origBytes + 0x68),
				*reinterpret_cast<uintptr_t*>(origBytes + 0x78),
				*reinterpret_cast<uintptr_t*>(origBytes + 0x88),
				*reinterpret_cast<uintptr_t*>(origBytes + 0x98));
			logger::info("[OAR]   GAME sizes: +0x60(blkSz)={} +0x70(fblkSz)={} +0x80(xfSz)={} +0x90(flSz)={} +0xA0(dataSz)={}",
				*reinterpret_cast<int32_t*>(origBytes + 0x60),
				*reinterpret_cast<int32_t*>(origBytes + 0x70),
				*reinterpret_cast<int32_t*>(origBytes + 0x80),
				*reinterpret_cast<int32_t*>(origBytes + 0x90),
				*reinterpret_cast<int32_t*>(origBytes + 0xA0));

			// Dump scalar fields from OUR animation
			logger::info("[OAR]   OURS scalars: +0x10(type)={} +0x14(dur)={:.3f} +0x18(trk)={} +0x1C(flt)={}",
				*reinterpret_cast<int32_t*>(repBytes + 0x10),
				*reinterpret_cast<float*>(repBytes + 0x14),
				*reinterpret_cast<int32_t*>(repBytes + 0x18),
				*reinterpret_cast<int32_t*>(repBytes + 0x1C));
			logger::info("[OAR]   OURS scalars: +0x38(frames)={} +0x3C(blocks)={} +0x44(maskSz)={}",
				*reinterpret_cast<int32_t*>(repBytes + 0x38),
				*reinterpret_cast<int32_t*>(repBytes + 0x3C),
				*reinterpret_cast<int32_t*>(repBytes + 0x44));
			logger::info("[OAR]   OURS ptrs: +0x58={:X} +0x68={:X} +0x78={:X} +0x88={:X} +0x98={:X}",
				*reinterpret_cast<uintptr_t*>(repBytes + 0x58),
				*reinterpret_cast<uintptr_t*>(repBytes + 0x68),
				*reinterpret_cast<uintptr_t*>(repBytes + 0x78),
				*reinterpret_cast<uintptr_t*>(repBytes + 0x88),
				*reinterpret_cast<uintptr_t*>(repBytes + 0x98));
			logger::info("[OAR]   OURS sizes: +0x60(blkSz)={} +0x70(fblkSz)={} +0x80(xfSz)={} +0x90(flSz)={} +0xA0(dataSz)={}",
				*reinterpret_cast<int32_t*>(repBytes + 0x60),
				*reinterpret_cast<int32_t*>(repBytes + 0x70),
				*reinterpret_cast<int32_t*>(repBytes + 0x80),
				*reinterpret_cast<int32_t*>(repBytes + 0x90),
				*reinterpret_cast<int32_t*>(repBytes + 0xA0));

			// Verify actual data at pointer targets
			auto* blkOff = *reinterpret_cast<uint32_t**>(repBytes + 0x58);
			auto* dataP = *reinterpret_cast<uint8_t**>(repBytes + 0x98);
			if (blkOff) {
				logger::info("[OAR]   OURS blockOffsets[0]={}", blkOff[0]);
			}
			if (dataP) {
				logger::info("[OAR]   OURS data first 16 bytes: {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}",
					dataP[0], dataP[1], dataP[2], dataP[3], dataP[4], dataP[5], dataP[6], dataP[7],
					dataP[8], dataP[9], dataP[10], dataP[11], dataP[12], dataP[13], dataP[14], dataP[15]);
			}

			// Also check GAME's blockOffsets[0] and data bytes for comparison
			auto* gBlkOff = *reinterpret_cast<uint32_t**>(origBytes + 0x58);
			auto* gDataP = *reinterpret_cast<uint8_t**>(origBytes + 0x98);
			if (gBlkOff) {
				logger::info("[OAR]   GAME blockOffsets[0]={}", gBlkOff[0]);
			}
			if (gDataP) {
				logger::info("[OAR]   GAME data first 16 bytes: {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}",
					gDataP[0], gDataP[1], gDataP[2], gDataP[3], gDataP[4], gDataP[5], gDataP[6], gDataP[7],
					gDataP[8], gDataP[9], gDataP[10], gDataP[11], gDataP[12], gDataP[13], gDataP[14], gDataP[15]);
			}

			s_swapLog++;
		}

		*animSlot = replacement;
	}

	void hkbClipGenerator_Deactivate(RE::hkbClipGenerator* a_this, const RE::hkbContext* a_context)
	{
		Hooks::ClipGeneratorHooks::_Deactivate(a_this, a_context);
	}

	void hkbClipGenerator_Generate(RE::hkbClipGenerator* a_this, const RE::hkbContext* a_context,
		const RE::hkbGeneratorOutput** a_activeChildrenOutput, RE::hkbGeneratorOutput& a_output, float a_timeOffset)
	{
		Hooks::ClipGeneratorHooks::_Generate(a_this, a_context, a_activeChildrenOutput, a_output, a_timeOffset);
	}

	void hkbClipGenerator_StartEcho(RE::hkbClipGenerator* a_this, float a_duration)
	{
		Hooks::ClipGeneratorHooks::_StartEcho(a_this, a_duration);
	}

	void HookedActorUpdate()
	{
		Hooks::UpdateHooks::RunActorUpdatesOrig();
	}

	RE::hkbCharacterStringData* ExtractStringDataFromGraph(void* a_firstArg)
	{
		auto* graphBytes = reinterpret_cast<uint8_t*>(a_firstArg);

		auto* charPtr = reinterpret_cast<RE::hkbCharacter*>(graphBytes + 0x1C8);
		if (IsBadReadPtr(charPtr, sizeof(void*))) {
			logger::warn("[OAR]   ExtractSD: character at graph+0x1C8 unreadable");
			return nullptr;
		}

		auto* setup = charPtr->setup._ptr;
		if (!setup) {
			logger::info("[OAR]   ExtractSD: character.setup is null (graph may be initializing)");
			return nullptr;
		}
		if (IsBadReadPtr(setup, sizeof(void*))) {
			logger::warn("[OAR]   ExtractSD: character.setup unreadable");
			return nullptr;
		}

		auto* typedSetup = reinterpret_cast<RE::hkbCharacterSetup*>(setup);
		if (!typedSetup->data._ptr) {
			logger::info("[OAR]   ExtractSD: setup.data is null");
			return nullptr;
		}
		if (IsBadReadPtr(typedSetup->data._ptr, sizeof(void*))) {
			logger::warn("[OAR]   ExtractSD: setup.data unreadable");
			return nullptr;
		}

		auto* stringData = typedSetup->data._ptr->stringData._ptr;
		if (!stringData) {
			logger::info("[OAR]   ExtractSD: data.stringData is null");
			return nullptr;
		}
		if (IsBadReadPtr(stringData, sizeof(uintptr_t))) {
			logger::warn("[OAR]   ExtractSD: stringData unreadable");
			return nullptr;
		}

		uintptr_t sdVtbl = *reinterpret_cast<uintptr_t*>(stringData);
		if (sdVtbl != Offsets::hkbCharacterStringData_vtbl.address()) {
			logger::warn("[OAR]   ExtractSD: stringData vtable mismatch ({:X} vs {:X})",
				sdVtbl, Offsets::hkbCharacterStringData_vtbl.address());
			return nullptr;
		}

		return stringData;
	}

	const char* SafeStr(const char* p)
	{
		if (!p) return "(null)";
		if (reinterpret_cast<uintptr_t>(p) < 0x10000) return "(invalid-ptr)";
		if (IsBadReadPtr(p, 1)) return "(unreadable)";
		return p;
	}

	void ProcessStringData(const char* a_hookName, RE::hkbCharacterStringData* a_stringData,
		const char* a_animationPath, bool& a_injected)
	{
		const char* safePath = SafeStr(a_animationPath);
		logger::info("[OAR] {}: valid stringData at {:X}, animPath='{}'",
			a_hookName, reinterpret_cast<uintptr_t>(a_stringData), safePath);

		auto* oar = OpenAnimationReplacer::GetSingleton();
		if (oar->GetTotalReplacementCount() > 0) {
			try {
				a_injected = oar->CreateReplacementAnimations(safePath, a_stringData);
			} catch (const std::exception& e) {
				logger::error("[OAR] Exception in CreateReplacementAnimations: {}", e.what());
			} catch (...) {
				logger::error("[OAR] Unknown exception in CreateReplacementAnimations");
			}
		} else {
			std::lock_guard lock(s_capturedMutex);
			s_capturedStringData = a_stringData;
			s_capturedAnimPath = (safePath && safePath[0] != '(') ? safePath : "";
			logger::info("[OAR] {}: no replacements yet, capturing stringData for deferred injection",
				a_hookName);
		}
	}

	void HandleLoadClipsCommon(const char* a_hookName,
		void* a_firstArg, const char* a_animationPath,
		bool& a_injected)
	{
		if (!a_firstArg || IsBadReadPtr(a_firstArg, sizeof(uintptr_t))) return;

		uintptr_t vtbl = *reinterpret_cast<uintptr_t*>(a_firstArg);

		if (vtbl == Offsets::hkbCharacterStringData_vtbl.address()) {
			auto* stringData = reinterpret_cast<RE::hkbCharacterStringData*>(a_firstArg);
			ProcessStringData(a_hookName, stringData, a_animationPath, a_injected);
			return;
		}

		static uintptr_t bsGraphVtbl = REL::Relocation<uintptr_t>{ REL::ID(742655) }.address();
		static uintptr_t bindingSetVtbl = REL::Relocation<uintptr_t>{ REL::ID(802975) }.address();

		if (vtbl == bsGraphVtbl) {
			logger::info("[OAR] {}: received BShkbAnimationGraph at {:X}, extracting stringData...",
				a_hookName, reinterpret_cast<uintptr_t>(a_firstArg));

			auto* stringData = ExtractStringDataFromGraph(a_firstArg);
			if (stringData) {
				ProcessStringData(a_hookName, stringData, a_animationPath, a_injected);
			} else {
				std::lock_guard lock(s_capturedGraphsMutex);
				s_capturedGraphs.push_back(a_firstArg);
				logger::info("[OAR] {}: stringData not ready, captured graph for deferred extraction ({} total)",
					a_hookName, s_capturedGraphs.size());
			}
			return;
		}

		if (vtbl == bindingSetVtbl) {
			logger::info("[OAR] {}: received hkbAnimationBindingSet at {:X} (not extractable yet)",
				a_hookName, reinterpret_cast<uintptr_t>(a_firstArg));
			return;
		}

		if (Settings::GetSingleton()->bVerboseLogging) {
			logger::warn("[OAR] {}: unknown first arg vtable {:X}",
				a_hookName, vtbl);
		}
	}

	void LogAllArgs(const char* hookName, void* a1, void* a2, void* a3, void* a4, const char* a5, void* a6)
	{
		auto safeVtbl = [](void* p) -> uintptr_t {
			if (!p || IsBadReadPtr(p, sizeof(uintptr_t))) return 0;
			return *reinterpret_cast<uintptr_t*>(p);
		};

		logger::info("[OAR] {} args:", hookName);
		logger::info("[OAR]   arg1={:X} vtbl={:X}", (uintptr_t)a1, safeVtbl(a1));
		logger::info("[OAR]   arg2={:X} vtbl={:X}", (uintptr_t)a2, safeVtbl(a2));
		logger::info("[OAR]   arg3={:X} vtbl={:X}", (uintptr_t)a3, safeVtbl(a3));
		logger::info("[OAR]   arg4={:X} vtbl={:X}", (uintptr_t)a4, safeVtbl(a4));
		if (a5 && !IsBadReadPtr(a5, 1))
			logger::info("[OAR]   arg5(path)='{}'", a5);
		else
			logger::info("[OAR]   arg5={:X}", (uintptr_t)a5);
		logger::info("[OAR]   arg6={:X}", (uintptr_t)a6);
	}

	RE::hkbCharacterStringData* ScanForStringData(void* obj, size_t scanBytes)
	{
		if (!obj || IsBadReadPtr(obj, scanBytes)) return nullptr;
		uintptr_t sdVtbl = Offsets::hkbCharacterStringData_vtbl.address();
		auto* bytes = reinterpret_cast<uintptr_t*>(obj);
		size_t count = scanBytes / sizeof(uintptr_t);

		for (size_t i = 0; i < count; i++) {
			if (IsBadReadPtr(&bytes[i], sizeof(uintptr_t))) break;
			uintptr_t val = bytes[i];
			if (!val || val < 0x10000) continue;
			if (IsBadReadPtr(reinterpret_cast<void*>(val), sizeof(uintptr_t))) continue;
			uintptr_t candidateVtbl = *reinterpret_cast<uintptr_t*>(val);
			if (candidateVtbl == sdVtbl) {
				logger::info("[OAR]   ScanForStringData: found at offset +0x{:X} (ptr={:X})",
					i * sizeof(uintptr_t), val);
				return reinterpret_cast<RE::hkbCharacterStringData*>(val);
			}
		}
		return nullptr;
	}

	void HookedLoadClips(RE::hkbCharacterStringData* a_stringData, void* a_bindingSet,
		void* a_assetLoader, RE::hkbBehaviorGraph* a_rootBehavior,
		const char* a_animationPath, void* a_annotationMap)
	{
		static bool s_logged = false;
		if (!s_logged) {
			LogAllArgs("HookedLoadClips[1]", a_stringData, a_bindingSet, a_assetLoader,
				a_rootBehavior, a_animationPath, a_annotationMap);
			s_logged = true;
		}

		Hooks::LoadClipsHooks::_LoadClips(a_stringData, a_bindingSet, a_assetLoader,
			a_rootBehavior, a_animationPath, a_annotationMap);
	}

	void HookedLoadClips2(RE::hkbCharacterStringData* a_stringData, void* a_bindingSet,
		void* a_assetLoader, RE::hkbBehaviorGraph* a_rootBehavior,
		const char* a_animationPath, void* a_annotationMap)
	{
		static bool s_logged = false;
		if (!s_logged) {
			LogAllArgs("HookedLoadClips[2]", a_stringData, a_bindingSet, a_assetLoader,
				a_rootBehavior, a_animationPath, a_annotationMap);
			s_logged = true;
		}

		Hooks::LoadClipsHooks::_LoadClips2(a_stringData, a_bindingSet, a_assetLoader,
			a_rootBehavior, a_animationPath, a_annotationMap);
	}
}

namespace Hooks
{
	void Install()
	{
		ClipGeneratorHooks::Install();
		LoadClipsHooks::Install();
		EnginePatchHooks::Install();
		PreloadHooks::Install();
		UpdateHooks::Install();
		FileRedirectHooks::Install();
		logger::info("[OAR] All hooks installed");
	}

	namespace ClipGeneratorHooks
	{
		void Install()
		{
			auto& vtbl = Offsets::hkbClipGenerator_vtbl;

			_Activate   = reinterpret_cast<ActivateFn>(vtbl.write_vfunc(Offsets::ClipGen_Activate, hkbClipGenerator_Activate));
			_Update     = reinterpret_cast<UpdateFn>(vtbl.write_vfunc(Offsets::ClipGen_Update, hkbClipGenerator_Update));
			_Deactivate = reinterpret_cast<DeactivateFn>(vtbl.write_vfunc(Offsets::ClipGen_Deactivate, hkbClipGenerator_Deactivate));
			_Generate   = reinterpret_cast<GenerateFn>(vtbl.write_vfunc(Offsets::ClipGen_Generate, hkbClipGenerator_Generate));
			_StartEcho  = reinterpret_cast<StartEchoFn>(vtbl.write_vfunc(Offsets::ClipGen_StartEcho, hkbClipGenerator_StartEcho));

			logger::info("[OAR] hkbClipGenerator vtable hooks installed (active mode)");
		}
	}

	namespace LoadClipsHooks
	{
		static bool FuncReferencesAddress(uint8_t* funcStart, int scanLen, uintptr_t funcAddr, uintptr_t targetAddr)
		{
			for (int i = 0; i < scanLen - 6; i++) {
				uint8_t prefix = funcStart[i];
				if (prefix != 0x48 && prefix != 0x4C) continue;

				uint8_t next = funcStart[i + 1];
				if (next != 0x8D && next != 0x8B && next != 0x89) continue;

				uint8_t modrm = funcStart[i + 2];
				if ((modrm & 0xC7) != 0x05) continue;

				int32_t disp = *reinterpret_cast<int32_t*>(funcStart + i + 3);
				uintptr_t resolved = funcAddr + i + 7 + disp;
				if (resolved == targetAddr) return true;
			}
			return false;
		}

		static int CountCallArgRegisters(uint8_t* funcStart, int scanLen)
		{
			int regSetupCount = 0;
			for (int i = 0; i < std::min(scanLen, 64); i++) {
				uint8_t b = funcStart[i];
				if (b == 0x48 || b == 0x4C || b == 0x49) {
					uint8_t next = funcStart[i + 1];
					if (next == 0x89 || next == 0x8B || next == 0x8D) {
						regSetupCount++;
					}
				}
			}
			return regSetupCount;
		}

		void Install()
		{
			auto moduleBase = REL::Module::get().base();
			auto textSeg = REL::Module::get().segment(REL::Segment::Name::text);
			auto textEnd = textSeg.address() + textSeg.size();

			auto* settings = Settings::GetSingleton();
			auto bsGraphVtblBase = REL::Relocation<uintptr_t>{ REL::ID(742655) }.address();
			auto bindingSetVtbl = REL::Relocation<uintptr_t>{ REL::ID(802975) }.address();
			auto stringDataVtbl = Offsets::hkbCharacterStringData_vtbl.address();

			logger::info("[OAR] Searching for loadClips call sites...");
			logger::info("[OAR]   moduleBase={:X}, BindingSet vtbl={:X}, StringData vtbl={:X}",
				moduleBase, bindingSetVtbl, stringDataVtbl);

			struct Candidate {
				uintptr_t callSite;
				uintptr_t target;
				int vtblIdx;
				int callOffset;
				int score;
			};
			std::vector<Candidate> candidates;

			for (int vi = 0; vi < 40; vi++) {
				auto* vtblEntry = reinterpret_cast<uintptr_t*>(bsGraphVtblBase + vi * 8);
				if (IsBadReadPtr(vtblEntry, 8)) break;
				auto funcAddr = *vtblEntry;
				if (funcAddr < moduleBase || funcAddr >= textEnd) continue;

				auto* funcBytes = reinterpret_cast<uint8_t*>(funcAddr);
				constexpr int funcLimit = 4096;

				for (int off = 0; off < funcLimit; off++) {
					if (IsBadReadPtr(funcBytes + off, 5)) break;
					if (funcBytes[off] != 0xE8) continue;

					int32_t rel = *reinterpret_cast<int32_t*>(funcBytes + off + 1);
					uintptr_t target = funcAddr + off + 5 + rel;
					if (target < moduleBase || target >= textEnd) continue;

					auto* targetBytes = reinterpret_cast<uint8_t*>(target);
					if (IsBadReadPtr(targetBytes, 2048)) continue;

					bool refsBindingSet = FuncReferencesAddress(targetBytes, 2048, target, bindingSetVtbl);
					bool refsStringData = FuncReferencesAddress(targetBytes, 2048, target, stringDataVtbl);

					if (!refsBindingSet && !refsStringData) continue;

					int score = 0;
					if (refsBindingSet) score += 10;
					if (refsStringData) score += 5;

					int regCount = CountCallArgRegisters(funcBytes + std::max(off - 32, 0), 32);
					if (regCount >= 4) score += 5;

					if (settings->bVerboseLogging) {
						logger::info("[OAR]   Candidate: vtbl[{}]+0x{:X} -> {:X} (rva {:X}) score={} {}{}",
							vi, off, target, target - moduleBase, score,
							refsBindingSet ? "[BindingSet] " : "",
							refsStringData ? "[StringData]" : "");
					}

					candidates.push_back({ funcAddr + off, target, vi, off, score });
				}
			}

			std::ranges::sort(candidates, [](const auto& a, const auto& b) {
				return a.score > b.score;
			});

			std::set<uintptr_t> hookedTargets;
			auto& trampoline = F4SE::GetTrampoline();
			int hookCount = 0;

			for (auto& c : candidates) {
				if (hookedTargets.count(c.target)) continue;
				if (hookCount >= 2) break;

				auto* callSite = reinterpret_cast<uint8_t*>(c.callSite);
				if (IsBadReadPtr(callSite, 5) || *callSite != 0xE8) continue;

				if (hookCount == 0) {
					logger::info("[OAR] Installing loadClips hook #1 at {:X} (target {:X}, rva {:X}) vtbl[{}]+0x{:X}",
						c.callSite, c.target, c.callSite - moduleBase, c.vtblIdx, c.callOffset);
					_LoadClips = reinterpret_cast<LoadClipsFn>(
						trampoline.write_call<5>(c.callSite, reinterpret_cast<uintptr_t>(HookedLoadClips)));
				} else {
					logger::info("[OAR] Installing loadClips hook #2 at {:X} (target {:X}, rva {:X}) vtbl[{}]+0x{:X}",
						c.callSite, c.target, c.callSite - moduleBase, c.vtblIdx, c.callOffset);
					_LoadClips2 = reinterpret_cast<LoadClipsFn>(
						trampoline.write_call<5>(c.callSite, reinterpret_cast<uintptr_t>(HookedLoadClips2)));
				}

				hookedTargets.insert(c.target);
				hookCount++;
				bHookInstalled = true;
			}

			if (hookCount == 0) {
				logger::warn("[OAR] Could not find any loadClips call sites");
				logger::warn("[OAR] Plugin will operate in safe pass-through mode");
			} else {
				logger::info("[OAR] Installed {} loadClips hook(s) covering {} unique target function(s)",
					hookCount, hookedTargets.size());
			}
		}

		bool TryDeferredInjection()
		{
			auto* oar = OpenAnimationReplacer::GetSingleton();
			if (oar->GetTotalReplacementCount() == 0) {
				logger::info("[OAR] TryDeferredInjection: no replacement animations parsed");
				return false;
			}

			logger::info("[OAR] TryDeferredInjection: {} mods loaded, enabling clip hooks for animation swap",
				oar->GetTotalReplacementCount());
			SetHasActiveReplacements(true);

			FileRedirectHooks::BuildFileRedirectMap();

			oar->loadingPhase = "Loading animations...";
			PreloadReplacementAnimations();

			oar->isLoading.store(false);
			oar->loadingComplete.store(true);

			return true;

			RE::hkbCharacterStringData* stringData = nullptr;

			{
				std::lock_guard lock(s_capturedMutex);
				if (s_capturedStringData && !IsBadReadPtr(s_capturedStringData, sizeof(uintptr_t))) {
					uintptr_t vtbl = *reinterpret_cast<uintptr_t*>(s_capturedStringData);
					if (vtbl == Offsets::hkbCharacterStringData_vtbl.address()) {
						stringData = s_capturedStringData;
						logger::info("[OAR] TryDeferredInjection: using directly captured stringData");
					}
				}
				s_capturedStringData = nullptr;
			}

			if (!stringData) {
				std::lock_guard lock(s_capturedGraphsMutex);
				logger::info("[OAR] TryDeferredInjection: trying {} captured graph(s)...",
					s_capturedGraphs.size());

				for (auto* graphPtr : s_capturedGraphs) {
					if (!graphPtr || IsBadReadPtr(graphPtr, sizeof(uintptr_t))) continue;

					uintptr_t vtbl = *reinterpret_cast<uintptr_t*>(graphPtr);
					static uintptr_t bsGraphVtbl = REL::Relocation<uintptr_t>{ REL::ID(742655) }.address();
					if (vtbl != bsGraphVtbl) continue;

					auto* sd = ExtractStringDataFromGraph(graphPtr);
					if (sd) {
						stringData = sd;
						logger::info("[OAR] TryDeferredInjection: extracted stringData from graph at {:X}",
							reinterpret_cast<uintptr_t>(graphPtr));
						break;
					}
				}
				s_capturedGraphs.clear();
			}

			if (!stringData) {
				logger::info("[OAR] TryDeferredInjection: trying player's animation graph...");

				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!player) {
					logger::warn("[OAR] TryDeferredInjection: PlayerCharacter is null");
				} else {
					RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
					if (!player->GetAnimationGraphManagerImpl(manager) || !manager) {
						logger::warn("[OAR] TryDeferredInjection: no animation graph manager");
					} else {
						logger::info("[OAR] TryDeferredInjection: graph count={}", manager->graph.size());
						for (size_t i = 0; i < manager->graph.size(); i++) {
							auto* graph = manager->graph[i].get();
							if (!graph) continue;
					logger::info("[OAR] TryDeferredInjection: graph[{}] at {:X}", i, (uintptr_t)graph);

						// ---- BEGIN MEMORY PROBE ----
						{
							uintptr_t sdVtblTarget = Offsets::hkbCharacterStringData_vtbl.address();
							auto* graphBytes = reinterpret_cast<uint8_t*>(graph);

							auto* charPtr = reinterpret_cast<RE::hkbCharacter*>(graphBytes + 0x1C8);
							if (!IsBadReadPtr(charPtr, sizeof(void*))) {
								logger::info("[OAR] Probe: character at {:X} (graph+0x1C8)", (uintptr_t)charPtr);

								// Scan character object (0x200 bytes)
								logger::info("[OAR] Probe: scanning character object for stringData vtbl {:X}...", sdVtblTarget);
								auto* charBase = reinterpret_cast<uintptr_t*>(charPtr);
								for (size_t ci = 0; ci < 0x200 / sizeof(uintptr_t); ci++) {
									if (IsBadReadPtr(&charBase[ci], sizeof(uintptr_t))) break;
									uintptr_t val = charBase[ci];
									if (!val || val < 0x10000) continue;
									if (IsBadReadPtr(reinterpret_cast<void*>(val), sizeof(uintptr_t))) continue;
									uintptr_t vtblAt = *reinterpret_cast<uintptr_t*>(val);
									bool match = (vtblAt == sdVtblTarget);
									if (match) {
										logger::info("[OAR] Probe: CHAR offset +0x{:X} = ptr {:X} (vtbl={:X}) [MATCH!]",
											ci * sizeof(uintptr_t), val, vtblAt);
									} else {
										logger::info("[OAR] Probe: CHAR offset +0x{:X} = ptr {:X} (vtbl={:X})",
											ci * sizeof(uintptr_t), val, vtblAt);
									}
								}

								auto* setupFromStruct = charPtr->setup._ptr;
								logger::info("[OAR] Probe: setup via struct = {:X}", (uintptr_t)setupFromStruct);

								auto* setupByOffset = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(charPtr) + 0x78);
								logger::info("[OAR] Probe: setup via raw offset +0x78 = {:X}", (uintptr_t)setupByOffset);

								void* setupToScan = setupFromStruct ? (void*)setupFromStruct : setupByOffset;
								if (setupToScan && !IsBadReadPtr(setupToScan, 0x200)) {
									logger::info("[OAR] Probe: scanning setup object at {:X} (0x200 bytes)...", (uintptr_t)setupToScan);
									auto* setupBase = reinterpret_cast<uintptr_t*>(setupToScan);
									for (size_t si = 0; si < 0x200 / sizeof(uintptr_t); si++) {
										if (IsBadReadPtr(&setupBase[si], sizeof(uintptr_t))) break;
										uintptr_t val = setupBase[si];
										if (!val || val < 0x10000) continue;
										if (IsBadReadPtr(reinterpret_cast<void*>(val), sizeof(uintptr_t))) continue;
										uintptr_t vtblAt = *reinterpret_cast<uintptr_t*>(val);
										bool match = (vtblAt == sdVtblTarget);
										if (match) {
											logger::info("[OAR] Probe: SETUP offset +0x{:X} = ptr {:X} (vtbl={:X}) [MATCH!]",
												si * sizeof(uintptr_t), val, vtblAt);
										} else {
											// Two-level deep: check if this object contains a pointer to stringData
											auto* innerBase = reinterpret_cast<uintptr_t*>(val);
											bool innerMatch = false;
											size_t innerLimit = 0x200 / sizeof(uintptr_t);
											if (!IsBadReadPtr(innerBase, 0x200)) {
												for (size_t ii = 0; ii < innerLimit; ii++) {
													if (IsBadReadPtr(&innerBase[ii], sizeof(uintptr_t))) break;
													uintptr_t innerVal = innerBase[ii];
													if (!innerVal || innerVal < 0x10000) continue;
													if (IsBadReadPtr(reinterpret_cast<void*>(innerVal), sizeof(uintptr_t))) continue;
													uintptr_t innerVtbl = *reinterpret_cast<uintptr_t*>(innerVal);
													if (innerVtbl == sdVtblTarget) {
														logger::info("[OAR] Probe: SETUP offset +0x{:X} -> ptr {:X} (vtbl={:X}) -> inner offset +0x{:X} = {:X} (vtbl={:X}) [INNER MATCH!]",
															si * sizeof(uintptr_t), val, vtblAt,
															ii * sizeof(uintptr_t), innerVal, innerVtbl);
														innerMatch = true;
													}
												}
											}
											if (!innerMatch) {
												logger::info("[OAR] Probe: SETUP offset +0x{:X} = ptr {:X} (vtbl={:X})",
													si * sizeof(uintptr_t), val, vtblAt);
											}
										}
									}
								} else if (setupToScan) {
									logger::info("[OAR] Probe: setup at {:X} is not readable for 0x200 bytes", (uintptr_t)setupToScan);
								} else {
									logger::info("[OAR] Probe: setup is null from both struct and raw offset");
								}
							} else {
								logger::info("[OAR] Probe: character at graph+0x1C8 is unreadable");
							}
						}
						// ---- END MEMORY PROBE ----

						auto* sd = ExtractStringDataFromGraph(graph);
							if (sd) {
								stringData = sd;
								logger::info("[OAR] TryDeferredInjection: extracted stringData from player graph[{}]", i);
								break;
							}

							sd = ScanForStringData(graph, 0x400);
							if (sd) {
								stringData = sd;
								logger::info("[OAR] TryDeferredInjection: found stringData via scan of player graph[{}]", i);
								break;
							}
						}
					}
				}
			}

			if (!stringData) {
				logger::warn("[OAR] TryDeferredInjection: could not find stringData from any source");
				return false;
			}

			logger::info("[OAR] TryDeferredInjection: injecting into stringData at {:X}",
				reinterpret_cast<uintptr_t>(stringData));

			try {
				bool injected = oar->CreateReplacementAnimations("", stringData);
				if (injected) {
					SetHasActiveReplacements(true);
					logger::info("[OAR] TryDeferredInjection: replacement animations injected successfully");
				} else {
					logger::warn("[OAR] TryDeferredInjection: CreateReplacementAnimations returned false (no matches)");
				}
				return injected;
			} catch (const std::exception& e) {
				logger::error("[OAR] TryDeferredInjection exception: {}", e.what());
				return false;
			}
		}
	}

	namespace EnginePatchHooks
	{
		void Install()
		{
			logger::info("[OAR] Engine patches:");

			auto moduleBase = REL::Module::get().base();
			auto textSeg = REL::Module::get().segment(REL::Segment::Name::text);
			auto textEnd = textSeg.address() + textSeg.size();

			// movsx→movzx patch on the ORIGINAL Activate function (not our hook)
			// Use the saved original function pointer, not the vtable (which now points to our hook)
			auto originalActivateAddr = reinterpret_cast<uintptr_t>(ClipGeneratorHooks::_Activate);
			if (originalActivateAddr && originalActivateAddr >= moduleBase && originalActivateAddr < textEnd) {
				auto* funcBytes = reinterpret_cast<uint8_t*>(originalActivateAddr);
				bool patched = false;

				for (int off = 0; off < 256; off++) {
					if (IsBadReadPtr(funcBytes + off, 4)) break;

					if (funcBytes[off] == 0x0F && funcBytes[off + 1] == 0xBF) {
						logger::info("[OAR]   Found movsx at original Activate+0x{:X}, patching to movzx", off);
						DWORD oldProtect;
						VirtualProtect(funcBytes + off, 2, PAGE_EXECUTE_READWRITE, &oldProtect);
						funcBytes[off + 1] = 0xB7;
						VirtualProtect(funcBytes + off, 2, oldProtect, &oldProtect);
						patched = true;
						break;
					}
				}
				if (!patched) {
					logger::info("[OAR]   No movsx found in original Activate - may already use unsigned");
				}
			} else {
				logger::info("[OAR]   Skipping movsx patch - original Activate not resolved");
			}

			auto bhkMemVtbl = REL::Relocation<uintptr_t>{ REL::ID(594246) }.address();
			logger::info("[OAR]   bhkThreadMemorySource vtable at {:X}", bhkMemVtbl);

			logger::info("[OAR] Engine patches done");
		}
	}

	namespace PreloadHooks
	{
		void Install()
		{
			logger::info("[OAR] Preload hooks: animation preloading active via LoadClips injection");
		}

		void PreloadReplacementAnimations(RE::BShkbAnimationGraph* a_graph)
		{
			if (!a_graph) return;

			auto* character = &a_graph->character;
			auto* setup = character->setup._ptr;
			if (!setup) return;

			auto* typedSetup = reinterpret_cast<RE::hkbCharacterSetup*>(setup);
			if (!typedSetup->data._ptr) return;

			auto* stringData = typedSetup->data._ptr->stringData._ptr;
			if (!stringData) return;

			auto* projData = OpenAnimationReplacer::GetSingleton()->GetReplacerProjectData(stringData);
			if (!projData) return;

			logger::info("[OAR] Preloading replacement animations for character '{}'",
				character->name.data() ? character->name.data() : "(unknown)");
		}
	}

	namespace UpdateHooks
	{
		void Install()
		{
			auto& trampoline = F4SE::GetTrampoline();
			RunActorUpdatesOrig = reinterpret_cast<RunActorUpdatesFn>(
				trampoline.write_call<5>(
					Offsets::ptr_RunActorUpdates.address() + Offsets::RunActorUpdates_Offset,
					reinterpret_cast<uintptr_t>(HookedActorUpdate)
				)
			);

			logger::info("[OAR] Actor update hook installed");
		}
	}

	namespace FileRedirectHooks
	{
		using CreateFileW_t = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
		static CreateFileW_t s_origCreateFileW{ nullptr };

		static std::shared_mutex s_fileMapMutex;
		static std::unordered_map<std::string, std::string> s_fileRedirectMap;
		static bool s_fileMapBuilt = false;

		void BuildFileRedirectMap()
		{
			std::unique_lock lock(s_fileMapMutex);

			auto* oar = OpenAnimationReplacer::GetSingleton();
			const auto& pathMap = oar->GetPathToSubModsMap();

			s_fileRedirectMap.clear();

			for (auto& [mapKey, replacementInfos] : pathMap) {
				std::string lowerKey = mapKey;
				std::ranges::transform(lowerKey, lowerKey.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				std::ranges::replace(lowerKey, '/', '\\');

				for (auto& info : replacementInfos) {
					s_fileRedirectMap[lowerKey] = info.replacementPath;
					logger::info("[OAR] FileRedirect: '{}' -> '{}'", lowerKey, info.replacementPath);
					break;
				}
			}

			s_fileMapBuilt = true;
			logger::info("[OAR] File redirect map ready with {} entries", s_fileRedirectMap.size());
		}

		static HANDLE WINAPI HookedCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
			LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
			DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
		{
			if (lpFileName && s_fileMapBuilt) {
				int len = WideCharToMultiByte(CP_UTF8, 0, lpFileName, -1, nullptr, 0, nullptr, nullptr);
				if (len > 0 && len < 1024) {
					char narrowBuf[1024];
					WideCharToMultiByte(CP_UTF8, 0, lpFileName, -1, narrowBuf, sizeof(narrowBuf), nullptr, nullptr);

					std::string narrow(narrowBuf);
					std::string lower = narrow;
					std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

					if (lower.find(".hkx") != std::string::npos || lower.find(".hkt") != std::string::npos) {
						std::shared_lock lock(s_fileMapMutex);

						for (auto& [origSuffix, replacePath] : s_fileRedirectMap) {
							if (lower.find(origSuffix) != std::string::npos) {
								logger::info("[OAR] FILE REDIRECT: '{}' -> '{}'", narrow, replacePath);

								std::wstring wideReplace(replacePath.begin(), replacePath.end());
								lock.unlock();
								return s_origCreateFileW(wideReplace.c_str(), dwDesiredAccess, dwShareMode,
									lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
							}
						}

						static int s_animLogCount = 0;
						if (s_animLogCount < 30) {
							logger::info("[OAR] AnimFile open: '{}'", narrow);
							s_animLogCount++;
						}
					}
				}
			}

			return s_origCreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
				lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
		}

		void Install()
		{
			auto* gameModule = GetModuleHandleW(nullptr);
			if (!gameModule) {
				logger::error("[OAR] FileRedirect: failed to get game module");
				return;
			}

			auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(gameModule);
			auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
				reinterpret_cast<uint8_t*>(gameModule) + dosHeader->e_lfanew);
			auto& importDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

			if (!importDir.VirtualAddress) {
				logger::error("[OAR] FileRedirect: no import directory");
				return;
			}

			auto* importDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
				reinterpret_cast<uint8_t*>(gameModule) + importDir.VirtualAddress);

			auto* realCreateFileW = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "CreateFileW");
			bool hooked = false;

			for (; importDesc->Name != 0; importDesc++) {
				auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
					reinterpret_cast<uint8_t*>(gameModule) + importDesc->FirstThunk);

				for (; thunk->u1.Function != 0; thunk++) {
					if (reinterpret_cast<void*>(thunk->u1.Function) == realCreateFileW) {
						s_origCreateFileW = reinterpret_cast<CreateFileW_t>(thunk->u1.Function);

						DWORD oldProtect;
						VirtualProtect(&thunk->u1.Function, sizeof(uintptr_t), PAGE_READWRITE, &oldProtect);
						thunk->u1.Function = reinterpret_cast<uintptr_t>(HookedCreateFileW);
						VirtualProtect(&thunk->u1.Function, sizeof(uintptr_t), oldProtect, &oldProtect);

						hooked = true;
						break;
					}
				}
				if (hooked) break;
			}

			if (hooked) {
				logger::info("[OAR] CreateFileW IAT hook installed (file redirect active)");
			} else {
				s_origCreateFileW = reinterpret_cast<CreateFileW_t>(realCreateFileW);
				logger::warn("[OAR] CreateFileW IAT entry not found, file redirect disabled");
			}
		}
	}
}
