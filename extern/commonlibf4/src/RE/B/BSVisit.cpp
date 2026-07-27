#include "RE/B/BSVisit.h"

#include "RE/N/NiAVObject.h"
#include "RE/N/NiNode.h"

namespace RE::BSVisit
{
	BSVisitControl TraverseScenegraphGeometries(NiAVObject* a_object, std::function<BSVisitControl(BSGeometry*)> a_func)
	{
		if (a_object) {
			if (auto geom = a_object->IsGeometry())
				return a_func(geom);

			if (const auto node = a_object->IsNode()) {
				for (const auto& child : node->children) {
					if (TraverseScenegraphGeometries(child.get(), a_func) == BSVisitControl::kStop) {
						return BSVisitControl::kStop;
					}
				}
			}
		}

		return BSVisitControl::kContinue;
	}

	BSVisitControl TraverseScenegraphObjects(NiAVObject* a_object, std::function<BSVisitControl(NiAVObject*)> a_func)
	{
		if (a_object) {
			if (a_func(a_object) == BSVisitControl::kStop)
				return BSVisitControl::kStop;

			if (const auto node = a_object->IsNode()) {
				for (const auto& child : node->children) {
					if (TraverseScenegraphObjects(child.get(), a_func) == BSVisitControl::kStop) {
						return BSVisitControl::kStop;
					}
				}
			}
		}

		return BSVisitControl::kContinue;
	}

	BSVisitControl TraverseScenegraphCollision(const NiAVObject* a_object, std::function<BSVisitControl(bhkNPCollisionObject*)> a_func)
	{
		if (a_object) {
			if (const auto collision = a_object->GetCollisionObject()) {
				if (const auto collisionNP = collision->IsbhkNPCollisionObject()) {
					if (a_func(collisionNP) == BSVisitControl::kStop) {
						return BSVisitControl::kStop;
					}
				}
			}

			if (const auto node = a_object->IsNode()) {
				for (const auto& child : node->children) {
					if (TraverseScenegraphCollision(child.get(), a_func) == BSVisitControl::kStop) {
						return BSVisitControl::kStop;
					}
				}
			}
		}

		return BSVisitControl::kContinue;
	}
}
