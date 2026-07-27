#include "RE/T/TES.h"

#include "RE/E/EXTERIOR_DATA.h"
#include "RE/G/GridCellArray.h"
#include "RE/T/TESObjectCELL.h"
#include "RE/T/TESObjectREFR.h"
#include "RE/T/TESWorldSpace.h"

namespace RE
{
	void TES::ForEachReference(std::function<BSContainer::ForEachResult(TESObjectREFR* a_ref)> a_callback)
	{
		if (interiorCell) {
			interiorCell->ForEachReference([&](TESObjectREFR* a_ref) {
				return a_callback(a_ref);
			});
		} else {
			if (const auto gridLength = gridCells ? gridCells->dimension : 0; gridLength > 0) {
				std::uint32_t x = 0;
				do {
					std::uint32_t y = 0;
					do {
						if (const auto cell = gridCells->GetCell(x, y); cell && cell->IsAttached()) {
							cell->ForEachReference([&](TESObjectREFR* a_ref) {
								return a_callback(a_ref);
							});
						}
						++y;
					} while (y < gridLength);
					++x;
				} while (x < gridLength);
			}
		}
		if (const auto skyCell = worldSpace ? worldSpace->GetSkyCell() : nullptr; skyCell) {
			skyCell->ForEachReference([&](TESObjectREFR* a_ref) {
				return a_callback(a_ref);
			});
		}
	}

	void TES::ForEachReferenceInRange(const NiPoint3& a_origin, const float a_radius, std::function<BSContainer::ForEachResult(TESObjectREFR* a_ref)> a_callback)
	{
		if (a_radius > 0.0f) {
			if (interiorCell) {
				interiorCell->ForEachReferenceInRange(a_origin, a_radius, [&](TESObjectREFR* a_ref) {
					return a_callback(a_ref);
				});
			} else {
				if (const auto gridLength = gridCells ? gridCells->dimension : 0; gridLength > 0) {
					const float yPlus = a_origin.y + a_radius;
					const float yMinus = a_origin.y - a_radius;
					const float xPlus = a_origin.x + a_radius;
					const float xMinus = a_origin.x - a_radius;

					std::uint32_t x = 0;
					do {
						std::uint32_t y = 0;
						do {
							if (const auto cell = gridCells->GetCell(x, y); cell && cell->IsAttached()) {
								if (const auto cellCoords = cell->GetCoordinates(); cellCoords) {
									const NiPoint2 worldPos{ cellCoords->worldX, cellCoords->worldY };
									if (worldPos.x < xPlus && (worldPos.x + 4096.0f) > xMinus && worldPos.y < yPlus && (worldPos.y + 4096.0f) > yMinus) {
										cell->ForEachReferenceInRange(a_origin, a_radius, [&](TESObjectREFR* a_ref) {
											return a_callback(a_ref);
										});
									}
								}
							}
							++y;
						} while (y < gridLength);
						++x;
					} while (x < gridLength);
				}
			}

			if (const auto skyCell = worldSpace ? worldSpace->GetSkyCell() : nullptr; skyCell) {
				skyCell->ForEachReferenceInRange(a_origin, a_radius, [&](TESObjectREFR* a_ref) {
					return a_callback(a_ref);
				});
			}
		} else {
			ForEachReference([&](TESObjectREFR* a_ref) {
				return a_callback(a_ref);
			});
		}
	}

	void TES::ForEachReferenceInRange(const TESObjectREFR* a_ref, const float a_radius, std::function<BSContainer::ForEachResult(TESObjectREFR* a_ref)> a_callback)
	{
		ForEachReferenceInRange(a_ref->GetPosition(), a_radius, a_callback);
	}
}
