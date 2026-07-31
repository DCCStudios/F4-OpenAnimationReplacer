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

	// ===== hkbClipGenerator vtable function indices (Havok 2014, FO4) =====
	// FO4 ships Havok 2014 — NOT Skyrim's 2010 layout. The lifecycle slots
	// shifted +3 relative to Skyrim while generate/startEcho stayed put.
	// (The old values 0x4/0x5/0x7, carried over from the Skyrim OAR port,
	// attached Activate/Update to inherited base virtuals and Deactivate to
	// the REAL activate — see GitHub issue #1 and the 2026-07-31 fix.)
	//
	// Measured directly from the AE 1.11.221 exe on disk
	// (tools/Verify-ClipGenVtbl.ps1 + Dump-VtblSlotBodies.ps1):
	//   slot 4/5 = pointer-identical to hkbBehaviorGraph's vtable slots 4/5
	//              (inherited hkbBindable/hkbNode defaults — not per-class)
	//   slot 7   = activate   — body has a RIP-relative LEA to the
	//              "Invalid clip generator detected" hkError string
	//   slot 8   = update     — per-class override, 3rd arg float in XMM2
	//              (matches update(const hkbContext&, hkReal timestep))
	//   slot 10  = deactivate — per-class override, no float, clears members
	//   slot 23  = generate   — 0x17, same as Skyrim (unchanged)
	//   slot 27  = startEcho  — 0x1B, the ONLY per-class override with a
	//              two-arg (this, float-in-XMM1) signature (unchanged)
	// Same layout on NG 1.10.984 and OG 1.10.163 (issue #1 measured the OG
	// vtable bytes at REL::ID(1360555) = 0x142dce288 and NG's slot bodies).
	// ClipGeneratorHooks::Install() re-verifies the activate slot against
	// the LOADED image at startup (string-anchor guard) and logs loudly if
	// a future runtime shuffles the table.
	constexpr std::size_t ClipGen_Activate  = 0x7;
	constexpr std::size_t ClipGen_Update    = 0x8;
	constexpr std::size_t ClipGen_Deactivate = 0xA;
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
