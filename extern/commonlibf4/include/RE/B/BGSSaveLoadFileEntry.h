#pragma once

namespace RE
{
	class __declspec(novtable) BGSSaveLoadFileEntry
	{
	public:
		void DeleteGame()
		{
			using func_t = decltype(&BGSSaveLoadFileEntry::DeleteGame);
			REL::Relocation<func_t> func{ ID::BGSSaveLoadFileEntry::DeleteGame };
			return func(this);
		}

		void LoadData()
		{
			using func_t = decltype(&BGSSaveLoadFileEntry::LoadData);
			REL::Relocation<func_t> func{ ID::BGSSaveLoadFileEntry::LoadData };
			return func(this);
		}

		// members
		char*              fileName;          // 00
		char*              playerName;        // 08
		char*              playerTitle;       // 10
		char*              location;          // 18
		char*              playTime;          // 20;
		char*              raceName;          // 28
		int                version;           // 30
		std::uint32_t      saveGameNumber;    // 34
		std::uint32_t      playerLevel;       // 38
		float              levelProgress;     // 3C
		float              levelThreshold;    // 40;
		std::uint32_t      screenshotWidth;   // 44;
		std::uint32_t      screenshotHeight;  // 48;
		std::uint32_t      screenshotOffset;  // 4C;
		REX::W32::FILETIME fileTime;          // 50
		REX::W32::FILETIME saveTime;          // 58
		int                deviceID;          // 60
		bool               loaded;            // 64;
		bool               corrupt;           // 65;
		bool               needsSync;         // 66
	};
	static_assert(sizeof(BGSSaveLoadFileEntry) == 0x68);
}
