#pragma once

#include "RE/B/BSExtraData.h"
#include "RE/B/BSIntrusiveRefCounted.h"
#include "RE/B/BSSpinLock.h"
#include "RE/B/BSTSmartPointer.h"
#include "RE/B/BaseExtraList.h"
#include "RE/E/EXTRA_DATA_TYPE.h"
#include "RE/M/MemoryManager.h"
#include "RE/T/TBO_InstanceData.h"

namespace RE
{
	class BGSObjectInstanceExtra;
	class BGSPrimitive;
	class NiPoint3;

	namespace detail
	{
		template <class T>
		concept ExtraDataListConstraint =
			std::derived_from<T, BSExtraData> &&
			!std::is_pointer_v<T> &&
			!std::is_reference_v<T>;
	}

	class ExtraDataList :
		public BSIntrusiveRefCounted  // 00
	{
	public:
		F4_HEAP_REDEFINE_NEW(ExtraDataList);

		using ComparisonQualifier = bool (*)(const BSExtraData*);

		enum class CLEAR_FOR
		{
			kContainer = 0x0,
			kReference = 0x1
		};

		ExtraDataList()
		{
			using func_t = void (*)(ExtraDataList*);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::Ctor };
			func(this);
		}

		inline void AddExtra(BSExtraData* a_extra)
		{
			const BSAutoWriteLock l{ extraRWLock };
			extraData.AddExtra(a_extra);
		}

		inline void CopyList(const ExtraDataList* a_copy) noexcept
		{
			using func_t = decltype(&ExtraDataList::CopyList);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::CopyList };
			func(this, a_copy);
		}

		inline TBO_InstanceData* CreateInstanceData(TESBoundObject* a_object, bool a_generateName)
		{
			using func_t = decltype(&ExtraDataList::CreateInstanceData);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::CreateInstanceData };
			return func(this, a_object, a_generateName);
		}

		[[nodiscard]] BSExtraData* GetByType(EXTRA_DATA_TYPE a_type) const noexcept
		{
			const BSAutoReadLock l{ extraRWLock };
			return extraData.GetByType(a_type);
		}

		template <detail::ExtraDataListConstraint T>
		[[nodiscard]] T* GetByType() const noexcept
		{
			return static_cast<T*>(GetByType(T::TYPE));
		}

		[[nodiscard]] BGSMod::Attachment::Mod* GetLegendaryMod()
		{
			using func_t = decltype(&ExtraDataList::GetLegendaryMod);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::GetLegendaryMod };
			return func(this);
		}

		[[nodiscard]] bool HasType(EXTRA_DATA_TYPE a_type) const noexcept
		{
			const BSAutoReadLock l{ extraRWLock };
			return extraData.HasType(a_type);
		}

		template <detail::ExtraDataListConstraint T>
		[[nodiscard]] bool HasType() const noexcept
		{
			return HasType(T::TYPE);
		}

		std::unique_ptr<BSExtraData> RemoveExtra(EXTRA_DATA_TYPE a_type)
		{
			const BSAutoWriteLock l{ extraRWLock };
			return extraData.RemoveExtra(a_type);
		}

		template <detail::ExtraDataListConstraint T>
		inline std::unique_ptr<T> RemoveExtra()
		{
			return std::unique_ptr<T>{ static_cast<T*>(RemoveExtra(T::TYPE).release()) };
		}

		inline bool SetBendableSplineInfo(float* a_thickness, float* a_slack, NiPoint3* a_halfExtents = nullptr, bool* a_detachedEnd = nullptr)
		{
			using func_t = decltype(&ExtraDataList::SetBendableSplineInfo);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::SetBendableSplineInfo };
			return func(this, a_thickness, a_slack, a_halfExtents, a_detachedEnd);
		}

		inline void SetCount(std::int16_t a_count) noexcept
		{
			using func_t = decltype(&ExtraDataList::SetCount);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::SetCount };
			func(this, a_count);
		}

		inline void SetDisplayNameFromInstanceData(BGSObjectInstanceExtra* a_instExtra, TESBoundObject* a_object, const BSTSmartPointer<TBO_InstanceData>& a_data)
		{
			using func_t = decltype(&ExtraDataList::SetDisplayNameFromInstanceData);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::SetDisplayNameFromInstanceData };
			func(this, a_instExtra, a_object, a_data);
		}

		inline void SetOverrideName(const char* a_name)
		{
			using func_t = decltype(&ExtraDataList::SetOverrideName);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::SetOverrideName };
			func(this, a_name);
		}

		inline void SetStartingWorldOrCell(TESForm* a_form)
		{
			using func_t = decltype(&ExtraDataList::SetStartingWorldOrCell);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::SetStartingWorldOrCell };
			func(this, a_form);
		}

		inline float GetHealthPerc()
		{
			using func_t = decltype(&ExtraDataList::GetHealthPerc);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::GetHealthPerc };
			return func(this);
		}

		inline void SetHealthPerc(float a_healthPerc)
		{
			using func_t = decltype(&ExtraDataList::SetHealthPerc);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::SetHealthPerc };
			func(this, a_healthPerc);
		}

		inline bool ClearFavorite()
		{
			using func_t = decltype(&ExtraDataList::ClearFavorite);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::ClearFavorite };
			return func(this);
		}

		inline bool IsFavorite()
		{
			using func_t = decltype(&ExtraDataList::IsFavorite);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::IsFavorite };
			return func(this);
		}

		inline bool IsDamaged()
		{
			using func_t = decltype(&ExtraDataList::IsDamaged);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::IsDamaged };
			return func(this);
		}

		inline bool CompareList(const ExtraDataList* a_compare, ComparisonQualifier a_qualifier)
		{
			using func_t = decltype(&ExtraDataList::CompareList);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::CompareList };
			return func(this, a_compare, a_qualifier);
		}

		inline void SetFavorite(char a_quickKeyIndex)
		{
			using func_t = decltype(&ExtraDataList::SetFavorite);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::SetFavorite };
			func(this, a_quickKeyIndex);
		}

		inline BGSPrimitive* GetPrimitive()
		{
			using func_t = decltype(&ExtraDataList::GetPrimitive);
			static REL::Relocation<func_t> func{ ID::ExtraDataList::GetPrimitive };
			return func(this);
		}

		// members
		BaseExtraList           extraData;    // 08
		mutable BSReadWriteLock extraRWLock;  // 20
	};
	static_assert(sizeof(ExtraDataList) == 0x28);
}
