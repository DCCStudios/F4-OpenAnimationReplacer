#pragma once

#include "REX/BASE.h"

#include "REX/FMemoryMap.h"
#include "REX/TSingleton.h"

namespace REL
{
	class IDDB :
		public REX::TSingleton<IDDB>
	{
	public:
		enum class Loader : std::uint32_t
		{
			None,
			SKSE,
			F4SE,
			SFSE,
			OBSE,
		};

		enum class Format : std::int32_t
		{
			None = std::numeric_limits<std::int32_t>::max(),
			V0 = 0,
			V1 = 1,
			V2 = 2,
			V5 = 5
		};

		struct MAPPING
		{
			std::uint64_t id;
			std::uint64_t offset;
		};

		IDDB();

		std::uint64_t offset(std::uint64_t a_id) const;

	private:
		class STREAM;
		class HEADER_V2;
		class HEADER_V5;

		void load_v0();
		void load_v2(STREAM& a_stream);
		void load_v5(STREAM& a_stream);
		void unpack_file(STREAM& a_stream, const HEADER_V2& a_header);
		void validate_file();

	protected:
		friend class Offset2ID;

		// clang-format off
		template <class T> std::span<T>      get_id2offset() const noexcept;
		template <> std::span<MAPPING>       get_id2offset() const noexcept { return m_v0; }
		template <> std::span<std::uint32_t> get_id2offset() const noexcept { return m_v5; }
		// clang-format on

	private:
		std::filesystem::path    m_path;
		Loader                   m_loader{ Loader::None };
		Format                   m_format{ Format::None };
		REX::FMemoryMap          m_mmap;
		std::span<MAPPING>       m_v0;
		std::span<std::uint32_t> m_v5;
	};
}
