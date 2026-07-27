#pragma once

#include "RE/N/NiExtraData.h"

namespace RE
{
	class __declspec(novtable) NiIntegerExtraData :
		public NiExtraData
	{
	public:
		static constexpr auto RTTI{ RTTI::NiIntegerExtraData };
		static constexpr auto VTABLE{ VTABLE::NiIntegerExtraData };
		static constexpr auto Ni_RTTI{ Ni_RTTI::NiIntegerExtraData };

		NiIntegerExtraData() :
			NiExtraData()
		{
			REX::EMPLACE_VTABLE(this);
		}

		NiIntegerExtraData(const std::int32_t a_value) :
			NiExtraData(), value(a_value)
		{
			REX::EMPLACE_VTABLE(this);
		}

		NiIntegerExtraData(const BSFixedString& a_name, const std::int32_t a_value) :
			NiExtraData(a_name), value(a_value)
		{
			REX::EMPLACE_VTABLE(this);
		}

	public:
		std::int32_t GetValue() const
		{
			return value;
		}

		void SetValue(const std::int32_t a_value)
		{
			value = a_value;
		}

	protected:
		// members
		std::int32_t value{ 0 };  // 0x18
	};
	static_assert(sizeof(NiIntegerExtraData) == 0x20);
}
