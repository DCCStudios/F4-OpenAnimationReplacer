#pragma once

#include "RE/B/BSTArray.h"

 namespace RE
 {
	 class BGSSaveLoadHistory
	 {
	 public:	
		 BSTArray<char*, BSTArrayHeapAllocator> Notes;
	 };
	static_assert(sizeof(BGSSaveLoadHistory) == 0x18);
 }
