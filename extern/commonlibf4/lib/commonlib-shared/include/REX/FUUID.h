#pragma once

namespace REX
{
	class FUUID
	{
	public:
		inline constexpr FUUID() noexcept = default;
		inline constexpr FUUID(FUUID&&) noexcept = default;
		inline constexpr FUUID(const FUUID&) noexcept = default;

		inline constexpr FUUID(std::uint8_t a_b1, std::uint8_t a_b2, std::uint8_t a_b3, std::uint8_t a_b4, std::uint8_t a_b5, std::uint8_t a_b6, std::uint8_t a_b7, std::uint8_t a_b8, std::uint8_t a_b9, std::uint8_t a_b10, std::uint8_t a_b11, std::uint8_t a_b12, std::uint8_t a_b13, std::uint8_t a_b14, std::uint8_t a_b15, std::uint8_t a_b16) noexcept :
			m_data{ a_b1, a_b2, a_b3, a_b4, a_b5, a_b6, a_b7, a_b8, a_b9, a_b10, a_b11, a_b12, a_b13, a_b14, a_b15, a_b16 }
		{}

		template <std::size_t N>
			requires(N == 37)
		inline constexpr FUUID(const char (&a_string)[N]) noexcept
		{
			ParseString(a_string);
		}

		inline constexpr FUUID& operator=(FUUID&&) noexcept = default;
		inline constexpr FUUID& operator=(const FUUID&) noexcept = default;

		inline constexpr std::uint8_t& operator[](std::size_t a_pos) noexcept
		{
			return m_data[a_pos];
		}

		inline constexpr const std::uint8_t operator[](std::size_t a_pos) const noexcept
		{
			return m_data[a_pos];
		}

		inline constexpr bool operator==(const FUUID& a_rhs) const noexcept
		{
			for (auto i = 0u; i < sizeof(m_data); i++) {
				if (m_data[i] != a_rhs.m_data[i]) {
					return false;
				}
			}

			return true;
		}

		inline constexpr explicit operator bool() const noexcept
		{
			for (auto i = 0u; i < sizeof(m_data); i++) {
				if (m_data[i]) {
					return true;
				}
			}

			return false;
		}

	private:
		inline static constexpr std::uint8_t ParseStringHexChar(const char a_char)
		{
			const char c = a_char | 0x20;
			if (c >= '0' && c <= '9')
				return c - '0';
			else if (c >= 'a' && c <= 'f')
				return c - 'a' + 10;
			else
				throw "invalid hexadecimal character in FUUID";
		}

		inline static constexpr std::uint8_t ParseStringHex(const char* a_str, const std::size_t a_pos)
		{
			return (ParseStringHexChar(a_str[a_pos]) << 4) + ParseStringHexChar(a_str[a_pos + 1]);
		}

		inline constexpr void ParseString(const char* a_str)
		{
			//constexpr std::size_t parse_string_table_le[16]{
			//	6, 4, 2, 0, 11, 9, 16, 14, 19, 21, 24, 26, 28, 30, 32, 34
			//};

			constexpr std::size_t parse_string_table_be[16]{
				0, 2, 4, 6, 9, 11, 14, 16, 19, 21, 24, 26, 28, 30, 32, 34
			};

			for (auto i = 0u; i < sizeof(m_data); i++) {
				m_data[i] = ParseStringHex(a_str, parse_string_table_be[i]);
			}
		}

	private:
		std::uint8_t m_data[16];
	};
	static_assert(sizeof(FUUID) == 16);
	static_assert(FUUID{ "62470f2d-f92b-4189-8af6-157310dadc8b" } == FUUID{ 0x62, 0x47, 0x0f, 0x2d, 0xf9, 0x2b, 0x41, 0x89, 0x8a, 0xf6, 0x15, 0x73, 0x10, 0xda, 0xdc, 0x8b });
}
