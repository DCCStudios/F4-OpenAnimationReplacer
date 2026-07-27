#pragma once

#include "RE/H/hkReferencedObject.h"

namespace RE
{
	class hkVtableClassRegistry :
		public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hkVtableClassRegistry };
		inline static constexpr auto VTABLE{ VTABLE::hkVtableClassRegistry };

		inline static hkVtableClassRegistry* GetSingleton()
		{
			static REL::Relocation<hkVtableClassRegistry*> singleton{ ID::hkVtableClassRegistry::Singleton };
			return singleton.get();
		}
	};
}
