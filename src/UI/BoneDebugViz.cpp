#include "UI/BoneDebugViz.h"

#include <imgui.h>

namespace BoneDebugViz
{
	namespace
	{
		// ------------------------------------------------------------------
		// Shared state (render thread writes toggles, game thread reads them
		// and writes label positions back).
		// ------------------------------------------------------------------
		struct LabelDraw
		{
			std::string text;
			float       x{ 0.f };  // normalized [0,1], left -> right
			float       y{ 0.f };  // normalized [0,1], bottom -> top (NDC style)
		};

		std::mutex                 s_stateMutex;
		std::set<std::string>      s_highlightBones;  // exact strings from the bone list UI
		std::map<std::string, int> s_labelModes;      // 0 off / 1 joint / 2 mesh
		std::vector<LabelDraw>     s_labelDraws;      // produced by game tick, consumed by DrawLabels

		// Diagnostics: bumped on every toggle so the game tick logs one full
		// resolution trace right after the user clicks a button, plus a slow
		// heartbeat while anything stays active.
		std::atomic<uint32_t> s_stateGeneration{ 0 };

		// ------------------------------------------------------------------
		// Game-thread-only bookkeeping (never touched by the render thread).
		// ------------------------------------------------------------------
		struct GeomSnapshot
		{
			RE::NiColor color{};             // original emissive color (if the mesh had one)
			float       scale{ 0.f };        // original fEmitColorScale
			bool        hadColorPtr{ false };
		};

		// original emissive values, keyed by geometry pointer. Entries are
		// only ever dereferenced for geometries re-found in the CURRENT scene
		// walk (pointer identity match), so stale pointers of freed meshes
		// are never written through.
		std::unordered_map<void*, GeomSnapshot> s_geomOriginals;

		// which geometry pointers each highlighted bone currently tints —
		// used to restore exactly those on toggle-off.
		std::map<std::string, std::vector<void*>> s_tintedByBone;

		constexpr float kHighlightR = 0.0f;
		constexpr float kHighlightG = 1.0f;
		constexpr float kHighlightB = 0.0f;
		constexpr float kHighlightScale = 5.0f;  // emissive intensity — bright, clearly visible glow

		// ------------------------------------------------------------------
		// BSLightingShaderProperty emissive accessors.
		//
		// The vendored CommonLibF4 does not define BSLightingShaderProperty,
		// so these use the same verified offsets as HeadshotsKillF4SE's
		// ScenegraphUtils (checked against F4SE 0.7.7, the f4se-code fork and
		// NAF; identical pre-NG and post-NG):
		//   BSShaderProperty base size          = 0x70 (matches our fork)
		//   0xB8  NiColor* pEmissiveColor
		//   0xC8  float    fEmitColorScale
		// A NiRTTI name check gates every access so non-lighting shaders
		// (effect/particle materials) are never poked.
		// ------------------------------------------------------------------
		bool IsLightingShader(RE::NiProperty* a_prop)
		{
			if (!a_prop) {
				return false;
			}
			const auto* rtti = a_prop->GetRTTI();
			return rtti && rtti->name && std::strcmp(rtti->name, "BSLightingShaderProperty") == 0;
		}

		RE::NiColor** GetEmissiveColorPtr(RE::NiProperty* a_prop)
		{
			return reinterpret_cast<RE::NiColor**>(reinterpret_cast<std::uint8_t*>(a_prop) + 0xB8);
		}

		float& GetEmitColorScale(RE::NiProperty* a_prop)
		{
			return *reinterpret_cast<float*>(reinterpret_cast<std::uint8_t*>(a_prop) + 0xC8);
		}

		// ------------------------------------------------------------------
		// Scene graph helpers (game thread only).
		// ------------------------------------------------------------------

		// Result of resolving a bone name against one skeleton root. FO4
		// character skeletons are BSFlattenedBoneTree: most bones live in a
		// flat array + name hash instead of as nested child nodes, so a bone
		// can have a valid world transform without any scene-graph node.
		struct ResolvedBone
		{
			RE::NiAVObject* node{ nullptr };   // scene node, when one exists
			bool            hasWorld{ false };
			bool            firstPerson{ false };  // resolved from the 1st-person skeleton
			RE::NiPoint3    world{};
			const char*     via{ "none" };     // diagnostics: which path resolved it
		};

		ResolvedBone ResolveBone(RE::NiAVObject* a_root, const char* a_name)
		{
			ResolvedBone res;
			if (!a_root) {
				return res;
			}

			// Engine-side recursive lookup (virtual NiAVObject::GetObjectByName;
			// the NiNode override searches children). Runtime-proven with this
			// same CommonLibF4 fork by MagnaScopes, which resolves nodes inside
			// the attached weapon graph from the 1st-person root this way.
			const RE::BSFixedString fixedName(a_name);
			if (auto* obj = a_root->GetObjectByName(fixedName)) {
				res.node = obj;
				res.world = obj->world.translate;
				res.hasWorld = true;
				res.via = "engine";
				return res;
			}

			// Flattened-bone-tree fallback: look the name up in the flat bone
			// map. Gives the bone's world transform even when the bone has no
			// scene node, plus its attach node when one exists.
			if (const auto* rtti = a_root->GetRTTI(); rtti && rtti->name &&
				std::strcmp(rtti->name, "BSFlattenedBoneTree") == 0) {
				auto* tree = static_cast<RE::BSFlattenedBoneTree*>(a_root);
				if (tree->bone && tree->boneCountExpanded > 0) {
					auto it = tree->boneMap.find(fixedName);
					if (it != tree->boneMap.end()) {
						const auto idx = it->second;
						if (idx >= 0 && idx < tree->boneCountExpanded) {
							auto& flatBone = tree->bone[idx];
							res.node = flatBone.node.get();
							res.world = flatBone.world.translate;
							res.hasWorld = true;
							res.via = "flat";
						}
					}
				}
			}
			return res;
		}

		// Visits every BSGeometry under a_obj whose shader is a lighting
		// shader. The callback receives the geometry and its shader property.
		template <typename Fn>
		void ForEachLightingGeom(RE::NiAVObject* a_obj, Fn& a_fn)
		{
			if (!a_obj) {
				return;
			}
			if (auto* geom = a_obj->IsGeometry()) {
				// properties[1] is the shader property slot (0x138), same
				// layout HeadshotsKill reads by raw offset.
				if (auto* prop = geom->properties[1].get(); IsLightingShader(prop)) {
					a_fn(geom, prop);
				}
				return;
			}
			if (auto* node = a_obj->IsNode()) {
				for (std::uint32_t i = 0; i < node->children.size(); ++i) {
					if (auto* child = node->children[i].get()) {
						ForEachLightingGeom(child, a_fn);
					}
				}
			}
		}

		RE::BSGeometry* FindFirstGeometry(RE::NiAVObject* a_obj)
		{
			if (!a_obj) {
				return nullptr;
			}
			if (auto* geom = a_obj->IsGeometry()) {
				return geom;
			}
			if (auto* node = a_obj->IsNode()) {
				for (std::uint32_t i = 0; i < node->children.size(); ++i) {
					if (auto* found = FindFirstGeometry(node->children[i].get())) {
						return found;
					}
				}
			}
			return nullptr;
		}

		// First-person points -> normalized screen position. FP geometry is
		// camera-relative and rendered with the dedicated 1st-person FOV, so
		// the world camera's worldToCam matrix does NOT apply to it (measured
		// failure: labels projected to norm X of 1.4-2.2, off-screen right).
		// Ported from MagnaScopes' runtime-verified WorldPointToScreen: view
		// space is cameraNode.rotate * (point - cameraNode.translate) with
		// -Z forward, projected with firstPersonFOV (vertical) and the
		// renderer window aspect.
		bool FirstPersonPointToScreenNorm(RE::NiAVObject* a_root1st, const RE::NiPoint3& a_point, float& a_nx, float& a_ny)
		{
			if (!a_root1st) {
				return false;
			}
			const RE::BSFixedString camName("Camera");
			auto* camNode = a_root1st->GetObjectByName(camName);
			if (!camNode) {
				return false;
			}

			auto* window = RE::BSGraphics::GetCurrentRendererWindow();
			if (!window || window->windowWidth <= 0 || window->windowHeight <= 0) {
				return false;
			}
			const float aspect = static_cast<float>(window->windowWidth) / static_cast<float>(window->windowHeight);

			auto* pcam = RE::PlayerCamera::GetSingleton();
			float fovDeg = pcam ? pcam->firstPersonFOV : 90.0f;
			if (!(fovDeg > 1.0f && fovDeg < 179.0f)) {
				fovDeg = 90.0f;
			}
			const float halfHeight = std::tan(fovDeg * (3.14159265358979f / 180.0f) * 0.5f);

			const RE::NiPoint3 delta = a_point - camNode->world.translate;
			const RE::NiPoint3 view = camNode->world.rotate * delta;
			const float forward = -view.z;
			if (!(forward > 0.001f) || !(halfHeight > 0.0f)) {
				return false;
			}

			const float ndcX = -view.x / (forward * halfHeight * aspect);
			const float ndcY = -view.y / (forward * halfHeight);
			a_nx = (ndcX + 1.0f) * 0.5f;
			a_ny = (ndcY + 1.0f) * 0.5f;
			return true;
		}

		// World -> normalized screen position. Same math NAF uses for its 3D
		// gizmos (worldToCam row-vector projection + perspective divide);
		// avoids the WorldPtToScreenPt3 engine call entirely. Only valid for
		// points in the MAIN world scene (3rd-person skeleton) — see the
		// first-person variant above.
		bool WorldToScreenNorm(const RE::NiPoint3& a_world, float& a_nx, float& a_ny)
		{
			auto* cam = RE::Main::WorldRootCamera();
			if (!cam) {
				return false;
			}
			const auto& m = cam->worldToCam;

			const float trace = a_world.x * m[3][0] + a_world.y * m[3][1] + a_world.z * m[3][2] + m[3][3];
			if (trace <= 0.00001f) {
				return false;  // behind the camera
			}
			const float inv = 1.0f / trace;
			const float x = (a_world.x * m[0][0] + a_world.y * m[0][1] + a_world.z * m[0][2] + m[0][3]) * inv;
			const float y = (a_world.x * m[1][0] + a_world.y * m[1][1] + a_world.z * m[1][2] + m[1][3]) * inv;
			a_nx = (x + 1.0f) * 0.5f;
			a_ny = (y + 1.0f) * 0.5f;
			return true;
		}

		struct HighlightStats
		{
			int lightingGeoms{ 0 };   // geometries with a BSLightingShaderProperty
			int emissiveNull{ 0 };    // of those, how many have no emissive color allocated
		};

		// Applies the green tint to every lighting-shader mesh under a_node,
		// snapshotting original emissive values the first time each geometry
		// is seen. Records tinted geometry pointers into a_outTinted.
		void ApplyHighlight(RE::NiAVObject* a_node, std::vector<void*>& a_outTinted, HighlightStats& a_stats)
		{
			auto visitor = [&](RE::BSGeometry* a_geom, RE::NiProperty* a_prop) {
				RE::NiColor** emissive = GetEmissiveColorPtr(a_prop);
				a_stats.lightingGeoms++;
				if (!emissive || !*emissive) a_stats.emissiveNull++;
				if (s_geomOriginals.find(a_geom) == s_geomOriginals.end()) {
					GeomSnapshot snap;
					snap.scale = GetEmitColorScale(a_prop);
					if (emissive && *emissive) {
						snap.hadColorPtr = true;
						snap.color = **emissive;
					}
					s_geomOriginals.emplace(a_geom, snap);
				}
				if (emissive && *emissive) {
					(*emissive)->r = kHighlightR;
					(*emissive)->g = kHighlightG;
					(*emissive)->b = kHighlightB;
				}
				GetEmitColorScale(a_prop) = kHighlightScale;
				a_outTinted.push_back(a_geom);
			};
			ForEachLightingGeom(a_node, visitor);
		}

		// Restores original emissive values for meshes under a_node that we
		// previously snapshotted. Only geometries re-found in this walk are
		// touched, so freed pointers are never dereferenced.
		void RestoreHighlight(RE::NiAVObject* a_node)
		{
			auto visitor = [&](RE::BSGeometry* a_geom, RE::NiProperty* a_prop) {
				auto it = s_geomOriginals.find(a_geom);
				if (it == s_geomOriginals.end()) {
					return;
				}
				RE::NiColor** emissive = GetEmissiveColorPtr(a_prop);
				if (it->second.hadColorPtr && emissive && *emissive) {
					**emissive = it->second.color;
				}
				GetEmitColorScale(a_prop) = it->second.scale;
				s_geomOriginals.erase(it);
			};
			ForEachLightingGeom(a_node, visitor);
		}

		// Picks which skeleton a label should track: prefer the 1st-person
		// bone while the 1st-person skeleton is visible (its root is
		// app-culled in 3rd person view), else the 3rd-person bone.
		ResolvedBone PickLabelBone(RE::NiAVObject* a_root1st, RE::NiAVObject* a_root3rd, const char* a_bone)
		{
			if (a_root1st && !a_root1st->GetAppCulled()) {
				if (auto res = ResolveBone(a_root1st, a_bone); res.hasWorld) {
					res.firstPerson = true;
					return res;
				}
			}
			return ResolveBone(a_root3rd, a_bone);
		}
	}

	// ======================================================================
	// Render-thread API (UI buttons)
	// ======================================================================
	void ToggleHighlight(const std::string& a_boneName)
	{
		bool nowOn = false;
		{
			std::lock_guard lock(s_stateMutex);
			if (!s_highlightBones.erase(a_boneName)) {
				s_highlightBones.insert(a_boneName);
				nowOn = true;
			}
		}
		s_stateGeneration.fetch_add(1, std::memory_order_relaxed);
		logger::info("[OAR-BoneViz] Highlight '{}' -> {}", a_boneName, nowOn ? "ON" : "OFF");
		// Removal is handled by the next game tick, which diffs the desired
		// set against s_tintedByBone and restores dropped bones there.
	}

	bool IsHighlighted(const std::string& a_boneName)
	{
		std::lock_guard lock(s_stateMutex);
		return s_highlightBones.contains(a_boneName);
	}

	void CycleLabel(const std::string& a_boneName)
	{
		int newMode = 0;
		{
			std::lock_guard lock(s_stateMutex);
			int& mode = s_labelModes[a_boneName];
			mode = (mode + 1) % 3;
			newMode = mode;
			if (mode == 0) {
				s_labelModes.erase(a_boneName);
			}
		}
		s_stateGeneration.fetch_add(1, std::memory_order_relaxed);
		logger::info("[OAR-BoneViz] Label '{}' -> mode {} (0=off 1=joint 2=mesh)", a_boneName, newMode);
	}

	int GetLabelMode(const std::string& a_boneName)
	{
		std::lock_guard lock(s_stateMutex);
		auto it = s_labelModes.find(a_boneName);
		return it != s_labelModes.end() ? it->second : 0;
	}

	bool HasActiveLabels()
	{
		std::lock_guard lock(s_stateMutex);
		return !s_labelModes.empty();
	}

	// ======================================================================
	// Game-thread tick
	// ======================================================================
	void OnGameTick()
	{
		// Copy the desired state under lock, then do all scene-graph work
		// without holding it.
		std::set<std::string>      wantHighlight;
		std::map<std::string, int> wantLabels;
		{
			std::lock_guard lock(s_stateMutex);
			wantHighlight = s_highlightBones;
			wantLabels = s_labelModes;
		}

		// Fast exit while the feature is idle (the common case).
		if (wantHighlight.empty() && wantLabels.empty() && s_tintedByBone.empty()) {
			std::lock_guard lock(s_stateMutex);
			s_labelDraws.clear();
			return;
		}

		// Diagnostic trace: once right after any button toggle, then a slow
		// heartbeat (~every 10 s at 60 fps) while anything stays active.
		static uint32_t s_lastLoggedGen = 0;
		static uint32_t s_ticksSinceLog = 0;
		const uint32_t  gen = s_stateGeneration.load(std::memory_order_relaxed);
		bool trace = false;
		if (gen != s_lastLoggedGen || ++s_ticksSinceLog >= 600) {
			s_lastLoggedGen = gen;
			s_ticksSinceLog = 0;
			trace = true;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		RE::NiAVObject* root1st = player ? player->Get3D(true) : nullptr;
		RE::NiAVObject* root3rd = player ? player->Get3D(false) : nullptr;
		if (root3rd == root1st) {
			root3rd = nullptr;  // avoid double work if both resolve to the same graph
		}

		if (trace) {
			logger::info("[OAR-BoneViz] Tick: player={} root1st={:X} ('{}') root3rd={:X} ('{}') highlights={} labels={}",
				player ? "ok" : "NULL",
				reinterpret_cast<uintptr_t>(root1st),
				root1st && root1st->name.c_str() ? root1st->name.c_str() : "?",
				reinterpret_cast<uintptr_t>(root3rd),
				root3rd && root3rd->name.c_str() ? root3rd->name.c_str() : "?",
				wantHighlight.size(), wantLabels.size());
		}

		// --- restore bones that were toggled off (or whose highlight is gone) ---
		for (auto it = s_tintedByBone.begin(); it != s_tintedByBone.end();) {
			if (wantHighlight.contains(it->first)) {
				++it;
				continue;
			}
			for (auto* root : { root1st, root3rd }) {
				if (auto res = ResolveBone(root, it->first.c_str()); res.node) {
					RestoreHighlight(res.node);
				}
			}
			// Snapshots of geometries we could not re-find belong to freed or
			// rebuilt meshes; drop them so the map cannot grow unbounded.
			for (void* geom : it->second) {
				s_geomOriginals.erase(geom);
			}
			it = s_tintedByBone.erase(it);
		}

		// --- (re)apply active highlights every tick ---
		// Per-frame reapply makes the tint survive scene rebuilds (weapon
		// swaps, save loads) without tracking engine lifetimes: freshly
		// created geometries simply get snapshotted and tinted on the next
		// tick, and anything the engine overwrote is tinted again.
		for (const auto& bone : wantHighlight) {
			auto& tinted = s_tintedByBone[bone];
			tinted.clear();
			HighlightStats stats;
			const auto res1 = ResolveBone(root1st, bone.c_str());
			const auto res3 = ResolveBone(root3rd, bone.c_str());
			if (res1.node) ApplyHighlight(res1.node, tinted, stats);
			if (res3.node) ApplyHighlight(res3.node, tinted, stats);
			if (trace) {
				logger::info("[OAR-BoneViz]   Highlight '{}': 1st={} (node={:X}) 3rd={} (node={:X}) lightingGeoms={} emissiveNull={}",
					bone, res1.via, reinterpret_cast<uintptr_t>(res1.node),
					res3.via, reinterpret_cast<uintptr_t>(res3.node),
					stats.lightingGeoms, stats.emissiveNull);
			}
		}

		// --- compute label screen positions ---
		std::vector<LabelDraw> draws;
		draws.reserve(wantLabels.size());
		for (const auto& [bone, mode] : wantLabels) {
			const auto res = PickLabelBone(root1st, root3rd, bone.c_str());
			if (!res.hasWorld) {
				if (trace) {
					logger::info("[OAR-BoneViz]   Label '{}': bone NOT FOUND in either skeleton", bone);
				}
				continue;
			}

			RE::NiPoint3 worldPos;
			std::string  text;
			if (mode == 2) {
				// Mesh mode: the world-bound center of the first mesh attached
				// under the joint. Falls back to the joint if nothing is attached.
				if (auto* geom = res.node ? FindFirstGeometry(res.node) : nullptr) {
					worldPos = geom->worldBound.center;
					text = bone + " [mesh]";
				} else {
					worldPos = res.world;
					text = bone + " [no mesh]";
				}
			} else {
				worldPos = res.world;
				text = bone;
			}

			LabelDraw draw;
			// First-person bones live in camera-relative space and need the
			// 1st-person projection; world-space (3rd-person) bones go
			// through the world camera.
			const bool projected = res.firstPerson
				? FirstPersonPointToScreenNorm(root1st, worldPos, draw.x, draw.y)
				: WorldToScreenNorm(worldPos, draw.x, draw.y);
			if (trace) {
				logger::info("[OAR-BoneViz]   Label '{}': mode={} via={} fp={} world=({:.1f},{:.1f},{:.1f}) projected={} norm=({:.3f},{:.3f})",
					bone, mode, res.via, res.firstPerson, worldPos.x, worldPos.y, worldPos.z, projected, draw.x, draw.y);
			}
			if (projected) {
				draw.text = std::move(text);
				draws.push_back(std::move(draw));
			}
		}

		{
			std::lock_guard lock(s_stateMutex);
			s_labelDraws = std::move(draws);
		}
	}

	void OnSceneInvalidated()
	{
		// The current scene graph is going away; every snapshot pointer is
		// about to dangle. Toggles stay active and re-apply to the new scene
		// on the next tick.
		s_geomOriginals.clear();
		s_tintedByBone.clear();
	}

	// ======================================================================
	// Render-thread label drawing
	// ======================================================================
	void DrawLabels()
	{
		std::vector<LabelDraw> draws;
		{
			std::lock_guard lock(s_stateMutex);
			if (s_labelDraws.empty()) {
				return;
			}
			draws = s_labelDraws;
		}

		// Diagnostic: log when the number of drawable labels changes so the
		// render side of the pipeline is visible in the log too.
		static size_t s_lastDrawCount = 0;
		if (draws.size() != s_lastDrawCount) {
			s_lastDrawCount = draws.size();
			logger::info("[OAR-BoneViz] DrawLabels: {} label(s) reaching the render thread", draws.size());
		}

		const ImVec2 display = ImGui::GetIO().DisplaySize;
		ImDrawList*  dl = ImGui::GetForegroundDrawList();

		constexpr ImU32 kMarkerColor = IM_COL32(0, 255, 0, 230);
		constexpr ImU32 kTextColor = IM_COL32(120, 255, 120, 255);
		constexpr ImU32 kShadowColor = IM_COL32(0, 0, 0, 220);

		for (const auto& draw : draws) {
			// y is NDC-style (up = 1); screen pixels grow downward.
			const float px = draw.x * display.x;
			const float py = (1.0f - draw.y) * display.y;
			if (px < -50.f || px > display.x + 50.f || py < -50.f || py > display.y + 50.f) {
				continue;
			}

			dl->AddCircleFilled(ImVec2(px, py), 3.5f, kMarkerColor);
			const ImVec2 textPos(px + 7.f, py - 8.f);
			dl->AddText(ImVec2(textPos.x + 1.f, textPos.y + 1.f), kShadowColor, draw.text.c_str());
			dl->AddText(textPos, kTextColor, draw.text.c_str());
		}
	}
}
