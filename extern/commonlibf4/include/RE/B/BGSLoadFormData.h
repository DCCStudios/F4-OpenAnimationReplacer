#pragma once

#include "RE/B/BGSChangeFlags.h"
#include "RE/B/BGSSaveGameBuffer.h"
#include "RE/B/BGSSaveLoadFormInfo.h"

namespace RE
{
	class BGSLoadFormData
	{
	public:		
		unsigned int        iFormID;
		unsigned int        uiDataSize;
		unsigned int        uiUncompressedSize;
		TESForm*            pForm;
		BGSChangeFlags      iChangeFlags;
		BGSChangeFlags      iOldChangeFlags;
		std::uint16_t		usFlags;
		BGSSaveLoadFormInfo FormInfo;
		std::uint8_t        cVersion;
	};
	static_assert(sizeof(BGSLoadFormData) == 0x28);
}
