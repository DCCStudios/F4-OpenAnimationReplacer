#include "BA2Archive.h"

#include <cctype>
#include <cstring>

namespace
{
#pragma pack(push, 1)
	struct GeneralFileRecord
	{
		std::uint32_t nameHash;
		char extension[4];
		std::uint32_t directoryHash;
		std::uint32_t flags;
		std::uint64_t offset;
		std::uint32_t compressedSize;
		std::uint32_t uncompressedSize;
		std::uint32_t alignment;
	};
#pragma pack(pop)
	static_assert(sizeof(GeneralFileRecord) == 36);

	std::string NormalizeArchivePath(std::string a_path)
	{
		std::ranges::replace(a_path, '/', '\\');
		while (!a_path.empty() && a_path.front() == '\\') {
			a_path.erase(a_path.begin());
		}
		std::ranges::transform(a_path, a_path.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		return a_path;
	}

	bool IsGeneralArchive(std::ifstream& a_file, std::uint32_t& a_version,
		std::uint32_t& a_fileCount, std::uint64_t& a_nameTableOffset)
	{
		char magic[4]{};
		char type[4]{};
		if (!a_file.read(magic, sizeof(magic))) return false;
		if (!a_file.read(reinterpret_cast<char*>(&a_version), sizeof(a_version))) return false;
		if (!a_file.read(type, sizeof(type))) return false;
		if (!a_file.read(reinterpret_cast<char*>(&a_fileCount), sizeof(a_fileCount))) return false;
		if (!a_file.read(reinterpret_cast<char*>(&a_nameTableOffset), sizeof(a_nameTableOffset))) return false;

		if (std::memcmp(magic, "BTDX", 4) != 0 || std::memcmp(type, "GNRL", 4) != 0) return false;
		return a_version == 1 || a_version == 7 || a_version == 8;
	}
}

namespace OAR::BA2
{
	Index Index::Build(const std::filesystem::path& a_dataRoot)
	{
		Index result;
		if (!std::filesystem::exists(a_dataRoot)) return result;

		try {
			for (const auto& entry : std::filesystem::directory_iterator(
				a_dataRoot, std::filesystem::directory_options::skip_permission_denied)) {
				if (!entry.is_regular_file()) continue;

				auto extension = entry.path().extension().string();
				std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
					return static_cast<char>(std::tolower(c));
				});
				if (extension != ".ba2") continue;

				result.ScanArchive(entry.path());
			}
		} catch (const std::filesystem::filesystem_error& e) {
			logger::warn("[OAR-BA2] Failed to enumerate Data archives: {}", e.what());
		}

		logger::info("[OAR-BA2] Indexed {} General BA2 resources from Data", result.entries.size());
		return result;
	}

	void Index::ScanArchive(const std::filesystem::path& a_archivePath)
	{
		std::ifstream file(a_archivePath, std::ios::binary);
		if (!file.is_open()) {
			logger::warn("[OAR-BA2] Cannot open archive '{}'", a_archivePath.string());
			return;
		}

		std::uint32_t version = 0;
		std::uint32_t fileCount = 0;
		std::uint64_t nameTableOffset = 0;
		if (!IsGeneralArchive(file, version, fileCount, nameTableOffset)) {
			return;
		}

		if (fileCount > 1'000'000 || nameTableOffset == 0) {
			logger::warn("[OAR-BA2] Invalid GNRL header in '{}'", a_archivePath.string());
			return;
		}

		std::vector<std::uint32_t> uncompressedSizes;
		uncompressedSizes.reserve(fileCount);
		for (std::uint32_t i = 0; i < fileCount; ++i) {
			GeneralFileRecord record{};
			if (!file.read(reinterpret_cast<char*>(&record), sizeof(record))) {
				logger::warn("[OAR-BA2] Truncated file table in '{}'", a_archivePath.string());
				return;
			}
			uncompressedSizes.push_back(record.uncompressedSize);
		}

		file.seekg(static_cast<std::streamoff>(nameTableOffset), std::ios::beg);
		if (!file) {
			logger::warn("[OAR-BA2] Invalid name table offset in '{}'", a_archivePath.string());
			return;
		}

		std::size_t added = 0;
		for (std::uint32_t i = 0; i < fileCount; ++i) {
			std::uint16_t nameLength = 0;
			if (!file.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength)) || nameLength == 0 || nameLength > 4096) {
				logger::warn("[OAR-BA2] Invalid name table entry {} in '{}'", i, a_archivePath.string());
				return;
			}

			std::string path(nameLength, '\0');
			if (!file.read(path.data(), static_cast<std::streamsize>(nameLength))) {
				logger::warn("[OAR-BA2] Truncated name table in '{}'", a_archivePath.string());
				return;
			}

			path = NormalizeArchivePath(std::move(path));
			if (!path.starts_with("meshes\\") || !path.ends_with(".hkx")) continue;
			if (!seenPaths.insert(path).second) continue;

			entries.push_back({ std::move(path), uncompressedSizes[i] });
			++added;
		}

		logger::info("[OAR-BA2] Archive '{}' version {} contributed {} HKX entries",
			a_archivePath.filename().string(), version, added);
	}
}
