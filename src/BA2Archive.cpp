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

	// Derives the owning plugin stem from an archive filename. The game auto-loads
	// archives named "<PluginName> - <Component>.ba2" (e.g. "MyMod - Main.ba2"), so the
	// owner is the substring before the last " - " separator, lowercased. Archives with
	// no separator (rare, non-plugin-named) fall back to the whole stem. Splitting on the
	// LAST separator tolerates plugin names that themselves contain " - ".
	std::string ArchiveOwner(const std::filesystem::path& a_archivePath)
	{
		auto stem = a_archivePath.stem().string();
		std::ranges::transform(stem, stem.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		if (const auto sep = stem.rfind(" - "); sep != std::string::npos) {
			stem.erase(sep);
		}
		return stem;
	}
}

namespace OAR::BA2
{
	Index Index::Build(const std::filesystem::path& a_dataRoot,
		const std::unordered_set<std::string>& a_registeredOwners)
	{
		Index result;
		if (!std::filesystem::exists(a_dataRoot)) return result;

		// When the caller could not resolve the load order, index every archive so we
		// never regress into finding nothing. Otherwise gate by registered ownership.
		const bool gateByOwner = !a_registeredOwners.empty();
		std::size_t skippedUnregistered = 0;

		try {
			for (const auto& entry : std::filesystem::directory_iterator(
				a_dataRoot, std::filesystem::directory_options::skip_permission_denied)) {
				if (!entry.is_regular_file()) continue;

				auto extension = entry.path().extension().string();
				std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
					return static_cast<char>(std::tolower(c));
				});
				if (extension != ".ba2") continue;

				if (gateByOwner && !a_registeredOwners.contains(ArchiveOwner(entry.path()))) {
					// Present on disk but owned by a plugin that is not in the load order;
					// the game will not register it, so indexing it would only produce
					// dead entries or a silent wrong-file fallback.
					++skippedUnregistered;
					continue;
				}

				result.ScanArchive(entry.path());
			}
		} catch (const std::filesystem::filesystem_error& e) {
			logger::warn("[OAR-BA2] Failed to enumerate Data archives: {}", e.what());
		}

		std::ranges::sort(result.entries, {}, &Entry::path);

		logger::info("[OAR-BA2] Indexed {} General BA2 resources from Data ({} archive(s) skipped as unregistered)",
			result.entries.size(), skippedUnregistered);
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

		for (std::uint32_t i = 0; i < fileCount; ++i) {
			GeneralFileRecord record{};
			if (!file.read(reinterpret_cast<char*>(&record), sizeof(record))) {
				logger::warn("[OAR-BA2] Truncated file table in '{}'", a_archivePath.string());
				return;
			}
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
			// Index replacement HKX (as before) plus OAR config.json / user.json
			// files that live under an "openanimationreplacer" directory, so a
			// fully-packaged BA2 mod (config inside the archive) can be discovered.
			// Paths are already lowercased by NormalizeArchivePath.
			if (!path.starts_with("meshes\\")) continue;
			const bool isHkx = path.ends_with(".hkx");
			const bool isOarConfig =
				(path.ends_with("\\config.json") || path.ends_with("\\user.json")) &&
				path.find("\\openanimationreplacer\\") != std::string::npos;
			if (!isHkx && !isOarConfig) continue;
			if (!seenPaths.insert(path).second) continue;

			// ParseAllMods runs on OAR's background loader. Do not call the game's
			// resource manager here: during startup it is still registering BA2
			// locations, and probing every indexed HKX from this worker can race
			// that process and corrupt the game's heap. Resource resolution is
			// deferred to AnimationCache::LoadAnimationResource, which only opens
			// paths referenced by an actual OAR replacement.
			entries.push_back({ std::move(path) });
			++added;
		}

		logger::info("[OAR-BA2] Archive '{}' version {} contributed {} resource entries",
			a_archivePath.filename().string(), version, added);
	}

	std::span<const Entry> Index::GetEntriesUnderPrefix(std::string_view a_prefix) const
	{
		if (entries.empty() || a_prefix.empty()) return {};

		std::string prefix(a_prefix);
		prefix = NormalizeArchivePath(std::move(prefix));
		if (!prefix.ends_with('\\')) prefix.push_back('\\');

		const auto first = std::ranges::lower_bound(entries, prefix, {}, &Entry::path);
		if (first == entries.end() || !first->path.starts_with(prefix)) return {};

		const auto last = std::find_if(first, entries.end(), [&](const Entry& a_entry) {
			return !a_entry.path.starts_with(prefix);
		});
		return std::span<const Entry>{
			std::to_address(first),
			static_cast<std::size_t>(std::distance(first, last))
		};
	}

	bool Index::Contains(std::string_view a_path) const
	{
		if (entries.empty() || a_path.empty()) return false;
		const std::string needle = NormalizeArchivePath(std::string(a_path));
		return std::ranges::binary_search(entries, needle, {}, &Entry::path);
	}

	std::vector<std::string> Index::GetOARSubModConfigPaths() const
	{
		static constexpr std::string_view kMarker = "\\openanimationreplacer\\";
		std::vector<std::string> result;
		for (const auto& entry : entries) {
			if (!entry.path.ends_with("\\config.json")) continue;
			const auto oarPos = entry.path.find(kMarker);
			if (oarPos == std::string::npos) continue;

			// Segments after the marker must be "<mod>\<submod>\config.json":
			// at least two directory segments before the trailing filename.
			const auto after = entry.path.substr(oarPos + kMarker.size());
			const auto firstSlash = after.find('\\');
			if (firstSlash == std::string::npos) continue;
			const auto secondSlash = after.find('\\', firstSlash + 1);
			if (secondSlash == std::string::npos) continue;  // "<mod>\config.json" = mod-level

			result.push_back(entry.path);
		}
		return result;
	}
}
