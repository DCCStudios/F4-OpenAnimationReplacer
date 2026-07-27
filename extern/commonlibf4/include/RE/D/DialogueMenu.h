#pragma once

#include "RE/B/BSTEvent.h"
#include "RE/B/BSTSmartPointer.h"
#include "RE/G/GameMenuBase.h"
#include "RE/H/HUDPerkVaultBoySwfDisplayEvent.h"
#include "RE/S/ShowingDialogueSpeechChallengeAnim.h"

namespace RE
{
	class BSInputEnableLayer;

	class __declspec(novtable) DialogueMenu :
		public GameMenuBase  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::DialogueMenu };
		static constexpr auto VTABLE{ VTABLE::DialogueMenu };
		static constexpr auto MENU_NAME{ "DialogueMenu"sv };

		// Members
		std::unique_ptr<BSGFxShaderFXTarget>                    dialogueButtonOBJs[4];
		std::unique_ptr<BSGFxShaderFXTarget>                    speechChallengeAnimObj;
		BSTValueEventSink<HUDPerkVaultBoySwfDisplayEvent>       currentVBPerk;
		BSTValueEventSource<ShowingDialogueSpeechChallengeAnim> showingSpeechChallenge;
		BSTSmartPointer<BSInputEnableLayer>                     inputLayer;
		UserEvents::INPUT_CONTEXT_ID                            currentContext;
		bool                                                    isLookingAtPlayer;
		bool                                                    areButtonsShown;
	};
	static_assert(sizeof(DialogueMenu) == 0x168);
}
