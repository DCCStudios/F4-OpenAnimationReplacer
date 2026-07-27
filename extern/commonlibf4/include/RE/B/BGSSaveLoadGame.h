#pragma once

#include "RE/T/TESFileCollection.h"
#include "RE/B/BSFixedString.h"
#include "RE/B/BSTArray.h"
#include "RE/B/BSPointerHandle.h"
#include "BGSSaveLoadFormIDMap.h"
#include "RE/B/BGSSaveLoadReferencesMap.h"
#include "RE/B/BGSConstructFormsInAllFilesMap.h"
#include "RE/B/BGSSaveLoadQueuedSubBufferMap.h"
#include "RE/B/BGSSaveLoadHistory.h"
#include "RE/B/BGSSaveLoadChangesMap.h"
#include "RE/B/BGSLoadFormData.h"

namespace RE
{
	
	class BGSSaveLoadGame
	{
	public:
		[[nodiscard]] static BGSSaveLoadGame* GetSingleton()
		{
			REL::Relocation<BGSSaveLoadGame**> singleton{ ID::BGSSaveLoadGame::Singleton };
			return *singleton;
		}

		bool GetChange(TESForm* form, BGSChangeFlags flags)
		{
			using func_t = decltype(&BGSSaveLoadGame::GetChange);
			static REL::Relocation<func_t> func{ ID::BGSSaveLoadGame::GetChange };
			return func(this, form, flags);
		}
		
		TESFileCollection SavedFiles;
		BGSSaveLoadFormIDMap WorldspaceFormIDMap;
		BSTHashMap<unsigned int, BSPointerHandle<Actor, BSUntypedPointerHandle<21, 5> >> QueuedInitPackageLocationsActorMap;
		BSTArray<BSPointerHandle<TESObjectREFR, BSUntypedPointerHandle<21, 5> >, BSTArrayHeapAllocator> QueuedMoveToEditorLocationArray;
		BGSSaveLoadReferencesMap ReferencesMap;
		BSTHashMap<unsigned int, unsigned int> ChangedFormIDMap;
		BGSReconstructFormsInAllFilesMap ReconstructForms;
		BGSSaveLoadQueuedSubBufferMap QueuedSubBuffersMap;
		BGSSaveLoadFormIDMap FormIDMap;
		BGSSaveLoadHistory History;
		BSTArray<BGSLoadFormData*, BSTArrayHeapAllocator> FormDataArray;
		BGSSaveLoadChangesMap* pChangesMap;
		BGSSaveLoadChangesMap* pOldChangesMap;
		unsigned int iGlobalFlags;
		std::uint8_t cCurrentMinorVersion;
		BSFixedString SavedGameVersion;
	};
	static_assert(sizeof(BGSSaveLoadGame) == 0x370);
}
