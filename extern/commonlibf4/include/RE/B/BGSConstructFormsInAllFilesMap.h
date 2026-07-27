#pragma once

#include "RE/B/BSTHashMap.h"
#include "RE/T/TESFile.h"

namespace RE
{
	class ConstructFormData
	{
	public:
		TESForm*     pForm;
		unsigned int iFlags;
	};
	static_assert(sizeof(ConstructFormData) == 0x10);

	class BGSConstructedForms
	{
	public:	
		BSTArray<ConstructFormData, BSTArrayHeapAllocator> FormsArray[3];
	};
	static_assert(sizeof(BGSConstructedForms) == 0x48);

	class BGSConstructFormsMap : public BSTHashMap<unsigned int, ConstructFormData>
	{
	public:	
		unsigned int iFlags;
	};
	static_assert(sizeof(BGSConstructFormsMap) == 0x38);

	class BGSConstructCellsMap : public BSTHashMap<unsigned int, BGSConstructFormsMap*>
	{
	};
	static_assert(sizeof(BGSConstructCellsMap) == 0x30);

	class BGSConstructCellSubBlocksMap : public BSTHashMap<unsigned int, BGSConstructCellsMap*>
	{};
	static_assert(sizeof(BGSConstructCellSubBlocksMap) == 0x30);

	class BGSConstructCellBlocksMap : public BSTHashMap<unsigned int, BGSConstructCellSubBlocksMap*>
	{
	public:	
		BGSConstructFormsMap* pPersistentCell;
		bool                  bExteriors;
	};
	static_assert(sizeof(BGSConstructCellBlocksMap) == 0x40);

	class BGSConstructWorldSpacesMap : public BSTHashMap<unsigned int, BGSConstructCellBlocksMap*>
	{
	};
	static_assert(sizeof(BGSConstructWorldSpacesMap) == 0x30);


	class BGSConstructFormsInFileMap : public BSTHashMap<unsigned int, BGSConstructFormsMap*>
	{
	public:	
		BGSConstructCellBlocksMap*  pInteriorCells;
		BGSConstructWorldSpacesMap* pWorldSpaces;
		unsigned int                iCount;
	};

	static_assert(sizeof(BGSConstructFormsInFileMap) == 0x48);

	class BGSConstructFormsInAllFilesMap :
		RE::BSTHashMap<TESFile*, BGSConstructFormsInFileMap*>
	{
	public:	
		BGSConstructedForms ConstructedForms;
		unsigned int        iCount;
	};
	static_assert(sizeof(BGSConstructFormsInAllFilesMap) == 0x80);

	class BGSReconstructFormsInAllFilesMap : BGSConstructFormsInAllFilesMap {};
	static_assert(sizeof(BGSReconstructFormsInAllFilesMap) == 0x80);
}

