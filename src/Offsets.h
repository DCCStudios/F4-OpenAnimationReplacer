#pragma once

// =============================================================================
// Offsets.h — Address Library anchors for OAR's own hooks.
// =============================================================================
// Multi-runtime notes (OG 1.10.163 / NG 1.10.980-984 / AE 1.11.x):
//
//  - The Havok/graph VTABLE ids below survived the NG and AE address-library
//    regeneration unchanged (RTTI-matched); a single-slot REL::ID resolves
//    correctly on every runtime. Verified against version-1-10-984-0.bin and
//    version-1-11-221-0.bin with tools/Check-ALIDs.ps1.
//
//  - Function/global ids measured on OG generally did NOT survive; those are
//    either multi-slot REL::ID{ og, ng, ae } at their call sites, or OG-only
//    and exposed here as lazily-resolving accessors so that merely loading
//    the DLL on NG/AE cannot trigger a missing-ID abort (namespace-scope
//    REL::Relocation globals resolve eagerly during static init).
// =============================================================================

namespace Offsets
{
	// ===== Verified Havok vtable REL::IDs (identical on OG/NG/AE) =====

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

	// ===== Actor update hook — proven from FPInertia. OG-ONLY =====
	// 556439 has no NG/AE address-library entry. UpdateHooks::Install uses
	// this trampoline anchor on OG and falls back to the F4SE permanent task
	// queue on NG/AE. Lazy accessor: resolve only when actually installing.
	[[nodiscard]] inline uintptr_t GetRunActorUpdatesAddr()
	{
		static REL::Relocation<uintptr_t> reloc{ REL::ID(556439) };
		return reloc.address();
	}
	constexpr std::ptrdiff_t RunActorUpdates_Offset = 0x17;

	// ===== D3D11 / ClipCursor =====
	// Handled entirely inside UIManager.cpp (F4SE Menu Framework 3 strategy):
	//  - D3D11CreateDeviceAndSwapChain: call-site REL::ID{ 224250, -, 4492363 }
	//    (+0x419 OG / +0x410 AE) and an import patch on NG.
	//  - ClipCursor / SetCursorPos: game-IAT walk by name, no ids needed.

	// ===== Additional verified REL::IDs from CommonLibF4 =====
	// RTTI::hkbClipGenerator — REL::ID(586430)
	// VTABLE::hkbGenerator — REL::ID(109700)
	// VTABLE::hkbBehaviorGraph — REL::ID(476513)
	// VTABLE::hkbAnimationBindingSet — REL::ID(802975)
	// VTABLE::BShkbAnimationGraph — REL::ID(742655)
	//
	// OG-only engine functions used elsewhere (all lazily resolved and gated
	// on REX::FModule::IsRuntimeOG() at their call sites):
	//   897074  BGSAnimationSystemUtils::GetEventSourcePointersFromGraph (HavokTypes.h)
	//   194777  BShkbAnimationGraph::VisitGraph (HavokTypes.h, diagnostics only)
	//   1381136/144578  hkbContext ctor/dtor (HavokTypes.h, unused)
	//   992878/326555   setActiveGeneratorLocalTime/getNodeClone (HavokTypes.h, unused)
	//   762973  LoadedIdleAnimData array (Hooks.cpp idle reverse map)
	//   943772  RequestDetectionLevel (RE_Additions.h)
}
