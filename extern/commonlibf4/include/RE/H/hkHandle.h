#pragma once

namespace RE
{
	template <class T, T N, class D>
	class hkHandle
	{
	public:
		~hkHandle() noexcept {}  // intentional

		// members
		T m_value;  // 0x00
	};
	static_assert(sizeof(hkHandle<std::uint8_t, 255, void>) == 0x01);
}
