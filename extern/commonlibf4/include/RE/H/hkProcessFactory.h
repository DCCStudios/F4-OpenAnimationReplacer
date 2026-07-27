#pragma once

#include "RE/H/hkArray.h"
#include "RE/H/hkReferencedObject.h"

namespace RE
{
	class hkProcess;
	class hkProcessContext;

	class __declspec(novtable) hkProcessFactory :
		public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hkProcessFactory };
		inline static constexpr auto VTABLE{ VTABLE::hkProcessFactory };

		inline static hkProcessFactory* GetSingleton()
		{
			static REL::Relocation<hkProcessFactory*> singleton{ ID::hkProcessFactory::Singleton };
			return singleton.get();
		}

		hkProcess* CreateProcess(const char* a_name, hkArray<hkProcessContext*>& a_contexts)
		{
			using func_t = decltype(&hkProcessFactory::CreateProcess);
			static REL::Relocation<func_t> func{ ID::hkProcessFactory::CreateProcess };
			return func(this, a_name, a_contexts);
		}
	};
}
