#pragma once

#include "RE/H/hkReferencedObject.h"

namespace RE
{
	class __declspec(novtable) hkHardwareInfo :
		public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hkHardwareInfo };
		inline static constexpr auto VTABLE{ VTABLE::hkHardwareInfo };

		inline static std::int32_t CalcNumHardwareThreads()
		{
			using func_t = decltype(&hkHardwareInfo::CalcNumHardwareThreads);
			static REL::Relocation<func_t> func{ ID::hkHardwareInfo::CalcNumHardwareThreads };
			return func();
		}

		// members
		std::int32_t  m_numHardwareThreads;
		std::uint32_t m_cpuFeatures;
	};
	static_assert(sizeof(hkHardwareInfo) == 0x18);
}
