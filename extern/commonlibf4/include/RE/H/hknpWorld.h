#pragma once

#include "RE/H/hkArray.h"
#include "RE/H/hkBitField.h"
#include "RE/H/hkBlockStream.h"
#include "RE/H/hkBool.h"
#include "RE/H/hkEnum.h"
#include "RE/H/hkIntSpaceUtil.h"
#include "RE/H/hkRefPtr.h"
#include "RE/H/hkReferencedObject.h"
#include "RE/H/hkSimdFloat32.h"
#include "RE/H/hkVector4.h"
#include "RE/H/hknpBodyManager.h"
#include "RE/H/hknpCollisionFlags.h"
#include "RE/H/hknpConstraintManager.h"
#include "RE/H/hknpContactSolverType.h"
#include "RE/H/hknpMotionManager.h"
#include "RE/H/hknpShapeManager.h"
#include "RE/H/hknpSolverInfo.h"
#include "RE/H/hknpThreadSafetyCheck.h"
#include "RE/H/hknpWorldCinfo.h"
#include "RE/H/hknpWorldSignals.h"

namespace RE
{
	class hkBlockStreamAllocator;
	class hknpActionManager;
	class hknpBodyQualityLibrary;
	class hknpBroadPhase;
	class hknpCollisionCacheManager;
	class hknpCollisionDispatcher;
	class hknpCollisionQueryDispatcherBase;
	class hknpConstraintAtomSolver;
	class hknpContactConstraintSolver;
	class hknpDeactivationManager;
	class hknpEventDispatcher;
	class hknpInternalCommandProcessor;
	class hknpMaterialLibrary;
	class hknpModifierManager;
	class hknpMotionPropertiesLibrary;
	class hknpNullShapeTagCodec;
	class hknpPostCollideTask;
	class hknpPostSolveTask;
	class hknpShapeTagCodec;
	class hknpSimulation;
	class hknpSimulationContext;
	class hknpSolverSumVelocity;
	class hknpSolverVelocity;
	class hknpSpaceSplitter;
	class hkPrimaryCommandDispatcher;
	class hkSecondaryCommandDispatcher;

	class hknpWorld :
		public hkReferencedObject
	{
	public:
		enum class AdditionMode : std::int32_t
		{
			kDoNotAddBody = 0,
			kAddBodyNow = 1,
			kAddBodyInNextStep = 2
		};

		enum class RebuildCachesMode : std::int32_t
		{
			kRebuild = 0,
			kKeep = 1
		};

		enum class SimulationStage : std::int32_t
		{
			kDone = 1,
			kPreCollide = 2,
			kCollide = 4,
			kPostCollide = 8,
			kPreSolve = 10,
			kSolve = 20,
			kPostSolve = 40
		};

		using AdditionFlagsEnum = hkcdRayQueryFlags::Enum;
		using LeavingBroadPhaseBehavior = hknpWorldCinfo::LeavingBroadPhaseBehavior;
		using SimulationType = hknpWorldCinfo::SimulationType;

		void AddBodies(hknpBodyId* a_ids, std::uint32_t a_idCount, AdditionMode a_additionMode, hkFlags<AdditionFlagsEnum, std::uint8_t> a_additionFlags)
		{
			using func_t = decltype(&hknpWorld::AddBodies);
			static REL::Relocation<func_t> func{ ID::hknpWorld::AddBodies };
			func(this, a_ids, a_idCount, a_additionMode, a_additionFlags);
		}

		hknpBodyId& CreateBody(const hknpBodyCinfo& a_cInfo, AdditionMode a_additionMode = AdditionMode::kAddBodyNow, hkFlags<AdditionFlagsEnum, std::uint8_t> a_additionFlags = AdditionFlagsEnum::kNoFlags)
		{
			using func_t = hknpBodyId&(hknpWorld*, hknpBodyId&, const hknpBodyCinfo&, AdditionMode, hkFlags<AdditionFlagsEnum, std::uint8_t>);
			static REL::Relocation<func_t> func{ ID::hknpWorld::CreateBody };

			hknpBodyId bodyId;
			return func(this, bodyId, a_cInfo, a_additionMode, a_additionFlags);
		}

		hknpMotionId& CreateMotion(const hknpMotionCinfo& a_cInfo)
		{
			using func_t = hknpMotionId&(hknpWorld*, hknpMotionId&, const hknpMotionCinfo&);
			static REL::Relocation<func_t> func{ ID::hknpWorld::CreateMotion };

			hknpMotionId motionId;
			return func(this, motionId, a_cInfo);
		}

		void DisableBodyFlags(hknpBodyId a_bodyId, hknpCollisionFlags a_enabledFlags, RebuildCachesMode a_cacheBehavior = RebuildCachesMode::kRebuild)
		{
			using func_t = decltype(&hknpWorld::DisableBodyFlags);
			static REL::Relocation<func_t> func{ ID::hknpWorld::DisableBodyFlags };
			func(this, a_bodyId, a_enabledFlags, a_cacheBehavior);
		}

		void EnableBodyFlags(hknpBodyId a_bodyId, hknpCollisionFlags a_enabledFlags, RebuildCachesMode a_cacheBehavior = RebuildCachesMode::kRebuild)
		{
			using func_t = decltype(&hknpWorld::EnableBodyFlags);
			static REL::Relocation<func_t> func{ ID::hknpWorld::EnableBodyFlags };
			func(this, a_bodyId, a_enabledFlags, a_cacheBehavior);
		}

		hknpActionManager* GetActionManager()
		{
			using func_t = decltype(&hknpWorld::GetActionManager);
			static REL::Relocation<func_t> func{ ID::hknpWorld::GetActionManager };
			return func(this);
		}

		void SetBodyCollisionFilterInfo(hknpBodyId a_bodyId, std::uint32_t a_collisionFilterInfo, RebuildCachesMode a_cacheBehavior = RebuildCachesMode::kRebuild)
		{
			using func_t = decltype(&hknpWorld::SetBodyCollisionFilterInfo);
			static REL::Relocation<func_t> func{ ID::hknpWorld::SetBodyCollisionFilterInfo };
			func(this, a_bodyId, a_collisionFilterInfo, a_cacheBehavior);
		}

		void SetBodyMotion(hknpBodyId a_bodyId, hknpMotionId a_motionId, RebuildCachesMode a_cacheBehavior = RebuildCachesMode::kRebuild)
		{
			using func_t = decltype(&hknpWorld::SetBodyMotion);
			static REL::Relocation<func_t> func{ ID::hknpWorld::SetBodyMotion };
			func(this, a_bodyId, a_motionId, a_cacheBehavior);
		}

		// members
		hknpBodyManager                                                 m_bodyManager;                       // 0x010
		hknpMotionManager                                               m_motionManager;                     // 0x0D8
		hknpConstraintManager                                           m_constraintManager;                 // 0x120
		hknpModifierManager*                                            m_modifierManager;                   // 0x150
		hkRefPtr<hkBlockStreamAllocator>                                m_persistentStreamAllocator;         // 0x158
		hkBlockStreamAllocator*                                         m_stepLocalStreamAllocator;          // 0x160
		hknpSimulation*                                                 m_simulation;                        // 0x168
		hknpSpaceSplitter*                                              m_spaceSplitter;                     // 0x170
		hknpBroadPhase*                                                 m_broadPhase;                        // 0x178
		hkIntSpaceUtil                                                  m_intSpaceUtil;                      // 0x180
		hkSimdFloat32                                                   m_collisionTolerance;                // 0x1E0
		hknpCollisionCacheManager*                                      m_collisionCacheManager;             // 0x1F0
		hknpCollisionDispatcher*                                        m_collisionDispatcher;               // 0x1F8
		hknpCollisionQueryDispatcherBase*                               m_collisionQueryDispatcher;          // 0x200
		hkBool                                                          m_contactCachingEnabled;             // 0x208
		hkBlockStream<std::int8_t>                                      m_lastFrameContactData;              // 0x210
		hknpContactSolverType::Enum                                     m_contactSolverType;                 // 0x310
		hknpContactConstraintSolver*                                    m_contactSolver;                     // 0x318
		hknpConstraintAtomSolver*                                       m_constraintAtomSolver;              // 0x320
		hknpSolverInfo                                                  m_solverInfo;                        // 0x330
		hkBool                                                          m_enableSolverDynamicScheduling;     // 0x470
		hkArray<hknpSolverVelocity>                                     m_solverVelocities;                  // 0x478
		hkArray<hknpSolverSumVelocity>                                  m_solverSumVelocities;               // 0x488
		hkBool                                                          m_deactivationEnabled;               // 0x498
		hknpDeactivationManager*                                        m_deactivationManager;               // 0x4A0
		hkBool                                                          m_deleteCachesOnDeactivation;        // 0x4A8
		hknpWorldSignals                                                m_signals;                           // 0x4B0
		hkPrimaryCommandDispatcher*                                     m_commandDispatcher;                 // 0x598
		hkRefPtr<hkSecondaryCommandDispatcher>                          m_traceDispatcher;                   // 0x5A0
		std::uint16_t                                                   m_userData;                          // 0x5A8
		hkVector4f                                                      m_gravity;                           // 0x5B0
		hkRefPtr<hkReferencedObject>                                    m_defaultModifierSet;                // 0x5C0
		hkRefPtr<hknpMaterialLibrary>                                   m_materialLibrary;                   // 0x5C8
		hkRefPtr<hknpMotionPropertiesLibrary>                           m_motionPropertiesLibrary;           // 0x5D0
		hkRefPtr<hknpBodyQualityLibrary>                                m_qualityLibrary;                    // 0x5D8
		hkBitField                                                      m_dirtyMaterials;                    // 0x5E0
		hkBitField                                                      m_dirtyQualities;                    // 0x5F8
		hknpShapeManager                                                m_shapeManager;                      // 0x610
		hkRefPtr<hknpShapeTagCodec>                                     m_shapeTagCodec;                     // 0x638
		hknpNullShapeTagCodec*                                          m_nullShapeTagCodec;                 // 0x640
		hkRefPtr<hknpEventDispatcher>                                   m_eventDispatcher;                   // 0x648
		hkRefPtr<hknpInternalCommandProcessor>                          m_internalPhysicsCommandDispatcher;  // 0x650
		hkBool                                                          m_consistencyChecksEnabled;          // 0x658
		hknpPostCollideTask*                                            m_postCollideTask;                   // 0x660
		hknpPostSolveTask*                                              m_postSolveTask;                     // 0x668
		hkEnum<SimulationStage, std::uint32_t>                          m_simulationStage;                   // 0x670
		hknpSimulationContext*                                          m_simulationContext;                 // 0x678
		hknpActionManager*                                              m_actionManager;                     // 0x680
		hkEnum<hknpWorldCinfo::SimulationType, std::uint8_t>            m_simulationType;                    // 0x688
		hkBool                                                          m_mergeEventsBeforeDispatch;         // 0x689
		hkEnum<hknpWorldCinfo::LeavingBroadPhaseBehavior, std::uint8_t> m_leavingBroadPhaseBehavior;         // 0x68A
		hknpThreadSafetyCheck                                           m_threadSafetyCheck;                 // 0x690
	};
	static_assert(sizeof(hknpWorld) == 0x6D0);
}
