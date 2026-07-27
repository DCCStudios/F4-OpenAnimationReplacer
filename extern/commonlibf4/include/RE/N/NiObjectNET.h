#pragma once

#include "RE/B/BSFixedString.h"
#include "RE/N/NiExtraDataContainer.h"
#include "RE/N/NiObject.h"
#include "RE/N/NiPointer.h"
#include "RE/N/NiTimeController.h"

namespace RE
{
	class NiExtraData;

	class __declspec(novtable) NiObjectNET :
		public NiObject  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::NiObjectNET };
		static constexpr auto VTABLE{ VTABLE::NiObjectNET };
		static constexpr auto Ni_RTTI{ Ni_RTTI::NiObjectNET };

		enum class CopyType
		{
			kNone = 0x0,
			kExact = 0x1,
			kUnique = 0x2
		};

		NiObjectNET();
		virtual ~NiObjectNET();  // NOLINT(modernize-use-override) 00

		F4_HEAP_REDEFINE_NEW(NiObjectNET);

		bool                 AddExtraData(const BSFixedString& a_key, NiExtraData* a_extra);
		bool                 AddExtraData(NiExtraData* a_extra);
		NiExtraData*         GetExtraData(const BSFixedString& a_key) const;
		std::uint16_t        GetExtraDataSize() const;
		const BSFixedString& GetName() const { return name; }
		bool                 HasExtraData(const BSFixedString& a_key) const;
		bool                 InsertExtraData(NiExtraData* a_extra);
		bool                 RemoveExtraData(const BSFixedString& a_key);

		template <class T>
		T* GetExtraData(const BSFixedString& a_key) const
		{
			return static_cast<T*>(GetExtraData(a_key));
		}

		// members
		BSFixedString               name{ "" };        // 10
		NiPointer<NiTimeController> controllers;       // 18
		NiExtraDataContainer*       extra{ nullptr };  // 20
	};
	static_assert(sizeof(NiObjectNET) == 0x28);
}
