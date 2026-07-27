#pragma once

#include "RE/N/NiExtraData.h"

namespace RE
{
	class __declspec(novtable) NiBinaryExtraData :
		public NiExtraData
	{
	public:
		static constexpr auto RTTI{ RTTI::NiBinaryExtraData };
		static constexpr auto VTABLE{ VTABLE::NiBinaryExtraData };
		static constexpr auto Ni_RTTI{ Ni_RTTI::NiBinaryExtraData };

		NiBinaryExtraData() :
			NiExtraData()
		{
			REX::EMPLACE_VTABLE(this);
		}

		NiBinaryExtraData(void* a_value, const std::uint32_t a_size) :
			NiExtraData(), value(a_value), size(a_size)
		{
			REX::EMPLACE_VTABLE(this);
		}

		NiBinaryExtraData(const BSFixedString& a_name, void* a_value, const std::uint32_t a_size) :
			NiExtraData(a_name), value(a_value), size(a_size)
		{
			REX::EMPLACE_VTABLE(this);
		}

	public:
		void GetValue(void** a_value, std::uint32_t* a_size) const
		{
			*a_value = value;
			*a_size = size;
		}

		void SetValue(void* a_value, const std::uint32_t a_size)
		{
			value = a_value;
			size = a_size;
		}

	protected:
		// members
		void*         value{ nullptr };  // 0x18
		std::uint32_t size{ 0 };         // 0x20
	};
	static_assert(sizeof(NiBinaryExtraData) == 0x28);
}
