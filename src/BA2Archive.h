#pragma once

namespace OAR::BA2
{
	struct Entry
	{
		std::string path;
		std::uint32_t uncompressedSize{ 0 };
	};

	class Index
	{
	public:
		static Index Build(const std::filesystem::path& a_dataRoot);

		const std::vector<Entry>& GetEntries() const { return entries; }
		std::size_t GetEntryCount() const { return entries.size(); }

	private:
		void ScanArchive(const std::filesystem::path& a_archivePath);

		std::vector<Entry> entries;
		std::unordered_set<std::string> seenPaths;
	};
}

