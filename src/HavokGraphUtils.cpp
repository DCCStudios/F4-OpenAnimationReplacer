#include "HavokGraphUtils.h"
#include "Offsets.h"

namespace HavokGraphUtils
{
	void EnumerateActiveClips(
		RE::hkbBehaviorGraph* a_graph,
		std::vector<ActiveClipInfo>& a_outClips)
	{
		if (!a_graph || !a_graph->activeNodes) return;

		auto* activeNodes = a_graph->activeNodes;
		struct HkArrayAccessor {
			RE::hkbNodeInfo** data;
			int32_t size;
			int32_t capacityAndFlags;
		};

		auto& arr = reinterpret_cast<HkArrayAccessor&>(*activeNodes);
		if (!arr.data || arr.size <= 0) return;

		auto clipGenVtbl = Offsets::hkbClipGenerator_vtbl.address();

		for (int32_t i = 0; i < arr.size; i++) {
			auto* nodeInfo = arr.data[i];
			if (!nodeInfo) continue;

			auto* node = reinterpret_cast<RE::hkbNode*>(nodeInfo);
			if (!node) continue;

			uintptr_t nodeVtbl = *reinterpret_cast<uintptr_t*>(node);
			if (nodeVtbl != clipGenVtbl) continue;

			auto* clipGen = reinterpret_cast<RE::hkbClipGenerator*>(node);
			ActiveClipInfo info;
			info.clipGenerator = clipGen;
			info.node = node;
			info.nodeId = node->id;
			a_outClips.push_back(info);
		}
	}

	std::vector<AnnotationEntry> ReadAnnotations(
		RE::hkbClipGenerator* a_clipGen)
	{
		std::vector<AnnotationEntry> result;
		if (!a_clipGen) return result;

		// Annotation reading requires accessing hkaAnimation through the binding.
		// The binding is a runtime field at an offset past the declared struct.
		// For safety, we return empty until the exact offset is verified at runtime.
		// This is a placeholder for the HaBCR-pattern annotation reading.

		return result;
	}

	void OverwriteLocalTime(
		RE::hkbClipGenerator* a_clipGen,
		float a_newTime)
	{
		if (!a_clipGen) return;

		// HaBCR three-write pattern:
		// 1. clipGen->localTime (at declared offset)
		// 2. animationControl->localTime (runtime field at clipGen + animControl offset, then +0x10)
		// 3. Unknown field at clipGen + 0x151
		//
		// Direct field access for the declared field:
		// (localTime is a runtime field not in our declared struct, so we need
		// to access it via offset. Based on FO4 layout analysis, localTime
		// is approximately at offset 0x128 from the start of hkbClipGenerator.)
		//
		// For safety, only set the fields we can verify:

		// Placeholder -- needs runtime offset verification
		logger::debug("[OAR] OverwriteLocalTime called with t={}, needs offset verification", a_newTime);
	}

	int32_t GetCurrentStateMachineState(
		RE::hkbBehaviorGraph* a_graph)
	{
		if (!a_graph || !a_graph->activeNodes) return -1;

		// Walk active nodes looking for an hkbStateMachine node
		// and read its currentStateId field.
		// The hkbStateMachine vtable can be identified via REL::ID.
		//
		// This is a placeholder -- the exact vtable ID and currentStateId
		// offset need to be determined for FO4.

		return -1;
	}

	RE::hkbCharacter* GetCharacterFromGraph(
		RE::BShkbAnimationGraph* a_graph)
	{
		if (!a_graph) return nullptr;
		return &a_graph->character;
	}

	RE::hkbBehaviorGraph* GetBehaviorGraphForActor(
		RE::Actor* a_actor)
	{
		if (!a_actor) return nullptr;

		RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
		if (!a_actor->GetAnimationGraphManagerImpl(manager) || !manager) return nullptr;

		if (manager->graph.empty()) return nullptr;

		auto& character = manager->graph[manager->activeGraph]->character;
		return character.behaviorGraph._ptr;
	}
}
