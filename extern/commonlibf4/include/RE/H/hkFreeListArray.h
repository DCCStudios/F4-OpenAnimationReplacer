#pragma once

#include "RE/H/hkArray.h"

namespace RE
{
	template <class T>
	struct hkDefaultFreeListArrayOperations
	{
		inline static void SetEmpty(T& a_element, std::uint32_t a_next)
		{
			(std::uint32_t&)a_element = a_next;
		}

		inline static std::uint32_t GetNext(const T& a_element)
		{
			return (std::uint32_t&)a_element;
		}
	};

	template <class T>
	struct hkFreeListArrayElement
	{
		T m_pod;
	};

	template <class T, class I, std::int32_t G, class O = hkDefaultFreeListArrayOperations<T>>
	class hkFreeListArray
	{
	public:
	private:
		// members
		hkArray<hkFreeListArrayElement<T>> m_elements;
		std::int32_t                       m_firstFree;
	};
	static_assert(sizeof(hkFreeListArray<std::uint8_t, std::uint8_t, 0>) == 0x18);
}
