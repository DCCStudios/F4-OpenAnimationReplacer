#pragma once

#include "RE/B/BSFixedString.h"
#include "RE/B/BSSpinLock.h"
#include "RE/B/BSTArray.h"
#include "RE/N/NiPointer.h"

namespace RE
{
	class NiExtraData;

	class NiExtraDataContainer
	{
	public:
		NiExtraDataContainer(const std::uint32_t a_count);

		void          Add(NiExtraData* a_extra);
		NiExtraData*  FindExtra(const BSFixedString& a_key) const;
		std::int32_t  FindIndex(const BSFixedString& a_key) const;
		std::uint32_t GetSize() const;
		bool          RemoveExtra(const BSFixedString& a_key);

		template <class T>
		T* FindExtra(const BSFixedString& a_key) const
		{
			return static_cast<T*>(FindExtra(a_key));
		}

		// members
		BSTArray<NiPointer<NiExtraData>> extra;
		mutable BSReadWriteLock          lock;
	};
	static_assert(sizeof(NiExtraDataContainer) == 0x20);
}
