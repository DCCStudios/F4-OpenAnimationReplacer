#include "API/OpenAnimationReplacerAPI.h"
#include "BaseConditions.h"
#include "Hooks.h"
#include "ActiveReplacementTracker.h"
#include "OpenAnimationReplacer.h"
#include "AnimationCache.h"

namespace
{
	// =========================================================================
	// API Result codes (mirrors OAR::Conditions::APIResult in the SDK header)
	// =========================================================================
	enum class APIResult : uint8_t
	{
		OK,
		AlreadyRegistered,
		Invalid,
		Failed
	};

	// Factory function type matching the SDK header's ConditionFactoryFn
	using ConditionFactoryFn = std::unique_ptr<ICondition>(*)();

	// =========================================================================
	// IConditionsAPI — virtual interface matching the SDK header
	// =========================================================================
	// The vtable layout here MUST match OAR::Conditions::IConditionsAPI exactly.
	// External plugins cast the void* return from RequestPluginAPI_Conditions
	// to their SDK header's IConditionsAPI* and call through the vtable.

	class IConditionsAPIInternal
	{
	public:
		virtual ~IConditionsAPIInternal() = default;
		virtual uint32_t GetAPIVersion() const = 0;
		virtual APIResult RegisterCondition(const char* a_name, ConditionFactoryFn a_factory) = 0;
		virtual bool UnregisterCondition(const char* a_name) = 0;
		virtual uint32_t GetRegisteredConditionCount() const = 0;
	};

	// =========================================================================
	// ConditionsAPIImpl — the actual implementation
	// =========================================================================

	class ConditionsAPIImpl : public IConditionsAPIInternal
	{
	public:
		uint32_t GetAPIVersion() const override
		{
			return 2;
		}

		APIResult RegisterCondition(const char* a_name, ConditionFactoryFn a_factory) override
		{
			if (!a_name || !a_factory) {
				logger::error("[OAR-API] RegisterCondition called with null arguments");
				return APIResult::Invalid;
			}

			auto* factory = ConditionFactory::GetSingleton();
			std::string name(a_name);

			if (factory->GetAllFactories().count(name)) {
				logger::warn("[OAR-API] Condition '{}' already registered", name);
				return APIResult::AlreadyRegistered;
			}

			// The external plugin's factory returns std::unique_ptr<OAR::ICondition>.
			// Since OAR::ICondition (in the SDK header) is binary-identical to our
			// internal ICondition (same vtable layout, same MSVC CRT), we can safely
			// store and call the factory directly.
			factory->Register(name, [a_factory]() -> std::unique_ptr<ICondition> {
				return a_factory();
			});

			logger::info("[OAR-API] Registered custom condition: '{}'", name);
			return APIResult::OK;
		}

		bool UnregisterCondition(const char* a_name) override
		{
			if (!a_name) return false;

			auto* factory = ConditionFactory::GetSingleton();
			std::string name(a_name);

			auto& factories = const_cast<std::unordered_map<std::string, ConditionFactory::FactoryFn>&>(
				factory->GetAllFactories());
			auto it = factories.find(name);
			if (it == factories.end()) {
				logger::warn("[OAR-API] Cannot unregister '{}': not found", name);
				return false;
			}

			factories.erase(it);
			logger::info("[OAR-API] Unregistered condition: '{}'", name);
			return true;
		}

		uint32_t GetRegisteredConditionCount() const override
		{
			return static_cast<uint32_t>(ConditionFactory::GetSingleton()->GetAllFactories().size());
		}
	};

	static ConditionsAPIImpl g_conditionsAPI;

	// =========================================================================
	// Clips API — clip/replacement data queries for external plugins
	// =========================================================================
	// The POD structs and the vtable layout here MUST match the redistributable
	// SDK header (OpenAnimationReplacerAPI-Clips.h) exactly. External plugins
	// cast the void* from RequestPluginAPI_Clips to their IClipsAPI*.

	// Mirrors OAR::Clips::ClipInfo
	struct ClipInfoPOD
	{
		uint64_t clipHandle;
		uint32_t actorFormID;
		uint8_t graphIndex;
		uint8_t perspective;
		uint8_t playbackMode;
		uint8_t replacementKind;
		float duration;
		float localTime;
		float playbackSpeed;
		float originalDuration;
		int32_t subModPriority;
		char animationName[128];
		char resolvedPath[260];
		char suffix[128];
		char subModName[128];
		char modName[128];
		char replacementPath[260];
	};

	// Mirrors OAR::Clips::ClipAnnotation
	struct ClipAnnotationPOD
	{
		float time;
		char text[124];
	};

	// Mirrors OAR::Clips::ActiveReplacementInfo
	struct ActiveReplacementInfoPOD
	{
		uint32_t actorFormID;
		bool conditionsPassed;
		char clipSuffix[128];
		char subModName[128];
		char replacementPath[260];
		char originalPath[260];
		char actorName[64];
	};

	// Mirrors OAR::Clips::Stats
	struct StatsPOD
	{
		uint32_t replacerModCount;
		uint32_t subModCount;
		uint32_t replacementAnimCount;
		uint32_t cachedAnimFileCount;
		uint32_t activeReplacementCount;
	};

	// Mirrors OAR::Clips::GraphInfo
	struct GraphInfoPOD
	{
		uint32_t actorFormID;
		uint8_t graphIndex;
		uint8_t isFirstPerson;
		uint8_t isRebuilding;
		uint8_t _pad0;
		uint32_t activeNodeCount;
		uint32_t activeClipCount;
		uint32_t boneCount;
		uint32_t animationNameCount;
		uint32_t eventNameCount;
		char characterName[64];
		char projectAnimationPath[260];
		char behaviorPath[260];
	};

	// Mirrors OAR::Clips::BoneInfo
	struct BoneInfoPOD
	{
		int16_t index;
		int16_t parentIndex;
		char name[92];
	};

	// Mirrors OAR::Clips::NameEntry
	struct NameEntryPOD
	{
		char name[260];
	};

	class IClipsAPIInternal
	{
	public:
		virtual ~IClipsAPIInternal() = default;
		virtual uint32_t GetAPIVersion() const = 0;
		virtual uint32_t GetActorClips(uint32_t a_actorFormID, ClipInfoPOD* a_outBuffer, uint32_t a_maxCount) = 0;
		virtual bool FindClip(uint32_t a_actorFormID, const char* a_nameOrLeaf, ClipInfoPOD* a_out) = 0;
		virtual uint32_t GetClipAnnotations(uint64_t a_clipHandle, ClipAnnotationPOD* a_outBuffer, uint32_t a_maxCount) = 0;
		virtual uint32_t GetActiveReplacements(uint32_t a_actorFormID, ActiveReplacementInfoPOD* a_outBuffer, uint32_t a_maxCount) = 0;
		virtual bool GetStats(StatsPOD* a_out) = 0;
		// v2 additions — appended, never reordered (ABI compatibility with v1 consumers)
		virtual uint32_t GetActorGraphs(uint32_t a_actorFormID, GraphInfoPOD* a_outBuffer, uint32_t a_maxCount) = 0;
		virtual uint32_t GetGraphBones(uint32_t a_actorFormID, uint32_t a_graphIndex, uint32_t a_startIndex, BoneInfoPOD* a_outBuffer, uint32_t a_maxCount) = 0;
		virtual uint32_t GetGraphAnimationNames(uint32_t a_actorFormID, uint32_t a_graphIndex, uint32_t a_startIndex, NameEntryPOD* a_outBuffer, uint32_t a_maxCount) = 0;
		virtual uint32_t GetGraphEventNames(uint32_t a_actorFormID, uint32_t a_graphIndex, uint32_t a_startIndex, NameEntryPOD* a_outBuffer, uint32_t a_maxCount) = 0;
	};

	// Truncating copy into a fixed buffer, always null-terminated.
	template <size_t N>
	static void CopyToBuf(char (&a_dst)[N], const std::string& a_src)
	{
		const size_t n = std::min(a_src.size(), N - 1);
		std::memcpy(a_dst, a_src.data(), n);
		a_dst[n] = '\0';
	}

	// Path leaf (after the last slash), lowercased for case-insensitive compare.
	static std::string LeafLower(std::string_view a_path)
	{
		const auto pos = a_path.find_last_of("\\/");
		std::string leaf{ pos == std::string_view::npos ? a_path : a_path.substr(pos + 1) };
		// Drop a trailing ".hkx" so "wpnfire.hkx" and "wpnfire" both match.
		if (leaf.size() > 4 && _stricmp(leaf.c_str() + leaf.size() - 4, ".hkx") == 0) {
			leaf.resize(leaf.size() - 4);
		}
		std::transform(leaf.begin(), leaf.end(), leaf.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return leaf;
	}

	static RE::TESObjectREFR* ResolveQueryActor(uint32_t a_formID)
	{
		if (a_formID == 0 || a_formID == 0x14) {
			return RE::PlayerCharacter::GetSingleton();
		}
		auto* form = RE::TESForm::GetFormByID(a_formID);
		return form ? form->As<RE::TESObjectREFR>() : nullptr;
	}

	static void FillClipInfoPOD(ClipInfoPOD& a_dst, const OARClipQueryData& a_src)
	{
		a_dst.clipHandle = a_src.clipHandle;
		a_dst.actorFormID = a_src.actorFormID;
		a_dst.graphIndex = a_src.graphIndex;
		a_dst.perspective = a_src.perspective;
		a_dst.playbackMode = a_src.playbackMode;
		a_dst.replacementKind = a_src.replacementKind;
		a_dst.duration = a_src.duration;
		a_dst.localTime = a_src.localTime;
		a_dst.playbackSpeed = a_src.playbackSpeed;
		a_dst.originalDuration = a_src.originalDuration;
		a_dst.subModPriority = a_src.subModPriority;
		CopyToBuf(a_dst.animationName, a_src.animationName);
		CopyToBuf(a_dst.resolvedPath, a_src.resolvedPath);
		CopyToBuf(a_dst.suffix, a_src.suffix);
		CopyToBuf(a_dst.subModName, a_src.subModName);
		CopyToBuf(a_dst.modName, a_src.modName);
		CopyToBuf(a_dst.replacementPath, a_src.replacementPath);
	}

	// Copies a paged window [a_startIndex, a_startIndex + a_maxCount) of a
	// name list into fixed NameEntry buffers. Returns the TOTAL list size.
	static uint32_t CopyNamePage(const std::vector<std::string>& a_names,
		uint32_t a_startIndex, NameEntryPOD* a_outBuffer, uint32_t a_maxCount)
	{
		const auto total = static_cast<uint32_t>(a_names.size());
		if (a_outBuffer && a_maxCount > 0 && a_startIndex < total) {
			const uint32_t n = std::min<uint32_t>(a_maxCount, total - a_startIndex);
			for (uint32_t i = 0; i < n; ++i) {
				CopyToBuf(a_outBuffer[i].name, a_names[a_startIndex + i]);
			}
		}
		return total;
	}

	class ClipsAPIImpl : public IClipsAPIInternal
	{
	public:
		uint32_t GetAPIVersion() const override
		{
			return 2;
		}

		uint32_t GetActorClips(uint32_t a_actorFormID, ClipInfoPOD* a_outBuffer, uint32_t a_maxCount) override
		{
			auto* refr = ResolveQueryActor(a_actorFormID);
			if (!refr) return 0;

			std::vector<OARClipQueryData> clips;
			CollectActorClipQueryData(refr, clips);

			if (a_outBuffer && a_maxCount > 0) {
				const uint32_t n = std::min<uint32_t>(a_maxCount, static_cast<uint32_t>(clips.size()));
				for (uint32_t i = 0; i < n; ++i) {
					FillClipInfoPOD(a_outBuffer[i], clips[i]);
				}
			}
			return static_cast<uint32_t>(clips.size());
		}

		bool FindClip(uint32_t a_actorFormID, const char* a_nameOrLeaf, ClipInfoPOD* a_out) override
		{
			if (!a_nameOrLeaf || !a_out) return false;
			auto* refr = ResolveQueryActor(a_actorFormID);
			if (!refr) return false;

			const std::string wanted = LeafLower(a_nameOrLeaf);
			if (wanted.empty()) return false;

			std::vector<OARClipQueryData> clips;
			CollectActorClipQueryData(refr, clips);

			for (const auto& c : clips) {
				if (LeafLower(c.suffix) == wanted ||
					LeafLower(c.animationName) == wanted ||
					LeafLower(c.resolvedPath) == wanted) {
					FillClipInfoPOD(*a_out, c);
					return true;
				}
			}
			return false;
		}

		uint32_t GetClipAnnotations(uint64_t a_clipHandle, ClipAnnotationPOD* a_outBuffer, uint32_t a_maxCount) override
		{
			std::vector<std::pair<float, std::string>> annots;
			CollectClipAnnotations(static_cast<uintptr_t>(a_clipHandle), annots);

			if (a_outBuffer && a_maxCount > 0) {
				const uint32_t n = std::min<uint32_t>(a_maxCount, static_cast<uint32_t>(annots.size()));
				for (uint32_t i = 0; i < n; ++i) {
					a_outBuffer[i].time = annots[i].first;
					CopyToBuf(a_outBuffer[i].text, annots[i].second);
				}
			}
			return static_cast<uint32_t>(annots.size());
		}

		uint32_t GetActiveReplacements(uint32_t a_actorFormID, ActiveReplacementInfoPOD* a_outBuffer, uint32_t a_maxCount) override
		{
			const auto snapshot = ActiveReplacementTracker::GetSingleton()->GetSnapshot();

			uint32_t total = 0;
			uint32_t written = 0;
			for (const auto& e : snapshot) {
				// a_actorFormID == 0 means "all actors" here (unlike the clip
				// queries, where 0 is a player shorthand — replacements are
				// global state, clips need one concrete graph to walk).
				if (a_actorFormID != 0 && e.actorFormID != a_actorFormID) continue;
				total++;
				if (a_outBuffer && written < a_maxCount) {
					auto& dst = a_outBuffer[written++];
					dst.actorFormID = e.actorFormID;
					dst.conditionsPassed = e.conditionsPassed;
					CopyToBuf(dst.clipSuffix, e.clipSuffix);
					CopyToBuf(dst.subModName, e.subModName);
					CopyToBuf(dst.replacementPath, e.replacementPath);
					CopyToBuf(dst.originalPath, e.fullPath);
					CopyToBuf(dst.actorName, e.actorName);
				}
			}
			return total;
		}

		bool GetStats(StatsPOD* a_out) override
		{
			if (!a_out) return false;

			auto* oar = OpenAnimationReplacer::GetSingleton();
			uint32_t mods = 0;
			uint32_t subMods = 0;
			{
				std::shared_lock lock(oar->GetModsMutex());
				mods = static_cast<uint32_t>(oar->GetReplacerMods().size());
				for (const auto& mod : oar->GetReplacerMods()) {
					subMods += static_cast<uint32_t>(mod->GetSubMods().size());
				}
			}
			a_out->replacerModCount = mods;
			a_out->subModCount = subMods;
			a_out->replacementAnimCount = static_cast<uint32_t>(oar->GetTotalReplacementCount());
			a_out->cachedAnimFileCount = static_cast<uint32_t>(AnimationCache::GetSingleton()->GetCacheSize());
			a_out->activeReplacementCount = static_cast<uint32_t>(ActiveReplacementTracker::GetSingleton()->GetCount());
			return true;
		}

		uint32_t GetActorGraphs(uint32_t a_actorFormID, GraphInfoPOD* a_outBuffer, uint32_t a_maxCount) override
		{
			auto* refr = ResolveQueryActor(a_actorFormID);
			if (!refr) return 0;

			std::vector<OARGraphQueryData> graphs;
			CollectActorGraphQueryData(refr, graphs);

			if (a_outBuffer && a_maxCount > 0) {
				const uint32_t n = std::min<uint32_t>(a_maxCount, static_cast<uint32_t>(graphs.size()));
				for (uint32_t i = 0; i < n; ++i) {
					auto& dst = a_outBuffer[i];
					const auto& src = graphs[i];
					dst.actorFormID = src.actorFormID;
					dst.graphIndex = src.graphIndex;
					dst.isFirstPerson = src.isFirstPerson ? 1 : 0;
					dst.isRebuilding = src.isRebuilding ? 1 : 0;
					dst._pad0 = 0;
					dst.activeNodeCount = src.activeNodeCount;
					dst.activeClipCount = src.activeClipCount;
					dst.boneCount = src.boneCount;
					dst.animationNameCount = src.animationNameCount;
					dst.eventNameCount = src.eventNameCount;
					CopyToBuf(dst.characterName, src.characterName);
					CopyToBuf(dst.projectAnimationPath, src.projectAnimationPath);
					CopyToBuf(dst.behaviorPath, src.behaviorPath);
				}
			}
			return static_cast<uint32_t>(graphs.size());
		}

		uint32_t GetGraphBones(uint32_t a_actorFormID, uint32_t a_graphIndex, uint32_t a_startIndex, BoneInfoPOD* a_outBuffer, uint32_t a_maxCount) override
		{
			auto* refr = ResolveQueryActor(a_actorFormID);
			if (!refr) return 0;

			std::vector<OARBoneQueryData> bones;
			CollectGraphBones(refr, a_graphIndex, bones);

			const auto total = static_cast<uint32_t>(bones.size());
			if (a_outBuffer && a_maxCount > 0 && a_startIndex < total) {
				const uint32_t n = std::min<uint32_t>(a_maxCount, total - a_startIndex);
				for (uint32_t i = 0; i < n; ++i) {
					const auto& src = bones[a_startIndex + i];
					auto& dst = a_outBuffer[i];
					dst.index = src.index;
					dst.parentIndex = src.parentIndex;
					CopyToBuf(dst.name, src.name);
				}
			}
			return total;
		}

		uint32_t GetGraphAnimationNames(uint32_t a_actorFormID, uint32_t a_graphIndex, uint32_t a_startIndex, NameEntryPOD* a_outBuffer, uint32_t a_maxCount) override
		{
			auto* refr = ResolveQueryActor(a_actorFormID);
			if (!refr) return 0;

			std::vector<std::string> names;
			CollectGraphAnimationNames(refr, a_graphIndex, names);
			return CopyNamePage(names, a_startIndex, a_outBuffer, a_maxCount);
		}

		uint32_t GetGraphEventNames(uint32_t a_actorFormID, uint32_t a_graphIndex, uint32_t a_startIndex, NameEntryPOD* a_outBuffer, uint32_t a_maxCount) override
		{
			auto* refr = ResolveQueryActor(a_actorFormID);
			if (!refr) return 0;

			std::vector<std::string> names;
			CollectGraphEventNames(refr, a_graphIndex, names);
			return CopyNamePage(names, a_startIndex, a_outBuffer, a_maxCount);
		}
	};

	static ClipsAPIImpl g_clipsAPI;
}

// =============================================================================
// DLL export — external plugins call GetProcAddress for this symbol
// =============================================================================

extern "C" OAR_API void* RequestPluginAPI_Conditions()
{
	logger::info("[OAR-API] Conditions API requested (version 2)");
	return static_cast<IConditionsAPIInternal*>(&g_conditionsAPI);
}

extern "C" OAR_API void* RequestPluginAPI_Clips()
{
	logger::info("[OAR-API] Clips API requested (version 2)");
	return static_cast<IClipsAPIInternal*>(&g_clipsAPI);
}
