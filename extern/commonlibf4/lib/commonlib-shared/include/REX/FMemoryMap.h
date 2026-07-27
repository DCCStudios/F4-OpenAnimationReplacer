#pragma once

#include "REX/W32/KERNEL32.h"

namespace REX
{
	class FMemoryMap
	{
		static constexpr auto DYNAMIC_SIZE{ static_cast<std::size_t>(-1) };

	public:
		FMemoryMap() = default;
		~FMemoryMap() { close(); }

	public:
		void close()
		{
			if (m_mapView) {
				REX::W32::UnmapViewOfFile(m_mapView);
				m_mapView = nullptr;
			}

			if (m_map) {
				REX::W32::CloseHandle(m_map);
				m_map = nullptr;
			}

			if (m_file != REX::W32::INVALID_HANDLE_VALUE) {
				REX::W32::CloseHandle(m_file);
				m_file = nullptr;
			}

			m_size = 0;
			m_owner = false;
		}

		std::byte* data() const noexcept
		{
			return static_cast<std::byte*>(m_mapView);
		}

		std::size_t size() const noexcept
		{
			return m_size;
		}

		bool is_open() const
		{
			return m_mapView;
		}

		bool is_owner() const
		{
			return m_owner;
		}

		bool is_file() const
		{
			return m_file != REX::W32::INVALID_HANDLE_VALUE;
		}

		bool create(bool a_write, const std::string_view a_name, std::size_t a_size)
		{
			close();

			if (create_impl(a_write, a_name, a_size))
				return true;

			close();
			return false;
		}

		bool create(bool a_write, std::filesystem::path a_path, const std::string_view a_name, std::size_t a_size = DYNAMIC_SIZE)
		{
			close();

			const auto access = a_write ? REX::W32::GENERIC_READ | REX::W32::GENERIC_WRITE : REX::W32::GENERIC_READ;
			const auto share = a_write ? REX::W32::FILE_SHARE_READ | REX::W32::FILE_SHARE_WRITE : REX::W32::FILE_SHARE_READ;
			m_file = REX::W32::CreateFileW(a_path.c_str(), access, share, nullptr, REX::W32::OPEN_EXISTING, REX::W32::FILE_ATTRIBUTE_READONLY, nullptr);
			if (m_file == REX::W32::INVALID_HANDLE_VALUE)
				return false;

			if (create_impl(a_write, a_name, a_size))
				return true;

			close();
			return false;
		}

	private:
		bool create_impl(bool a_write, const std::string_view a_name, std::size_t a_size)
		{
			REX::W32::LARGE_INTEGER size;
			if (a_size == DYNAMIC_SIZE) {
				if (m_file == REX::W32::INVALID_HANDLE_VALUE)
					return false;

				if (REX::W32::GetFileSizeEx(m_file, &size) == 0)
					return false;

			} else {
				size.value = a_size;
			}

			const auto access = a_write ? REX::W32::FILE_MAP_READ | REX::W32::FILE_MAP_WRITE : REX::W32::FILE_MAP_READ;
			m_map = REX::W32::OpenFileMappingA(access, false, a_name.data());
			if (!m_map) {
				const auto protect = a_write ? REX::W32::PAGE_READWRITE : REX::W32::PAGE_READONLY;
				m_map = REX::W32::CreateFileMappingA(m_file, nullptr, protect, size.hi, size.lo, a_name.data());
				if (!m_map)
					return false;

				m_owner = true;
			}

			m_mapView = REX::W32::MapViewOfFile(m_map, access, 0, 0, size.value);
			if (!m_mapView)
				return false;

			m_size = static_cast<std::size_t>(size.value);

			return true;
		}

	private:
		void*       m_file{ REX::W32::INVALID_HANDLE_VALUE };
		void*       m_map{ nullptr };
		void*       m_mapView{ nullptr };
		std::size_t m_size{ 0 };
		bool        m_owner{ false };
	};

	using MemoryMap [[deprecated("Renamed to 'REX::FMemoryMap'")]] = FMemoryMap;
}
