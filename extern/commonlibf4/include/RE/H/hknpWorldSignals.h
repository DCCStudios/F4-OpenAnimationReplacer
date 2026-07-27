#pragma once

#include "RE/H/hkHandle.h"
#include "RE/H/hkRefPtr.h"
#include "RE/H/hkSignal.h"
#include "RE/H/hknpConstraintIdDiscriminant.h"

namespace RE
{
	class hknpBodyCinfo;
	class hknpBodyId;
	class hknpBodyManager;
	class hknpConstraint;
	class hknpConstraintCinfo;
	class hknpConstraintManager;
	class hknpMotionCinfo;
	class hknpMotionId;
	class hknpMotionManager;
	class hknpShape;
	class hknpSolverData;
	class hknpStepInput;
	class hknpWorld;
	class hkVector4f;

	class hknpWorldSignals
	{
	public:
		class WorldDestroyedSignal :
			public hkSignal1<hknpWorld*>
		{
		public:
		};
		static_assert(sizeof(WorldDestroyedSignal) == 0x08);

		class WorldShiftedSignal :
			public hkSignal2<hknpWorld*, const hkVector4f&>
		{
		public:
		};
		static_assert(sizeof(WorldShiftedSignal) == 0x08);

		class BodyBufferFullSignal :
			public hkSignal2<hknpWorld*, hknpBodyManager*>
		{
		public:
		};
		static_assert(sizeof(BodyBufferFullSignal) == 0x08);

		class BodyBufferChangedSignal :
			public hkSignal2<hknpWorld*, hknpBodyManager*>
		{
		public:
		};
		static_assert(sizeof(BodyBufferChangedSignal) == 0x08);

		class BodyCreatedSignal :
			public hkSignal3<hknpWorld*, const hknpBodyCinfo*, hknpBodyId>
		{
		public:
		};
		static_assert(sizeof(BodyCreatedSignal) == 0x08);

		class BodyAddedSignal :
			public hkSignal2<hknpWorld*, hknpBodyId>
		{
		public:
		};
		static_assert(sizeof(BodyAddedSignal) == 0x08);

		class BodyRemovedSignal :
			public hkSignal2<hknpWorld*, hknpBodyId>
		{
		public:
		};
		static_assert(sizeof(BodyRemovedSignal) == 0x08);

		class BodyDestroyedSignal :
			public hkSignal2<hknpWorld*, hknpBodyId>
		{
		public:
		};
		static_assert(sizeof(BodyDestroyedSignal) == 0x08);

		class MotionBufferFullSignal :
			public hkSignal2<hknpWorld*, hknpMotionManager*>
		{
		public:
		};
		static_assert(sizeof(MotionBufferFullSignal) == 0x08);

		class MotionBufferChangedSignal :
			public hkSignal2<hknpWorld*, hknpMotionManager*>
		{
		public:
		};
		static_assert(sizeof(MotionBufferChangedSignal) == 0x08);

		class MotionCreatedSignal :
			public hkSignal3<hknpWorld*, const hknpMotionCinfo*, hknpMotionId>
		{
		public:
		};
		static_assert(sizeof(MotionCreatedSignal) == 0x08);

		class MotionDestroyedSignal :
			public hkSignal2<hknpWorld*, hknpMotionId>
		{
		public:
		};
		static_assert(sizeof(MotionDestroyedSignal) == 0x08);

		class StaticBodyMovedSignal :
			public hkSignal2<hknpWorld*, hknpBodyId>
		{
		public:
		};
		static_assert(sizeof(StaticBodyMovedSignal) == 0x08);

		class BodySwitchStaticDynamicSignal :
			public hkSignal3<hknpWorld*, hknpBodyId, bool>
		{
		public:
		};
		static_assert(sizeof(BodySwitchStaticDynamicSignal) == 0x08);

		class BodyAttachToCompoundSignal :
			public hkSignal3<hknpWorld*, hknpBodyId, hknpBodyId>
		{
		public:
		};
		static_assert(sizeof(BodyAttachToCompoundSignal) == 0x08);

		class BodyDetachToCompoundSignal :
			public hkSignal3<hknpWorld*, hknpBodyId, hknpMotionId>
		{
		public:
		};
		static_assert(sizeof(BodyDetachToCompoundSignal) == 0x08);

		class BodyShapeSetSignal :
			public hkSignal3<hknpWorld*, hknpBodyId, hkRefPtr<const hknpShape>&>
		{
		public:
		};
		static_assert(sizeof(BodyShapeSetSignal) == 0x08);

		class BodyChangedSignal :
			public hkSignal2<hknpWorld*, hknpBodyId>
		{
		public:
		};
		static_assert(sizeof(BodyChangedSignal) == 0x08);

		class ConstraintBufferFullSignal :
			public hkSignal2<hknpWorld*, hknpConstraintManager*>
		{
		public:
		};
		static_assert(sizeof(ConstraintBufferFullSignal) == 0x08);

		class ConstraintBufferChangedSignal :
			public hkSignal2<hknpWorld*, hknpConstraintManager*>
		{
		public:
		};
		static_assert(sizeof(ConstraintBufferChangedSignal) == 0x08);

		class ConstraintCreatedSignal :
			public hkSignal3<hknpWorld*, const hknpConstraintCinfo*, hkHandle<std::uint32_t, 2147483647, hknpConstraintIdDiscriminant>>
		{
		public:
		};
		static_assert(sizeof(ConstraintCreatedSignal) == 0x08);

		class ConstraintDestroyedSignal :
			public hkSignal2<hknpWorld*, hkHandle<std::uint32_t, 2147483647, hknpConstraintIdDiscriminant>>
		{
		public:
		};
		static_assert(sizeof(ConstraintDestroyedSignal) == 0x08);

		class ConstraintEnabledSignal :
			public hkSignal2<hknpWorld*, hkHandle<std::uint32_t, 2147483647, hknpConstraintIdDiscriminant>>
		{
		public:
		};
		static_assert(sizeof(ConstraintEnabledSignal) == 0x08);

		class ConstraintDisabledSignal :
			public hkSignal2<hknpWorld*, hkHandle<std::uint32_t, 2147483647, hknpConstraintIdDiscriminant>>
		{
		public:
		};
		static_assert(sizeof(ConstraintDisabledSignal) == 0x08);

		class ImmediateConstraintAddedSignal :
			public hkSignal2<hknpWorld*, hknpConstraint*>
		{
		public:
		};
		static_assert(sizeof(ImmediateConstraintAddedSignal) == 0x08);

		class PreCollideSignal :
			public hkSignal2<hknpWorld*, const hknpStepInput&>
		{
		public:
		};
		static_assert(sizeof(PreCollideSignal) == 0x08);

		class PostCollideSignal :
			public hkSignal2<hknpWorld*, hknpSolverData*>
		{
		public:
		};
		static_assert(sizeof(PostCollideSignal) == 0x08);

		class PreSolveSignal :
			public hkSignal2<hknpWorld*, hknpSolverData*>
		{
		public:
		};
		static_assert(sizeof(PreSolveSignal) == 0x08);

		class PostSolveSignal :
			public hkSignal1<hknpWorld*>
		{
		public:
		};
		static_assert(sizeof(PostSolveSignal) == 0x08);

		// members
		WorldDestroyedSignal           worldDestroyed;            // 0x00
		WorldShiftedSignal             worldShifted;              // 0x08
		BodyBufferFullSignal           bodyBufferFull;            // 0x10
		BodyBufferChangedSignal        bodyBufferChanged;         // 0x18
		BodyCreatedSignal              bodyCreated;               // 0x20
		BodyAddedSignal                bodyAdded;                 // 0x28
		BodyRemovedSignal              bodyRemoved;               // 0x30
		BodyDestroyedSignal            bodyDestroyed;             // 0x38
		MotionBufferFullSignal         motionBufferFull;          // 0x40
		MotionBufferChangedSignal      motionBufferChanged;       // 0x48
		MotionCreatedSignal            motionCreated;             // 0x50
		MotionDestroyedSignal          motionDestroyed;           // 0x58
		StaticBodyMovedSignal          staticBodyMoved;           // 0x60
		BodySwitchStaticDynamicSignal  bodySwitchStaticDynamic;   // 0x68
		BodyAttachToCompoundSignal     bodyAttached;              // 0x70
		BodyDetachToCompoundSignal     bodyDetached;              // 0x78
		BodyShapeSetSignal             bodyShapeChanged;          // 0x80
		BodyChangedSignal              bodyChanged;               // 0x88
		ConstraintBufferFullSignal     constraintBufferFull;      // 0x90
		ConstraintBufferChangedSignal  constraintBufferChanged;   // 0x98
		ConstraintCreatedSignal        constraintCreated;         // 0xA0
		ConstraintDestroyedSignal      constraintDestroyed;       // 0xA8
		ConstraintEnabledSignal        constraintEnabled;         // 0xB0
		ConstraintDisabledSignal       constraintDisabled;        // 0xB8
		ImmediateConstraintAddedSignal immediateConstraintAdded;  // 0xC0
		PreCollideSignal               preCollide;                // 0xC8
		PostCollideSignal              postCollide;               // 0xD0
		PreSolveSignal                 preSolve;                  // 0xD8
		PostSolveSignal                postSolve;                 // 0xE0
	};
	static_assert(sizeof(hknpWorldSignals) == 0xE8);
}
