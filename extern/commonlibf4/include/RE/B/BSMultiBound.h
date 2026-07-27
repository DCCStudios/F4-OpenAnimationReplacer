#pragma once

namespace RE
{
	class __declspec(novtable) BSMultiBound :
		public NiObject  // 00
	{
	public:
		inline static constexpr auto RTTI{ RTTI::BSMultiBound };
		inline static constexpr auto VTABLE{ VTABLE::BSMultiBound };
		inline static constexpr auto Ni_RTTI{ Ni_RTTI::BSMultiBound };

		// members
		std::uint32_t                boundFrameCount;  // 10
		NiPointer<BSMultiBoundShape> shape;            // 18
	};
	static_assert(sizeof(BSMultiBound) == 0x20);
}
