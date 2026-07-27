#pragma once

namespace RE
{
	enum class hkResultEnum : std::int32_t
	{
		kSuccess,
		kFailure
	};

	class hkResult
	{
	public:
		using enum hkResultEnum;

		hkResult() = default;

		hkResult(hkResultEnum a_enum) :
			m_enum(a_enum)
		{}

		hkResult& operator=(hkResultEnum a_enum)
		{
			m_enum = a_enum;
			return *this;
		}

		bool operator==(const hkResult&) const = default;
		bool operator!=(const hkResult&) const = default;

		// members
		hkResultEnum m_enum;  // 0x00
	};
	static_assert(sizeof(hkResult) == 0x4);
}
