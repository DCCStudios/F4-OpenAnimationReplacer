#include "REL/IDDB.h"

#include "REL/Version.h"

#include "REX/FModule.h"
#include "REX/HASH.h"
#include "REX/LOG.h"
#include "REX/W32/KERNEL32.h"

namespace REL
{
	class IDDB::STREAM
	{
	public:
		using stream_type = std::ifstream;
		using pointer = stream_type*;
		using const_pointer = const stream_type*;
		using reference = stream_type&;
		using const_reference = const stream_type&;

		STREAM(const std::filesystem::path a_path, const std::ios_base::openmode a_mode) :
			_stream(a_path, a_mode)
		{
			if (!_stream.is_open())
				REX::FAIL(L"Failed to open Address Library file!\nPath: {}", a_path.wstring());

			_stream.exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);
		}

		void ignore(const std::streamsize a_count)
		{
			_stream.ignore(a_count);
		}

		template <class T>
		void readin(T& a_val)
		{
			_stream.read(std::bit_cast<char*>(std::addressof(a_val)), sizeof(T));
		}

		template <class T>
			requires(std::is_arithmetic_v<T>)
		T readout()
		{
			T val{};
			readin(val);
			return val;
		}

	private:
		stream_type _stream;
	};

	class IDDB::HEADER_V2
	{
	public:
		HEADER_V2(IDDB::STREAM& a_in)
		{
			a_in.readin(m_gameVersion);

			const auto nameLen = a_in.readout<std::uint32_t>();
			for (std::uint32_t i = 0; i < nameLen; i++)
				a_in.readin(m_name[i]);

			m_name[nameLen] = '\0';

			a_in.readin(m_pointerSize);
			a_in.readin(m_addressCount);
		}

		std::string_view name() const noexcept { return m_name; }
		std::size_t      address_count() const noexcept { return static_cast<std::size_t>(m_addressCount); }
		std::uint64_t    pointer_size() const noexcept { return static_cast<std::uint64_t>(m_pointerSize); }

		Version game_version() const noexcept
		{
			Version version;
			for (std::size_t i = 0; i < 4; i++)
				version[i] = static_cast<std::uint16_t>(m_gameVersion[i]);

			return version;
		}

	private:
		char          m_name[64]{};
		std::uint32_t m_gameVersion[4]{};
		std::int32_t  m_pointerSize{ 0 };
		std::int32_t  m_addressCount{ 0 };
	};

	class IDDB::HEADER_V5
	{
	public:
		HEADER_V5(IDDB::STREAM& a_in)
		{
			a_in.readin(m_gameVersion);
			a_in.readin(m_name);
			a_in.readin(m_pointerSize);
			a_in.readin(m_dataFormat);
			a_in.readin(m_offsetCount);
		}

		std::string_view name() const noexcept { return m_name; }
		std::size_t      offset_count() const noexcept { return static_cast<std::size_t>(m_offsetCount); }

		Version game_version() const noexcept
		{
			Version version;
			for (std::size_t i = 0; i < 4; i++)
				version[i] = static_cast<std::uint16_t>(m_gameVersion[i]);

			return version;
		}

	private:
		std::int32_t  m_fileVersion{ 5 };
		std::uint32_t m_gameVersion[4]{};
		char          m_name[64]{};
		std::int32_t  m_pointerSize{ 0 };
		std::int32_t  m_dataFormat{ 0 };
		std::int32_t  m_offsetCount{ 0 };
	};
}

namespace REL
{
	IDDB::IDDB()
	{
		std::unordered_map<Loader, std::vector<std::wstring>> g_rootMap{
			{ Loader::SKSE, { L"versionlib", L"version" } },
			{ Loader::F4SE, { L"version" } },
			{ Loader::SFSE, { L"versionlib" } },
			{ Loader::OBSE, { L"versionlib" } },
		};

		std::unordered_map<Loader, std::pair<std::string, std::wstring>> g_loaderMap{
			{ Loader::SKSE, { "SKSE", L"SKSE" } },
			{ Loader::F4SE, { "F4SE", L"F4SE" } },
			{ Loader::SFSE, { "SFSE", L"SFSE" } },
			{ Loader::OBSE, { "OBSE", L"OBSE" } },
		};

		wchar_t buffer[REX::W32::MAX_PATH];
		REX::W32::GetModuleFileNameW(REX::W32::GetCurrentModule(), buffer, REX::W32::MAX_PATH);
		std::filesystem::path plugin(buffer);

		auto loader = plugin.parent_path().parent_path();
		auto loaderName = loader.filename().wstring();
		std::transform(loaderName.begin(), loaderName.end(), loaderName.begin(), std::towupper);
		for (auto& entry : g_loaderMap)
			if (loaderName == entry.second.second) {
				m_loader = entry.first;
				break;
			}

		if (m_loader == Loader::None)
			REX::FAIL("Failed to determine Address Library loader!");

		const auto mod = REX::FModule::GetExecutingModule();
		const auto version = mod.GetFileVersion().wstring(L"-");
		for (const auto& root : g_rootMap[m_loader]) {
			const auto name = std::format(L"{}-{}.bin", root, version);
			const auto path = plugin.parent_path() / name;
			if (std::filesystem::exists(path)) {
				if (m_loader == Loader::F4SE && root == L"version")
					m_format = Format::V0;

				m_path = path;
				break;
			}
		}

		if (m_path.empty())
			REX::FAIL("Failed to determine Address Library path!\nLoader: {}", g_loaderMap[m_loader].first);

		if (m_format == Format::V0) {
			load_v0();
			return;
		}

		STREAM     stream(m_path, std::ios::in | std::ios::binary);
		const auto format = stream.readout<std::uint32_t>();
		if (format < 1 || (format > 2 && format < 5) || format > 5)
			REX::FAIL("Unsupported Address Library format: {}", format);

		m_format = static_cast<Format>(format);
		if (m_format == Format::V1 || m_format == Format::V2) {
			load_v2(stream);
		} else if (m_format == Format::V5) {
			load_v5(stream);
		}
	}

	void IDDB::load_v0()
	{
		const auto mod = REX::FModule::GetExecutingModule();
		const auto mapName = std::format("COMMONLIB_IDDB_OFFSETS_{}", mod.GetFileVersion().string("_"));
		if (!m_mmap.create(false, m_path, mapName))
			REX::FAIL(L"Failed to create Address Library MemoryMap!\nError: {}\nPath: {}", REX::W32::GetLastError(), m_path.wstring());

		validate_file();

		m_v0 = {
			reinterpret_cast<MAPPING*>(m_mmap.data() + sizeof(std::uint64_t)),
			*reinterpret_cast<std::uint64_t*>(m_mmap.data())
		};
	}

	void IDDB::load_v2(STREAM& a_stream)
	{
		try {
			HEADER_V2 header(a_stream);

			const auto mod = REX::FModule::GetExecutingModule();
			const auto modVersion = mod.GetFileVersion();
			if (header.game_version() != modVersion) {
				REX::FAIL(
					"Address Library version mismatch!\n"
					"Expected Version: {}\n"
					"Actual Version: {}",
					modVersion.string(), header.game_version().string());
			}

			const auto mapName = std::format("COMMONLIB_IDDB_OFFSETS_{}", modVersion.string("_"));
			const auto byteSize = static_cast<std::size_t>(header.address_count()) * sizeof(MAPPING);
			if (!m_mmap.create(true, mapName, byteSize))
				REX::FAIL("Failed to create Address Library MemoryMap!\nError: {}", REX::W32::GetLastError());

			validate_file();

			m_v0 = { reinterpret_cast<MAPPING*>(m_mmap.data()), header.address_count() };

			if (m_mmap.is_owner()) {
				unpack_file(a_stream, header);
				std::sort(
					m_v0.begin(),
					m_v0.end(),
					[](auto&& a_lhs, auto&& a_rhs) {
						return a_lhs.id < a_rhs.id;
					});
			}
		} catch (const std::system_error&) {
			REX::FAIL(L"Failed to open Address Library file!\nPath: {}", m_path.wstring());
		}
	}

	void IDDB::load_v5(STREAM& a_stream)
	{
		try {
			HEADER_V5 header(a_stream);

			const auto mod = REX::FModule::GetExecutingModule();
			const auto modVersion = mod.GetFileVersion();
			if (header.game_version() != modVersion) {
				REX::FAIL(
					"Address Library version mismatch!\n"
					"Expected Version: {}\n"
					"Actual Version: {}",
					modVersion.string(), header.game_version().string());
			}

			const auto mapName = std::format("COMMONLIB_IDDB_OFFSETS_{}", modVersion.string("_"));
			if (!m_mmap.create(false, m_path, mapName))
				REX::FAIL(L"Failed to create Address Library MemoryMap!\nError: {}\nPath: {}", REX::W32::GetLastError(), m_path.wstring());

			validate_file();

			m_v5 = { reinterpret_cast<std::uint32_t*>(m_mmap.data() + sizeof(HEADER_V5)), header.offset_count() };

		} catch (const std::system_error&) {
			REX::FAIL(L"Failed to open Address Library file!\nPath: {}", m_path.wstring());
		}
	}

	void IDDB::unpack_file(STREAM& a_stream, const HEADER_V2& a_header)
	{
		std::uint8_t  type = 0;
		std::uint64_t id = 0;
		std::uint64_t offset = 0;
		std::uint64_t prevID = 0;
		std::uint64_t prevOffset = 0;
		for (auto& mapping : m_v0) {
			a_stream.readin(type);
			const auto lo = static_cast<std::uint8_t>(type & 0xF);
			const auto hi = static_cast<std::uint8_t>(type >> 4);

			switch (lo) {
				case 0:
					a_stream.readin(id);
					break;
				case 1:
					id = prevID + 1;
					break;
				case 2:
					id = prevID + a_stream.readout<std::uint8_t>();
					break;
				case 3:
					id = prevID - a_stream.readout<std::uint8_t>();
					break;
				case 4:
					id = prevID + a_stream.readout<std::uint16_t>();
					break;
				case 5:
					id = prevID - a_stream.readout<std::uint16_t>();
					break;
				case 6:
					id = a_stream.readout<std::uint16_t>();
					break;
				case 7:
					id = a_stream.readout<std::uint32_t>();
					break;
				default:
					REX::FAIL("Unhandled type while loading Address Library!");
			}

			const std::uint64_t tmp = (hi & 8) != 0 ? (prevOffset / a_header.pointer_size()) : prevOffset;

			switch (hi & 7) {
				case 0:
					a_stream.readin(offset);
					break;
				case 1:
					offset = tmp + 1;
					break;
				case 2:
					offset = tmp + a_stream.readout<std::uint8_t>();
					break;
				case 3:
					offset = tmp - a_stream.readout<std::uint8_t>();
					break;
				case 4:
					offset = tmp + a_stream.readout<std::uint16_t>();
					break;
				case 5:
					offset = tmp - a_stream.readout<std::uint16_t>();
					break;
				case 6:
					offset = a_stream.readout<std::uint16_t>();
					break;
				case 7:
					offset = a_stream.readout<std::uint32_t>();
					break;
				default:
					REX::FAIL("Unhandled type while loading Address Library!");
			}

			if ((hi & 8) != 0) {
				offset *= a_header.pointer_size();
			}

			mapping = { id, offset };

			prevOffset = offset;
			prevID = id;
		}
	}

	void IDDB::validate_file()
	{
		// clang-format off
		std::unordered_map<Loader, std::vector<std::pair<REL::Version, std::string_view>>> g_blacklistMap{
			{ Loader::SKSE, {} },
			{ Loader::F4SE, { 
				{ REL::Version{ 1, 10, 980 }, "2AD60B95388F1B6E77A6F86F17BEB51D043CF95A341E91ECB2E911A393E45FE8156D585D2562F7B14434483D6E6652E2373B91589013507CABAE596C26A343F1"sv },
				{ REL::Version{ 1, 11, 159 }, "686D40387F638ED75AD43BB76CA14170576F1A30E91144F280987D13A3012B1CA6A4E04E6BE7A5B99E46C50332C49BE40C3D9448038E17D3D31C40E72A90AE26"sv }
			} },
			{ Loader::SFSE, {} },
			{ Loader::OBSE, {} },
		};
		// clang-format on

		const auto mod = REX::FModule::GetExecutingModule();
		const auto modVersion = mod.GetFileVersion();
		for (auto& check : g_blacklistMap[m_loader]) {
			if (modVersion == check.first) {
				auto sha = REX::SHA512({ m_mmap.data(), m_mmap.size() });
				if (!sha)
					REX::FAIL("Failed to hash Address Library file!\nPath: {}", m_path.string());
				if (*sha == check.second)
					REX::FAIL("Invalid Address Library loaded!\n\nRedownload Address Library for your game version.\nGame Version: {}", modVersion.string());
			}
		}
	}

	std::uint64_t IDDB::offset(std::uint64_t a_id) const
	{
		const auto mod = REX::FModule::GetExecutingModule();
		if (std::to_underlying(m_format) < 5) {
			if (m_v0.empty())
				REX::FAIL("No Address Library has been loaded!");

			const MAPPING elem{ a_id, 0 };
			const auto    it = std::lower_bound(
                m_v0.begin(),
                m_v0.end(),
                elem,
                [](auto&& a_lhs, auto&& a_rhs) {
                    return a_lhs.id < a_rhs.id;
                });

			if (it == m_v0.end()) {
				REX::FAIL(
					"Failed to find offset for Address Library ID!\n"
					"Invalid ID: {}\n"
					"Game Version: {}",
					a_id, mod.GetFileVersion().string());
			}

			return static_cast<std::size_t>(it->offset);
		}

		if (m_v5.empty())
			REX::FAIL("No Address Library has been loaded!");

		const auto offset = static_cast<std::uint64_t>(m_v5[a_id]);
		if (!offset) {
			REX::FAIL(
				"Failed to find offset for Address Library ID!\n"
				"Invalid ID: {}\n"
				"Game Version: {}",
				a_id, mod.GetFileVersion().string());
		}

		return offset;
	}
}
