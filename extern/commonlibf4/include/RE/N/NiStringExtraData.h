#pragma once

#include "RE/N/NiExtraData.h"

namespace RE
{
	class __declspec(novtable) NiStringExtraData :
		public NiExtraData
	{
	public:
		static constexpr auto RTTI{ RTTI::NiStringExtraData };
		static constexpr auto VTABLE{ VTABLE::NiStringExtraData };
		static constexpr auto Ni_RTTI{ Ni_RTTI::NiStringExtraData };

		NiStringExtraData() :
			NiExtraData()
		{
			REX::EMPLACE_VTABLE(this);
		}

		NiStringExtraData(const BSFixedString& a_value) :
			NiExtraData(), value(a_value)
		{
			REX::EMPLACE_VTABLE(this);
		}

		NiStringExtraData(const BSFixedString& a_name, const BSFixedString& a_value) :
			NiExtraData(a_name), value(a_value)
		{
			REX::EMPLACE_VTABLE(this);
		}

	public:
		const BSFixedString& GetValue() const
		{
			return value;
		}

		void SetValue(const BSFixedString& a_value)
		{
			value = a_value;
		}

	protected:
		// members
		BSFixedString value;  // 0x18
	};
	static_assert(sizeof(NiStringExtraData) == 0x20);
}
