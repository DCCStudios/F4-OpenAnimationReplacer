#pragma once

#include "RE/B/BSScaleformExternalTexture.h"
#include "RE/B/BSTHashMap.h"
#include "RE/S/StartMenuBase.h"

namespace RE
{
	class MainMenu :
		public StartMenuBase,
		public BSTEventSink<MenuOpenCloseEvent>
	{
	public:
		static constexpr auto RTTI{ RTTI::MainMenu };
		static constexpr auto VTABLE{ VTABLE::MainMenu };
		static constexpr auto MENU_NAME{ "MainMenu"sv };

		enum class MAIN_MENU_EXIT_CONDITION : std::int32_t
		{
			kNoExit = 0x00,
			kNewGame = 0x01,
			kContinue = 0x02,
			kLoadGame = 0x03,
		};

		virtual ~MainMenu();  // 00

		// members
		MAIN_MENU_EXIT_CONDITION   mainMenuExitCondition;        // 230
		BSTSet<std::uint32_t>      ownedDLC;                     // 238
		std::uint64_t              creationTime;                 // 268
		std::int32_t               queuedLoadIndex;              // 270
		bool                       choseContinue;                // 274
		bool                       queueStartNewGame;            // 275
		bool                       queueContinueGame;            // 276
		bool                       creditScreen;                 // 277
		bool                       userEngaged;                  // 278
		bool                       mainBinkShown;                // 279
		bool                       allowSkip;                    // 27A
		bool                       debounceMainListPress;        // 27B
		bool                       nativeConsolePanelIsOpen;     // 27C
		bool                       shouldDebounceMainListPress;  // 27D
		BSScaleformExternalTexture gamerIconTexture;             // 280
		BSScaleformExternalTexture dlcImageTextures[14];         // 298
	};
	static_assert(sizeof(MainMenu) == 0x3E8);
}
