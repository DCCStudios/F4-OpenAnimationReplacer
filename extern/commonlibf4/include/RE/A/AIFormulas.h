#pragma once

namespace RE
{
	namespace AIFormulas
	{
		inline float GetBarterValue(float a_baseValue, float a_charisma, bool a_selling, TESObjectREFR* a_refTarget)
		{
			using func_t = decltype(&AIFormulas::GetBarterValue);
			static REL::Relocation<func_t> func{ ID::AIFormulas::GetBarterValue };
			return func(a_baseValue, a_charisma, a_selling, a_refTarget);
		}

		inline std::uint32_t ComputePickpocketSuccess(float a_thiefSkill, float a_targetSkill, std::int32_t a_valueStolen, float a_weightStolen, Actor* a_thief, Actor* a_target, TESForm* a_itemPickpocketing, bool a_placingItem)
		{
			using func_t = decltype(&AIFormulas::ComputePickpocketSuccess);
			static REL::Relocation<func_t> func{ ID::AIFormulas::ComputePickpocketSuccess };
			return func(a_thiefSkill, a_targetSkill, a_valueStolen, a_weightStolen, a_thief, a_target, a_itemPickpocketing, a_placingItem);
		}
	}
}
