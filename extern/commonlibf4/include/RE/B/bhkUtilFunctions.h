#pragma once

#include "RE/H/hknpBodyId.h"

namespace RE
{
	class bhkNPCollisionObject;
	class hknpBSWorld;
	class NiAVObject;

	namespace bhkUtilFunctions
	{
		inline bhkNPCollisionObject* FindFirstCollisionObject(const NiAVObject* a_object)
		{
			using func_t = decltype(&bhkUtilFunctions::FindFirstCollisionObject);
			static REL::Relocation<func_t> func{ ID::bhkUtilFunctions::FindFirstCollisionObject };
			return func(a_object);
		}

		inline NiAVObject* GetAVObjectFromBodyID(const hknpBSWorld* a_world, const hknpBodyId a_bodyID)
		{
			using func_t = decltype(&bhkUtilFunctions::GetAVObjectFromBodyID);
			static REL::Relocation<func_t> func{ ID::bhkUtilFunctions::GetAVObjectFromBodyID };
			return func(a_world, a_bodyID);
		}
	}
}
