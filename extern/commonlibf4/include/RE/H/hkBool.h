#pragma once

namespace RE
{
	class hkBool
	{
	public:
		hkBool() = default;

		hkBool(bool a_bool) :
			m_bool(a_bool)
		{}

		hkBool& operator=(bool a_bool)
		{
			m_bool = a_bool;
			return *this;
		}

		bool operator==(const hkBool&) const = default;
		bool operator!=(const hkBool&) const = default;

		operator bool() { return m_bool; }

		// members
		bool m_bool{ false };
	};

	using hkBool32 = std::uint32_t;
	using hkBoolLL = std::uint64_t;
}
