#pragma once

namespace RE
{
	class hkQuaternionf
	{
	public:
		// members
		hkVector4f m_vec;  // 0x00
	};
	static_assert(sizeof(hkQuaternionf) == 0x10);

	using hkQuaternion = hkQuaternionf;
}
