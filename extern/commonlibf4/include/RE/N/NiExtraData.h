#pragma once

#include "RE/B/BSFixedString.h"
#include "RE/N/NiObject.h"

namespace RE
{
	class __declspec(novtable) NiExtraData :
		public NiObject  // 0x00
	{
	public:
		static constexpr auto RTTI{ RTTI::NiExtraData };
		static constexpr auto VTABLE{ VTABLE::NiExtraData };
		static constexpr auto Ni_RTTI{ Ni_RTTI::NiExtraData };

		NiExtraData();
		NiExtraData(const BSFixedString& a_name);

		virtual ~NiExtraData();  // 00

		// add
		virtual bool KeepForExport() const { return false; }        // 28
		virtual bool KeepForExporterOnly() const { return false; }  // 29
		virtual bool IsStreamable() const { return true; }          // 2A
		virtual bool IsCloneable() const { return true; }           // 2B

		const BSFixedString& GetName() const;
		void                 SetName(const BSFixedString& a_name);

		// members
		BSFixedString name;  // 0x10
	};
	static_assert(sizeof(NiExtraData) == 0x18);
}
