#pragma once

#include "RE/B/BSSpinLock.h"
#include "RE/B/BSTArray.h"
#include "RE/B/bhkCharProxyManager.h"
#include "RE/B/bhkCharRigidBodyManager.h"
#include "RE/H/hkRefPtr.h"
#include "RE/H/hkVector4.h"
#include "RE/H/hknpWorld.h"
#include "RE/N/NiObject.h"

namespace RE
{
	class bhkIAddToWorld;
	class bhkTrapListener;
	class hclBSWorld;
	class hkbnpPhysicsInterface;
	class hknpBSWorld;
	class hknpPhysicsSystem;
	class hkTaskQueue;
	class NiAVObject;

	class __declspec(novtable) bhkWorld :
		public NiObject  // 00
	{
	public:
		inline static constexpr auto RTTI{ RTTI::bhkWorld };
		inline static constexpr auto VTABLE{ VTABLE::bhkWorld };
		inline static constexpr auto Ni_RTTI{ Ni_RTTI::bhkWorld };

		virtual ~bhkWorld();  // 00

		// add
		virtual bool Update(std::uint32_t a_updateFlags);  // 28
		virtual void Init(const hknpWorldCinfo& a_info);   // 29

		bool AddObjects(NiAVObject* a_object, bool a_recurse, bool a_notify, std::uint32_t a_group, bool a_force, const bhkIAddToWorld* a_addToWorld)
		{
			using func_t = decltype(&bhkWorld::AddObjects);
			static REL::Relocation<func_t> func{ ID::bhkWorld::AddObjects };
			return func(this, a_object, a_recurse, a_notify, a_group, a_force, a_addToWorld);
		}

		void AddPhysicsSystem(hknpPhysicsSystem* a_physicsSystem, bool a_startActive)
		{
			using func_t = decltype(&bhkWorld::AddPhysicsSystem);
			static REL::Relocation<func_t> func{ ID::bhkWorld::AddPhysicsSystem };
			func(this, a_physicsSystem, a_startActive);
		}

		bool IsBodyAdded(hknpBodyId a_bodyId)
		{
			using func_t = decltype(&bhkWorld::IsBodyAdded);
			static REL::Relocation<func_t> func{ ID::bhkWorld::IsBodyAdded };
			return func(this, a_bodyId);
		}

		void RemovePhysicsSystem(hknpPhysicsSystem* a_physicsSystem)
		{
			using func_t = decltype(&bhkWorld::RemovePhysicsSystem);
			static REL::Relocation<func_t> func{ ID::bhkWorld::RemovePhysicsSystem };
			func(this, a_physicsSystem);
		}

		static bool Activate(NiAVObject* a_object, bool a_activate, bool a_recurse, bool a_force)
		{
			using func_t = decltype(&bhkWorld::Activate);
			static REL::Relocation<func_t> func{ ID::bhkWorld::Activate };
			return func(a_object, a_activate, a_recurse, a_force);
		}

		static bool Enable(NiAVObject* a_object, bool a_enable, bool a_recurse, bool a_force)
		{
			using func_t = decltype(&bhkWorld::Enable);
			static REL::Relocation<func_t> func{ ID::bhkWorld::Enable };
			return func(a_object, a_enable, a_recurse, a_force);
		}

		static bool EnableCollision(NiAVObject* a_object, bool a_enable, bool a_recurse, bool a_force)
		{
			using func_t = decltype(&bhkWorld::EnableCollision);
			static REL::Relocation<func_t> func{ ID::bhkWorld::EnableCollision };
			return func(a_object, a_enable, a_recurse, a_force);
		}

		static bool IsActive(NiAVObject* a_object, bool a_recurse, bool a_force)
		{
			using func_t = decltype(&bhkWorld::IsActive);
			static REL::Relocation<func_t> func{ ID::bhkWorld::IsActive };
			return func(a_object, a_recurse, a_force);
		}

		static bool RemoveObjects(NiAVObject* a_object, bool a_recurse, bool a_force)
		{
			using func_t = decltype(&bhkWorld::RemoveObjects);
			static REL::Relocation<func_t> func{ ID::bhkWorld::RemoveObjects };
			return func(a_object, a_recurse, a_force);
		}

		static bool SetCollisionGroup(NiAVObject* a_object, std::uint32_t a_group, bool a_recurse, bool a_force)
		{
			using func_t = decltype(&bhkWorld::SetCollisionGroup);
			static REL::Relocation<func_t> func{ ID::bhkWorld::SetCollisionGroup };
			return func(a_object, a_group, a_recurse, a_force);
		}

		static bool SetDismemberedLimb(NiAVObject* a_object, bool a_tf, bool a_recurse)
		{
			using func_t = decltype(&bhkWorld::SetDismemberedLimb);
			static REL::Relocation<func_t> func{ ID::bhkWorld::SetDismemberedLimb };
			return func(a_object, a_tf, a_recurse);
		}

		static bool SetMotion(NiAVObject* a_object, std::uint32_t a_type, bool a_recurse, bool a_force, bool a_allowActivate)
		{
			using func_t = decltype(&bhkWorld::SetMotion);
			static REL::Relocation<func_t> func{ ID::bhkWorld::SetMotion };
			return func(a_object, a_type, a_recurse, a_force, a_allowActivate);
		}

		// members
		hkRefPtr<hkTaskQueue>            m_taskQueue;                 // 0x010
		BSTArray<bhkIWorldStepListener*> m_stepListenerA;             // 0x018
		hkRefPtr<hkbnpPhysicsInterface>  m_behaviorInterface;         // 0x030
		BSReadWriteLock                  m_charProxyManagerLock;      // 0x038
		BSReadWriteLock                  m_charRigidBodyManagerLock;  // 0x040
		BSReadWriteLock                  m_stepListenerALock;         // 0x048
		hkVector4f                       m_origin;                    // 0x050
		hkRefPtr<hknpBSWorld>            m_worldNP;                   // 0x060
		bhkCharProxyManager              m_charProxyManager;          // 0x068
		bhkCharRigidBodyManager          m_charRigidBodyManager;      // 0x0D8
		hkRefPtr<hclBSWorld>             m_clothWorld;                // 0x148
		char*                            m_clothStepBuffer;           // 0x150
		std::int32_t                     m_clothStepBufferSize;       // 0x158
		hknpWorld::AdditionMode          m_additionMode;              // 0x15C
		bhkTrapListener*                 m_trapListener;              // 0x160
		float                            m_originalTau;               // 0x168
		float                            m_originalDamping;           // 0x16C
		bool                             m_globalWindActivated;       // 0x170
		bool                             m_added;                     // 0x171
		bool                             m_enabled;                   // 0x172
		std::byte                        m_pad173[0xD];               // 0x173
	};
	static_assert(sizeof(bhkWorld) == 0x180);
}
