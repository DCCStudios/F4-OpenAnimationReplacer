#pragma once

#include "RE/B/BSFixedString.h"

namespace RE
{
	class BSFlattenedBoneTree;
	class NiAVObject;

	namespace BSUtilities
	{
		inline std::uint16_t ConvertFloatToHalf(float a_value)
		{
			using func_t = decltype(&BSUtilities::ConvertFloatToHalf);
			static REL::Relocation<func_t> func{ ID::BSUtilities::ConvertFloatToHalf };
			return func(a_value);
		}

		inline float ConvertHalfToFloat(std::uint16_t a_value)
		{
			using func_t = decltype(&BSUtilities::ConvertHalfToFloat);
			static REL::Relocation<func_t> func{ ID::BSUtilities::ConvertHalfToFloat };
			return func(a_value);
		}

		inline NiAVObject* GetObjectByName(NiAVObject* a_root, const BSFixedString& a_name, bool a_tryInternal, bool a_dontAttach)
		{
			using func_t = decltype(&BSUtilities::GetObjectByName);
			static REL::Relocation<func_t> func{ ID::BSUtilities::GetObjectByName };
			return func(a_root, a_name, a_tryInternal, a_dontAttach);
		}

		inline BSFlattenedBoneTree* GetFlattenedBoneTree(NiAVObject* a_root)
		{
			using func_t = decltype(&BSUtilities::GetFlattenedBoneTree);
			static REL::Relocation<func_t> func{ ID::BSUtilities::GetFlattenedBoneTree };
			return func(a_root);
		}
	}
}
