#pragma once

// ============================================================================
// BoneDebugViz — session-only visual debugging aids for the track filter UI.
//
// Two per-bone toggles, driven by small buttons next to each bone in the
// track filtering section:
//
//   1. Mesh highlight: tints every mesh attached under the named node bright
//      green via the BSLightingShaderProperty emissive channel (the same
//      technique as HeadshotsKillF4SE's TintScenegraph — works on any lit
//      mesh without needing a TESEffectShader form). Original emissive
//      values are snapshotted per geometry and restored on toggle-off.
//
//   2. 3D label: draws the bone name as floating text projected into screen
//      space. Cycles through three states per click:
//        off -> at the joint's world position -> at the attached mesh's
//        world-bound center -> off.
//
// Neither toggle is persisted; all state lives in process memory and dies
// with the game session.
//
// THREADING CONTRACT
//   - Toggle*/Cycle*/Is*/Get* are called from the render thread (ImGui).
//   - OnGameTick() must run on the game thread (called from
//     Hooks::HookedActorUpdate) — it is the only code that touches the
//     scene graph and shader properties.
//   - DrawLabels() runs on the render thread and only consumes screen-space
//     positions computed by the last game tick.
// ============================================================================

#include <string>

namespace BoneDebugViz
{
	// --- render thread (UI buttons) ---
	void ToggleHighlight(const std::string& a_boneName);
	bool IsHighlighted(const std::string& a_boneName);

	// Label mode: 0 = off, 1 = at joint position, 2 = at attached mesh center.
	void CycleLabel(const std::string& a_boneName);
	int  GetLabelMode(const std::string& a_boneName);

	// True while any label is active — UIManager uses this to keep the ImGui
	// frame alive when no windows are open.
	bool HasActiveLabels();

	// --- game thread ---
	// Applies/maintains highlights and computes label screen positions.
	// Called once per frame from the actor update hook.
	void OnGameTick();

	// Drops per-geometry bookkeeping when the loaded scene is about to be
	// torn down (save load). Active toggles stay on and re-apply to the new
	// scene graph; only the stale restore snapshots are discarded.
	void OnSceneInvalidated();

	// --- render thread ---
	// Draws active labels into the ImGui foreground draw list.
	void DrawLabels();
}
