#pragma once

#include "RE/N/NiCollisionObject.h"
#include "RE/N/NiTFlags.h"

namespace RE
{
	class __declspec(novtable) bhkNPCollisionObjectBase :
		public NiCollisionObject  // 00
	{
	public:
		inline static constexpr auto RTTI{ RTTI::bhkNPCollisionObjectBase };
		inline static constexpr auto VTABLE{ VTABLE::bhkNPCollisionObjectBase };
		inline static constexpr auto Ni_RTTI{ Ni_RTTI::bhkNPCollisionObjectBase };

		enum class TRANSCHANGEFLAGS : std::uint32_t
		{
			kNone = 0x0,
			kPos = 0x1,
			kRot = 0x2
		};

		enum class Flag : std::uint16_t
		{
			kNotifyPos = 0x4,
			kSetLocalPos = 0x8,
			kUseVelPos = 0x20,
			kResetPos = 0x40,
			kSyncOnUpdate = 0x80,
			kAnimTargeted = 0x400,
			kDismemberedLimb = 0x800,
		};

		// add
		virtual void LockMotionImpl() = 0;  // 2C

		// members
		NiTFlags<std::uint16_t, bhkNPCollisionObjectBase> flags;  // 18
	};
	static_assert(sizeof(bhkNPCollisionObjectBase) == 0x20);
}
