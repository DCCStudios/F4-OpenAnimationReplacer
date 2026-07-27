#pragma once

namespace RE
{
	class bhkNPCollisionObject;
	class BSGeometry;
	class NiAVObject;

	enum class BSVisitControl
	{
		kContinue = 0x0,
		kStop = 0x1
	};

	namespace BSVisit
	{
		BSVisitControl TraverseScenegraphGeometries(NiAVObject* a_object, std::function<BSVisitControl(BSGeometry*)> a_func);
		BSVisitControl TraverseScenegraphObjects(NiAVObject* a_object, std::function<BSVisitControl(NiAVObject*)> a_func);
		BSVisitControl TraverseScenegraphCollision(const NiAVObject* a_object, std::function<BSVisitControl(bhkNPCollisionObject*)> a_func);
	}
}
