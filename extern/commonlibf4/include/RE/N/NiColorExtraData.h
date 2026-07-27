#pragma once

#include "RE/N/NiColor.h"
#include "RE/N/NiExtraData.h"

namespace RE
{
	class __declspec(novtable) NiColorExtraData :
		public NiExtraData
	{
	public:
		static constexpr auto RTTI{ RTTI::NiColorExtraData };
		static constexpr auto VTABLE{ VTABLE::NiColorExtraData };
		static constexpr auto Ni_RTTI{ Ni_RTTI::NiColorExtraData };

		NiColorExtraData() :
			NiExtraData()
		{
			REX::EMPLACE_VTABLE(this);
		}

		NiColorExtraData(const NiColorA& a_value) :
			NiExtraData(), value(a_value)
		{
			REX::EMPLACE_VTABLE(this);
		}

		NiColorExtraData(const BSFixedString& a_name, const NiColorA& a_value) :
			NiExtraData(a_name), value(a_value)
		{
			REX::EMPLACE_VTABLE(this);
		}

	public:
		const NiColorA& GetValue() const
		{
			return value;
		}

		void SetValue(const NiColor& a_value)
		{
			value = a_value;
		}

		void SetValue(const NiColorA& a_value)
		{
			value = a_value;
		}

	protected:
		// members
		NiColorA value;  // 0x18
	};
	static_assert(sizeof(NiColorExtraData) == 0x28);
}
