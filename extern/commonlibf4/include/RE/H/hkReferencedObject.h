#pragma once

#include "RE/H/hkBaseObject.h"

namespace RE
{
	class hkClass;

	class __declspec(novtable) hkReferencedObject :
		public hkBaseObject  // 0x00
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hkReferencedObject };
		inline static constexpr auto VTABLE{ VTABLE::hkReferencedObject };

		hkReferencedObject() { REX::EMPLACE_VTABLE(this); }

		// add
		virtual const hkClass* GetClassType() const { return nullptr; }             // 0x10 [02]
		virtual void           DeleteThisReferencedObject() const { delete this; }  // 0x18 [03]

		void AddReference()
		{
			using func_t = decltype(&hkReferencedObject::AddReference);
			static REL::Relocation<func_t> func{ ID::hkReferencedObject::AddReference };
			func(this);
		}

		void RemoveReference()
		{
			using func_t = decltype(&hkReferencedObject::RemoveReference);
			static REL::Relocation<func_t> func{ ID::hkReferencedObject::RemoveReference };
			func(this);
		}

		// members
		std::uint32_t m_memSizeAndRefCount{ static_cast<std::uint32_t>(-65535) };  // 0x08
	};
	static_assert(sizeof(hkReferencedObject) == 0x10);
}
