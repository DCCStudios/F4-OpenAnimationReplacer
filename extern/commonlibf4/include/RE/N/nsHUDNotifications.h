#pragma once

#include "RE/B/BSFixedString.h"

namespace RE
{
	namespace nsHUDNotifications
	{
		[[nodiscard]] inline bool IsQuestNotification(const BSFixedString* a_type)
		{
			using func_t = decltype(&nsHUDNotifications::IsQuestNotification);
			static REL::Relocation<func_t> func{ ID::nsHUDNotifications::IsQuestNotification };
			return func(a_type);
		}

		[[nodiscard]] inline bool IsObjectiveNotification(const BSFixedString* a_type)
		{
			using func_t = decltype(&nsHUDNotifications::IsObjectiveNotification);
			static REL::Relocation<func_t> func{ ID::nsHUDNotifications::IsObjectiveNotification };
			return func(a_type);
		}

		[[nodiscard]] inline bool IsLocationDiscoveredNotification(const BSFixedString* a_type)
		{
			using func_t = decltype(&nsHUDNotifications::IsLocationDiscoveredNotification);
			static REL::Relocation<func_t> func{ ID::nsHUDNotifications::IsLocationDiscoveredNotification };
			return func(a_type);
		}
	}
}
