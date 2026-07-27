#pragma once

#include "RE/H/hkArray.h"
#include "RE/H/hkRefPtr.h"
#include "RE/H/hkReferencedObject.h"
#include "RE/H/hknpActivation.h"
#include "RE/H/hknpBodyId.h"
#include "RE/H/hknpConstraintId.h"
#include "RE/H/hknpWorld.h"

namespace RE
{
	class hknpPhysicsSystemData;

	class __declspec(novtable) hknpPhysicsSystem :
		public hkReferencedObject
	{
	public:
		static constexpr auto RTTI{ RTTI::hknpPhysicsSystem };
		static constexpr auto VTABLE{ VTABLE::hknpPhysicsSystem };

		enum class Flags : std::int32_t
		{
			CLONE_POWERABLE_CONSTRAINT_DATAS = 0x1,
			FORCE_EXPORTABLE_CONSTRAINTS = 0x2,
			CREATE_CONSTRAINTS = 0x4,
			DEFAULT_FLAGS = 0x5,
		};

		virtual ~hknpPhysicsSystem();  // 00

		void AddToWorld(hknpWorld::AdditionMode a_additionMode, hkFlags<hknpWorld::AdditionFlagsEnum, std::uint8_t> a_additionFlags, hknpActivationMode::Enum a_constraintActivationMode)
		{
			using func_t = decltype(&hknpPhysicsSystem::AddToWorld);
			static REL::Relocation<func_t> func{ ID::hknpPhysicsSystem::AddToWorld };
			func(this, a_additionMode, a_additionFlags, a_constraintActivationMode);
		}

		void RemoveFromWorld()
		{
			using func_t = decltype(&hknpPhysicsSystem::RemoveFromWorld);
			static REL::Relocation<func_t> func{ ID::hknpPhysicsSystem::RemoveFromWorld };
			func(this);
		}

		// members
		hkRefPtr<const hknpPhysicsSystemData> m_data;           // 0x10
		hkRefPtr<hknpWorld>                   m_world;          // 0x18
		hkArray<hknpBodyId>                   m_bodyIds;        // 0x20
		hkArray<hknpConstraintId>             m_constraintIds;  // 0x30
	};
	static_assert(sizeof(hknpPhysicsSystem) == 0x40);
}
