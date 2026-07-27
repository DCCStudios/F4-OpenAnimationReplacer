#pragma once

#include "RE/H/hkTransform.h"
#include "RE/N/NiTransform.h"

namespace RE
{
	constexpr auto HK_TO_BS_SCALE{ 69.99125F };
	constexpr auto BS_TO_HK_SCALE{ 0.0142875F };

	template <class T1, class T2>
	constexpr T1 HK_TO_NI(const T2& a_hk) noexcept
	{
		if constexpr (std::is_same_v<T1, NiPoint3> && std::is_same_v<T2, hkVector4f>) {
			return NiPoint3(a_hk.x, a_hk.y, a_hk.z) * HK_TO_BS_SCALE;
		} else if constexpr (std::is_same_v<T1, NiPoint3A> && std::is_same_v<T2, hkVector4f>) {
			return NiPoint3A(a_hk.x, a_hk.y, a_hk.z) * HK_TO_BS_SCALE;
		} else if constexpr (std::is_same_v<T1, NiPoint4> && std::is_same_v<T2, hkVector4f>) {
			return NiPoint4(a_hk.x, a_hk.y, a_hk.z, a_hk.w) * HK_TO_BS_SCALE;
		} else if constexpr (std::is_same_v<T1, NiMatrix3> && (std::is_same_v<T2, hkMatrix3f> || std::is_same_v<T2, hkRotationf>)) {
			NiMatrix3 ni;
			ni[0] = { a_hk[0][0], a_hk[1][0], a_hk[2][0], 0.0F };
			ni[1] = { a_hk[0][1], a_hk[1][1], a_hk[2][1], 0.0F };
			ni[2] = { a_hk[0][2], a_hk[1][2], a_hk[2][2], 0.0F };
			return ni;
		} else if constexpr (std::is_same_v<T1, NiTransform> && std::is_same_v<T2, hkTransform>) {
			return NiTransform(HK_TO_NI<NiMatrix3>(a_hk.m_rotation), HK_TO_NI<NiPoint3>(a_hk.m_translation));
		}
	}

	template <class T1, class T2>
	constexpr T1 NI_TO_HK(const T2& a_ni) noexcept
	{
		if constexpr (std::is_same_v<T1, hkVector4f> && std::is_same_v<T2, NiPoint3>) {
			return hkVector4f(a_ni.x, a_ni.y, a_ni.z, 0.0F) * BS_TO_HK_SCALE;
		} else if constexpr (std::is_same_v<T1, hkVector4f> && std::is_same_v<T2, NiPoint3A>) {
			return hkVector4f(a_ni.x, a_ni.y, a_ni.z, 0.0F) * BS_TO_HK_SCALE;
		} else if constexpr (std::is_same_v<T1, hkVector4f> && std::is_same_v<T2, NiPoint4>) {
			return hkVector4f(a_ni.x, a_ni.y, a_ni.z, a_ni.w) * BS_TO_HK_SCALE;
		} else if constexpr ((std::is_same_v<T1, hkMatrix3f> || std::is_same_v<T1, hkRotationf>) && std::is_same_v<T2, NiMatrix3>) {
			hkMatrix3f hk;
			hk[0] = { a_ni[0][0], a_ni[1][0], a_ni[2][0], 0.0F };
			hk[1] = { a_ni[0][1], a_ni[1][1], a_ni[2][1], 0.0F };
			hk[2] = { a_ni[0][2], a_ni[1][2], a_ni[2][2], 0.0F };
			return hk;
		} else if constexpr (std::is_same_v<T1, hkTransform> && std::is_same_v<T2, NiTransform>) {
			return hkTransform(NI_TO_HK<hkMatrix3f>(a_ni.rotate), NI_TO_HK<hkVector4f>(a_ni.translate));
		}
	}
}
