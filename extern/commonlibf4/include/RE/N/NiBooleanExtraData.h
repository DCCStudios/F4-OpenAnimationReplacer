#pragma once

#include "RE/N/NiExtraData.h"

namespace RE
{
	class __declspec(novtable) NiBooleanExtraData :
		public NiExtraData
	{
	public:
		static constexpr auto RTTI{ RTTI::NiBooleanExtraData };
		static constexpr auto VTABLE{ VTABLE::NiBooleanExtraData };
		static constexpr auto Ni_RTTI{ Ni_RTTI::NiBooleanExtraData };

		NiBooleanExtraData() :
			NiExtraData()
		{
			REX::EMPLACE_VTABLE(this);
		}

		NiBooleanExtraData(const bool a_value) :
			NiExtraData(), value(a_value)
		{
			REX::EMPLACE_VTABLE(this);
		}

		NiBooleanExtraData(const BSFixedString& a_name, const bool a_value) :
			NiExtraData(a_name), value(a_value)
		{
			REX::EMPLACE_VTABLE(this);
		}

	public:
		bool GetValue() const
		{
			return value;
		}

		void SetValue(const bool a_value)
		{
			value = a_value;
		}

	protected:
		// members
		bool value{ 0 };  // 0x18
	};
	static_assert(sizeof(NiBooleanExtraData) == 0x20);
}
