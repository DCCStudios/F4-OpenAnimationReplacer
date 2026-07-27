#pragma once

#include "RE/B/BGSObjectInstance.h"
#include "RE/T/TESObjectWEAP.h"

namespace RE
{
	namespace CombatFormulas
	{
		[[nodiscard]] inline float GetWeaponDisplayAccuracy(const BGSObjectInstanceT<TESObjectWEAP>& a_weapon, Actor* a_actor)
		{
			using func_t = decltype(&CombatFormulas::GetWeaponDisplayAccuracy);
			static REL::Relocation<func_t> func{ ID::CombatFormulas::GetWeaponDisplayAccuracy };
			return func(a_weapon, a_actor);
		}

		[[nodiscard]] inline float GetWeaponDisplayDamage(const BGSObjectInstanceT<TESObjectWEAP>& a_weapon, const TESAmmo* a_ammo, float a_condition)
		{
			using func_t = decltype(&CombatFormulas::GetWeaponDisplayDamage);
			static REL::Relocation<func_t> func{ ID::CombatFormulas::GetWeaponDisplayDamage };
			return func(a_weapon, a_ammo, a_condition);
		}

		[[nodiscard]] inline float GetWeaponDisplayRange(const BGSObjectInstanceT<TESObjectWEAP>& a_weapon)
		{
			using func_t = decltype(&CombatFormulas::GetWeaponDisplayRange);
			static REL::Relocation<func_t> func{ ID::CombatFormulas::GetWeaponDisplayRange };
			return func(a_weapon);
		}

		[[nodiscard]] inline float GetWeaponDisplayRateOfFire(const TESObjectWEAP& a_weapon, const TESObjectWEAP::InstanceData* a_data)
		{
			using func_t = decltype(&CombatFormulas::GetWeaponDisplayRateOfFire);
			static REL::Relocation<func_t> func{ ID::CombatFormulas::GetWeaponDisplayRateOfFire };
			return func(a_weapon, a_data);
		}

		[[nodiscard]] inline float CalcTargetedLimbDamage(Actor* a_target, const BGSBodyPart* a_bodyPart, float a_physicalDamage, BSTArray<BSTTuple<TESForm*, BGSTypedFormValuePair::SharedVal>, BSTArrayHeapAllocator>* a_damageTypes)
		{
			using func_t = decltype(&CombatFormulas::CalcTargetedLimbDamage);
			static REL::Relocation<func_t> func{ ID::CombatFormulas::CalcWeaponDamage };
			return func(a_target, a_bodyPart, a_physicalDamage, a_damageTypes);
		}

		[[nodiscard]] inline std::int64_t GetNumCrippledAttackConditions(Actor* a_actor)
		{
			using func_t = decltype(&CombatFormulas::GetNumCrippledAttackConditions);
			static REL::Relocation<func_t> func{ ID::CombatFormulas::GetNumCrippledAttackConditions };
			return func(a_actor);
		}

		[[nodiscard]] inline float CalcScopeSteadyActionPointDrain(Actor* a_actor, float a_deltaSec)
		{
			using func_t = decltype(&CombatFormulas::CalcScopeSteadyActionPointDrain);
			static REL::Relocation<func_t> func{ ID::CombatFormulas::CalcScopeSteadyActionPointDrain };
			return func(a_actor, a_deltaSec);
		}

		[[nodiscard]] inline float CalcResistedPercentage(const ActorValueInfo* a_resistance, float a_damage, float a_resistancePoints)
		{
			using func_t = decltype(&CombatFormulas::CalcResistedPercentage);
			static REL::Relocation<func_t> func{ ID::CombatFormulas::CalcResistedPercentage };
			return func(a_resistance, a_damage, a_resistancePoints);
		}
	}
}
