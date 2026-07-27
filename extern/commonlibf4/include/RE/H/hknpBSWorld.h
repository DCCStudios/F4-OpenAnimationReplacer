#pragma once

#include "RE/B/BSSpinLock.h"
#include "RE/B/BSTHashMap.h"
#include "RE/H/hkMap.h"
#include "RE/H/hknpBodyId.h"
#include "RE/H/hknpWorld.h"

namespace RE
{
	class __declspec(novtable) hknpBSWorld :
		public hknpWorld
	{
	public:
		static constexpr auto RTTI{ RTTI::hknpBSWorld };
		static constexpr auto VTABLE{ VTABLE::hknpBSWorld };

		virtual ~hknpBSWorld() override;  // 00

		hknpMotion* AccessMotion(hknpMotionId a_motionId)
		{
			using func_t = decltype(&hknpBSWorld::AccessMotion);
			static REL::Relocation<func_t> func{ ID::hknpBSWorld::AccessMotion };
			return func(this, a_motionId);
		}

		void ActivateBody(hknpBodyId a_bodyId)
		{
			using func_t = decltype(&hknpBSWorld::ActivateBody);
			static REL::Relocation<func_t> func{ ID::hknpBSWorld::ActivateBody };
			func(this, a_bodyId);
		}

		void SetBodyAngularVelocity(hknpBodyId a_bodyId, const hkVector4& a_angularVelocity)
		{
			using func_t = decltype(&hknpBSWorld::SetBodyAngularVelocity);
			static REL::Relocation<func_t> func{ ID::hknpBSWorld::SetBodyAngularVelocity };
			func(this, a_bodyId, a_angularVelocity);
		}

		void SetBodyKeyframed(hknpBodyId a_bodyId, RebuildCachesMode a_cacheBehavior)
		{
			using func_t = decltype(&hknpBSWorld::SetBodyKeyframed);
			static REL::Relocation<func_t> func{ ID::hknpBSWorld::SetBodyKeyframed };
			func(this, a_bodyId, a_cacheBehavior);
		}

		void SetBodyLinearVelocity(hknpBodyId a_bodyId, const hkVector4& a_linearVelocity)
		{
			using func_t = decltype(&hknpBSWorld::SetBodyLinearVelocity);
			static REL::Relocation<func_t> func{ ID::hknpBSWorld::SetBodyLinearVelocity };
			func(this, a_bodyId, a_linearVelocity);
		}

		void SetBodyStatic(hknpBodyId a_bodyId, RebuildCachesMode a_cacheBehavior)
		{
			using func_t = decltype(&hknpBSWorld::SetBodyStatic);
			static REL::Relocation<func_t> func{ ID::hknpBSWorld::SetBodyStatic };
			func(this, a_bodyId, a_cacheBehavior);
		}

		void SetBodyVelocity(hknpBodyId a_bodyId, const hkVector4& a_linearVelocity, const hkVector4& a_angularVelocity)
		{
			using func_t = decltype(&hknpBSWorld::SetBodyVelocity);
			static REL::Relocation<func_t> func{ ID::hknpBSWorld::SetBodyVelocity };
			func(this, a_bodyId, a_linearVelocity, a_angularVelocity);
		}

		// members
		void*                     m_userData;                        // 0x6D0
		BSReadWriteLock           m_worldLock;                       // 0x6D8
		std::byte                 m_activateBodyPostCollideSet[48];  // 0x6E0 - BSTSet<hknpBodyId>
		BSSpinLock                m_activateBodyPostCollideLock;     // 0x710
		hkMultiMap<std::uint32_t> m_constrainedBodyFromBody;         // 0x718
	};
	static_assert(sizeof(hknpBSWorld) == 0x730);
}
