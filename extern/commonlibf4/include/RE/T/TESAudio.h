#pragma once

#include "RE/B/BSIntrusiveRefCounted.h"

namespace RE
{
	namespace TESAudio
	{
		enum class CategoryDuckingType : std::int32_t
		{
			kHolotapeProgram = 0x0,
			kHolotapeScene = 0x1,
			kDialogueMenu = 0x2,
			kSemiDialogue = 0x3,
			kDialogueCamera = 0x4,
			kInterestingNonDialogue = 0x5,
			kHighlightNearby = 0x6
		};

		class __declspec(novtable) alignas(0x08) RepeaterData :
			public BSIntrusiveRefCounted  // 00
		{
		public:
			// members
			BSSoundHandle   currSound;  // 04
			const TESSound* soundBase;  // 10
			float           countdown;  // 18
		};
		static_assert(sizeof(RepeaterData) == 0x20);

		class __declspec(novtable) ScriptedMusicState :
			public BSTEventSink<TESLoadGameEvent>,      // 00
			public BSTSingletonSDM<ScriptedMusicState>  // 08
		{
		public:
			[[nodiscard]] static ScriptedMusicState* GetSingleton()
			{
				static REL::Relocation<ScriptedMusicState**> singleton{ ID::TESAudio::ScriptedMusicState::Singleton };
				return *singleton;
			}

			void Remove(BGSMusicType* a_toRemove)
			{
				using func_t = decltype(&ScriptedMusicState::Remove);
				static REL::Relocation<func_t> func{ ID::TESAudio::ScriptedMusicState::Remove };
				return func(this, a_toRemove);
			}

			// members
			BSTSmallArray<BGSMusicType*, 2> currentTypes;
		};
		static_assert(sizeof(ScriptedMusicState) == 0x30);

		class __declspec(novtable) SnapTransitionData
		{
		public:
			// members
			float transitionQuantum;  // 00
			float desiredMult;        // 04
		};

		class __declspec(novtable) alignas(0x08) SnapshotEntry
		{
		public:
			// members
			const BGSSoundCategorySnapshot* snapForm;        // 00
			float                           transitionSecs;  // 08
		};
		static_assert(sizeof(SnapshotEntry) == 0x10);
	}
}
