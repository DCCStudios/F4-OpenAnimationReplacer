#pragma once

#include "RE/B/BSTArray.h"
#include "RE/G/GridArray.h"
#include "RE/G/GridCell.h"
#include "RE/N/NiPoint3.h"

namespace RE
{
	class __declspec(novtable) GridCellArray :
		public GridArray  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::GridCellArray };
		static constexpr auto VTABLE{ VTABLE::GridCellArray };

		class QueuedAttach
		{
		private:
			enum class ProgressiveAttachState : std::int32_t
			{
				kWorld = 0,
				kModels = 1,
				kRegisterCombinedObjectVisibility = 2,
				kDone = 3,
			};

		public:
			// members
			TESObjectCELL*         cell;         // 00
			ProgressiveAttachState attachState;  // 08
		};
		static_assert(sizeof(QueuedAttach) == 0x10);

		class QueuedDetach
		{
		private:
			enum class ProgressiveDetachState : std::int32_t
			{
				kActorsNext = 0,
				kRefsNext = 1,
				kCombinedArtNext = 2,
				kDone = 3,
			};

		public:
			// members
			TESObjectCELL*         cell;         // 00
			ProgressiveDetachState detachState;  // 08
		};
		static_assert(sizeof(QueuedDetach) == 0x10);

		class AutoDisableQueuedCellAttachDetach
		{
		public:
			// members
			GridCellArray* grid;  // 00
		};

		~GridCellArray() override;  // 00

		// override
		void KillAll() override;                                                                                         // 02
		bool SetCenter(std::int32_t a_x, std::int32_t a_y) override;                                                     // 03
		void Detach(std::uint32_t a_x, std::uint32_t a_y) override;                                                      // 05
		void ClearItem(std::uint32_t a_x, std::uint32_t a_y) override;                                                   // 06
		void MoveItem(std::uint32_t a_fromX, std::uint32_t a_fromY, std::uint32_t a_toX, std::uint32_t a_toY) override;  // 07
		void SwapItem(std::uint32_t a_fromX, std::uint32_t a_fromY, std::uint32_t a_toX, std::uint32_t a_toY) override;  // 08

		GridCell* Get(std::uint32_t a_x, std::uint32_t a_y) const
		{
			using func_t = decltype(&GridCellArray::Get);
			static REL::Relocation<func_t> func{ ID::GridCellArray::Get };
			return func(this, a_x, a_y);
		}

		TESObjectCELL* GetCell(const std::uint32_t a_x, const std::uint32_t a_y) const noexcept
		{
			return Get(a_x, a_y)->cell;
		};

		// members
		GridCell*              gridCell;                       // 0x18 - (memory allocated using 0x8 * numGrids * numGrids)
		std::uint32_t          cellAttachDetachQueueDisabled;  // 0x20
		BSTArray<QueuedAttach> queuedAttach;                   // 0x28
		BSTArray<QueuedDetach> queuedDetach;                   // 0x40
		NiPoint3               worldCenter;                    // 0x58
		bool                   landAttached;                   // 0x64
	};
	static_assert(sizeof(GridCellArray) == 0x68);
}
