#pragma once

#include "RE/N/NiExtraData.h"

namespace RE
{
	class __declspec(novtable) NiFloatExtraData :
		public NiExtraData
	{
	public:
		static constexpr auto RTTI{ RTTI::NiFloatExtraData };
		static constexpr auto VTABLE{ VTABLE::NiFloatExtraData };
		static constexpr auto Ni_RTTI{ Ni_RTTI::NiFloatExtraData };

		NiFloatExtraData() :
			NiExtraData()
		{
			REX::EMPLACE_VTABLE(this);
		}

		NiFloatExtraData(const float a_value) :
			NiExtraData(), value(a_value)
		{
			REX::EMPLACE_VTABLE(this);
		}

		NiFloatExtraData(const BSFixedString& a_name, const float a_value) :
			NiExtraData(a_name), value(a_value)
		{
			REX::EMPLACE_VTABLE(this);
		}

	public:
		float GetValue() const
		{
			return value;
		}

		void SetValue(const float a_value)
		{
			value = a_value;
		}

	protected:
		// members
		float value{ 0 };  // 0x18
	};
	static_assert(sizeof(NiIntegerExtraData) == 0x20);
}
