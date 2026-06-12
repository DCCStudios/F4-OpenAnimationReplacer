#pragma once

// =============================================================================
// RE_Additions.h — RE types and helpers missing from the main CommonLibF4 tree
// =============================================================================
// Ported from the f4se-code-main and alandtse CommonLibF4 forks.
// These provide ProcessLists iteration, detection level queries,
// MenuTopicManager for dialogue state, and NiPoint3 math.
// =============================================================================

#include <cstdint>
#include <cmath>

namespace OAR_RE
{
	// =========================================================================
	// ProcessLists — iterate high-process actors in the loaded area
	// =========================================================================
	// Minimal definition: only exposes the singleton and handle arrays we need.

	class ProcessLists
	{
	public:
		[[nodiscard]] static ProcessLists* GetSingleton()
		{
			REL::Relocation<ProcessLists**> singleton{ REL::ID(1569706) };
			return *singleton;
		}

		// Skip to the highActorHandles array at offset 0x40
		// Layout before it: vtable(8) + eventSink pad(8) + singleton pad(1+7)
		//   + BSSemaphore(8) + crimeUpdateTimer(4) + removeExcessDeadTimer(4)
		//   + numberHighActors(4) + numberFullyEnabledHighActors(4)
		//   + crimeNumber(4) + statdetect(4) + runDetection...(8 bools + 4 pad)
		uint8_t _pad[0x40];
		RE::BSTArray<RE::ActorHandle> highActorHandles;     // 0x040
		RE::BSTArray<RE::ActorHandle> lowActorHandles;      // 0x058
		RE::BSTArray<RE::ActorHandle> middleHighActorHandles; // 0x070
		RE::BSTArray<RE::ActorHandle> middleLowActorHandles; // 0x088
	};

	// =========================================================================
	// RequestDetectionLevel — query stealth detection between two actors
	// =========================================================================
	// Returns detection level (>0 = detected, 0 = undetected, <0 = lost).

	enum class DETECTION_PRIORITY : int32_t
	{
		kNone = 0,
		kVeryLow = 1,
		kLow = 2,
		kNormal = 3,
		kHigh = 4,
		kCritical = 5
	};

	inline int32_t RequestDetectionLevel(RE::Actor* a_detector, RE::Actor* a_target,
		DETECTION_PRIORITY a_priority = DETECTION_PRIORITY::kNormal)
	{
		using func_t = int32_t(*)(RE::Actor*, RE::Actor*, DETECTION_PRIORITY);
		static REL::Relocation<func_t> func{ REL::ID(943772) };
		return func(a_detector, a_target, a_priority);
	}

	// =========================================================================
	// MenuTopicManager — dialogue state tracking
	// =========================================================================

	class MenuTopicManager
	{
	public:
		[[nodiscard]] static MenuTopicManager* GetSingleton()
		{
			static REL::Relocation<MenuTopicManager**> singleton{ REL::ID(520890) };
			return *singleton;
		}

		// Skip to the member fields we care about.
		// Offset 0x14: speaker handle, 0x4C: menuOpen bool
		uint8_t _pad0[0x4C];
		bool menuOpen;          // 0x4C
		bool shutMenu;          // 0x4D
		bool canSkip;           // 0x4E
		bool shuttingDown;      // 0x4F
	};

	// =========================================================================
	// NiPoint3 math helpers
	// =========================================================================

	inline float GetSquaredDistance(const RE::NiPoint3& a, const RE::NiPoint3& b)
	{
		float dx = b.x - a.x;
		float dy = b.y - a.y;
		float dz = b.z - a.z;
		return dx * dx + dy * dy + dz * dz;
	}

	inline float GetDistance(const RE::NiPoint3& a, const RE::NiPoint3& b)
	{
		return std::sqrt(GetSquaredDistance(a, b));
	}

	// =========================================================================
	// GetRelationshipRank — lookup relationship rank between two NPCs
	// =========================================================================
	// Returns the relationship rank (-4 archnemesis to +4 lover).
	// Falls back to iterating the relationship array on TESNPC.

	inline int32_t GetRelationshipRank(RE::TESNPC* a_npc1, RE::TESNPC* a_npc2)
	{
		if (!a_npc1 || !a_npc2) return 0;

		// TESNPC stores a pointer to its relationships array at offset 0x2F0
		auto* relArray = a_npc1->relationships;
		if (!relArray) return 0;

		for (auto* rel : *relArray) {
			if (!rel) continue;
			if ((rel->npc1 == a_npc1 && rel->npc2 == a_npc2) ||
				(rel->npc1 == a_npc2 && rel->npc2 == a_npc1)) {
				// packedData lower 3 bits encode the association rank index
				// Rank values: 0=Acquaintance, 1=Friend, 2=Confidant, 3=Ally, 4=Lover
				// For enemies: the assocType encodes negative ranks
				// The packed rank uses bits 0-2 for the rank index (0-4 positive scale)
				int32_t rankIndex = static_cast<int32_t>(rel->packedData & 0x7);
				// Convert to signed rank: 0=neutral(0), 1=friend(1), 2=confidant(2), 3=ally(3), 4=lover(4)
				// Negative relationships are encoded differently via assocType
				return rankIndex;
			}
		}

		return 0;
	}

} // namespace OAR_RE
