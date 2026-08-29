#pragma once

#include <span>
#include <string_view>

namespace OAR::BA2
{
	struct Entry
	{
		std::string path;
	};

	class Index
	{
	public:
		// a_registeredOwners: lowercased plugin stems (filename without extension) for
		// every plugin active in the current load order. Only archives whose owning
		// plugin appears in this set are indexed, so archives that are present on disk
		// but belong to a disabled plugin - which the game's resource layer never
		// registers - are skipped, avoiding dead index entries and silent wrong-file
		// fallbacks. Pass an empty set to disable the gate and index every General BA2
		// (legacy behavior, used when the load order is not yet available).
		static Index Build(const std::filesystem::path& a_dataRoot,
			const std::unordered_set<std::string>& a_registeredOwners);

		const std::vector<Entry>& GetEntries() const { return entries; }
		std::size_t GetEntryCount() const { return entries.size(); }
		std::span<const Entry> GetEntriesUnderPrefix(std::string_view a_prefix) const;

	private:
		void ScanArchive(const std::filesystem::path& a_archivePath);

		std::vector<Entry> entries;
		std::unordered_set<std::string> seenPaths;
	};
}
