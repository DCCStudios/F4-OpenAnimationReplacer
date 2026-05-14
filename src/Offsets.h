#pragma once

namespace Offsets
{
	// ===== Verified Havok vtable REL::IDs (from CommonLibF4 VTABLE_IDs.h) =====

	// hkbClipGenerator vtable — REL::ID(1360555)
	inline REL::Relocation<uintptr_t> hkbClipGenerator_vtbl{ REL::ID(1360555) };

	// hkbCharacterStringData vtable — REL::ID(931110)
	inline REL::Relocation<uintptr_t> hkbCharacterStringData_vtbl{ REL::ID(931110) };

	// ===== hkbClipGenerator vtable function indices (Havok SDK) =====
	// Verified via OAR-main FO4 port (Hooks.h) — same as Skyrim SE Havok SDK
	constexpr std::size_t ClipGen_Activate  = 0x4;
	constexpr std::size_t ClipGen_Update    = 0x5;
	constexpr std::size_t ClipGen_Deactivate = 0x7;
	constexpr std::size_t ClipGen_Generate  = 0x17;
	constexpr std::size_t ClipGen_StartEcho = 0x1B;

	// ===== BShkbAnimationGraph layout =====
	// BShkbAnimationGraph::character at offset 0x1C8 (verified by NAF static_assert)
	// hkbCharacter::behaviorGraph at offset 0x80 (verified by NAF static_assert)
	// BSAnimationGraphManager::variableCache.graphToCacheFor at offset 0xC0
	//   (BSAnimationGraphVariableCache starts at 0x88, graphToCacheFor at +0x38 = 0xC0)

	// ===== Actor update hook — proven from FPInertia =====
	inline REL::Relocation<uintptr_t> ptr_RunActorUpdates{ REL::ID(556439) };
	constexpr std::ptrdiff_t RunActorUpdates_Offset = 0x17;

	// ===== D3D11 hook — proven from GunMover/Shadow-Boost =====
	inline REL::Relocation<uintptr_t> ptr_D3D11CreateDevice{ REL::ID(224250) };
	constexpr std::ptrdiff_t D3D11Create_Offset = 0x419;

	// ===== ClipCursor IAT hook =====
	inline REL::Relocation<uintptr_t> ptr_ClipCursor{ REL::ID(641385) };

	// ===== BSAnimationGraphEvent source registration — from FPInertia =====
	inline REL::Relocation<uintptr_t> ptr_GetEventSourcePtrs{ REL::ID(897074) };

	// ===== hkbContext constructor — from NAF =====
	inline REL::Relocation<uintptr_t> ptr_hkbContextCtor{ REL::ID(1381136) };

	// ===== BShkbAnimationGraph::VisitGraph — from NAF =====
	inline REL::Relocation<uintptr_t> ptr_VisitGraph{ REL::ID(194777) };

	// ===== Graph traversal — from NAF =====
	// GraphTraverser::Next — REL::ID(849404)
	// GraphTraverser::Ctor — REL::ID(424303)

	// ===== Additional verified REL::IDs from CommonLibF4 =====
	// RTTI::hkbClipGenerator — REL::ID(586430)
	// VTABLE::hkbGenerator — REL::ID(109700)
	// VTABLE::hkbBehaviorGraph — REL::ID(476513)
	// VTABLE::hkbAnimationBindingSet — REL::ID(802975)
	// VTABLE::BShkbAnimationGraph — REL::ID(742655)
}
