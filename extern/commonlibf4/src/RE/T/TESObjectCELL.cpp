#include "RE/T/TESObjectCELL.h"

#include "RE/E/EXTERIOR_DATA.h"
#include "RE/E/ExtraCellWaterType.h"
#include "RE/E/ExtraDataList.h"
#include "RE/T/TESObjectREFR.h"
#include "RE/T/TESWorldSpace.h"

namespace RE
{
	void TESObjectCELL::ForEachReference(std::function<BSContainer::ForEachResult(TESObjectREFR*)> a_callback)
	{
		const BSAutoLock locker(spinLock);
		for (const auto& ref : references) {
			if (ref && a_callback(ref.get()) == BSContainer::ForEachResult::kStop) {
				break;
			}
		}
	}

	void TESObjectCELL::ForEachReferenceInRange(const NiPoint3& a_origin, float a_radius, std::function<BSContainer::ForEachResult(TESObjectREFR*)> a_callback)
	{
		const float squaredRadius = a_radius * a_radius;
		ForEachReference([&](TESObjectREFR* a_ref) {
			const auto distance = a_origin.GetSquaredDistance(a_ref->GetPosition());
			if (distance <= squaredRadius)
				return a_callback(a_ref);

			return BSContainer::ForEachResult::kContinue;
		});
	}

	EXTERIOR_DATA* TESObjectCELL::GetCoordinates() const
	{
		return IsExterior() ? cellData.exterior : nullptr;
	}

	TESWaterForm* TESObjectCELL::GetWaterType() const noexcept
	{
		const auto xWater = extraList ? extraList->GetByType<ExtraCellWaterType>() : nullptr;
		auto       water = xWater ? xWater->water : nullptr;
		if (!water) {
			water = IsExterior() && worldSpace ? worldSpace->GetWaterType() : nullptr;
			if (!water) {
				static REL::Relocation<TESWaterForm**> defaultWater{ ID::TESObjectCELL::DefaultWater };
				water = *defaultWater;
			}
		}

		return water;
	}
}
