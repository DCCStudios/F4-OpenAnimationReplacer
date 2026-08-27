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
		static Index Build(const std::filesystem::path& a_dataRoot);

		const std::vector<Entry>& GetEntries() const { return entries; }
		std::size_t GetEntryCount() const { return entries.size(); }
		std::span<const Entry> GetEntriesUnderPrefix(std::string_view a_prefix) const;

	private:
		void ScanArchive(const std::filesystem::path& a_archivePath);

		std::vector<Entry> entries;
		std::unordered_set<std::string> seenPaths;
	};
}

