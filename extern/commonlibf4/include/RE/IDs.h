#pragma once

namespace RE::ID
{
	namespace ActiveEffect
	{
		inline constexpr REL::VariantID CheckDisplacementSpellOnTarget{ 1415178, 2226001 };
	}

	namespace Actor
	{
		inline constexpr REL::VariantID AddPerk{ 187096, 2230121 };
		inline constexpr REL::VariantID CanUseIdle{ 1223707, 2229592 };
		inline constexpr REL::VariantID ClearAttackStates{ 1525555, 2229773 };
		inline constexpr REL::VariantID EndInterruptPackage{ 575188, 2229892 };
		inline constexpr REL::VariantID ExitCover{ 770035, 2231166 };
		inline constexpr REL::VariantID GetAimVector{ 554863, 2230378 };
		inline constexpr REL::VariantID GetClosestBone{ 1180004, 2230051 };
		inline constexpr REL::VariantID GetCollisionFilter{ REL::Offset{ 0x1D80640 }, 2277949 };  // OG: 1474995 this function calls the desired one inside itself, something like a Singleton.
		inline constexpr REL::VariantID GetCombatStyle{ 1270929, 2231053 };
		inline constexpr REL::VariantID SetCurrentAmmoCount{ 725546, 2229952 };
		inline constexpr REL::VariantID GetCurrentCollisionGroup{ 410500, 2229993 };
		inline constexpr REL::VariantID GetCurrentFireLocation{ 663107, 2231167 };
		inline constexpr REL::VariantID GetDesiredSpeed{ 106892, 2230410 };
		inline constexpr REL::VariantID GetGhost{ 1298473, 2229668 };
		inline constexpr REL::VariantID GetHostileToActor{ 1148686, 2229968 };
		inline constexpr REL::VariantID GetMountHandle{ 313362, 2231230 };
		inline constexpr REL::VariantID GetLevel{ 661617, 2229734 };
		inline constexpr REL::VariantID GetPerkRank{ 1368313, 2230125 };
		inline constexpr REL::VariantID GetSex{ 1216256, 2229674 };
		inline constexpr REL::VariantID HandleDefaultAnimationSwitch{ 1163130, 2229780 };
		inline constexpr REL::VariantID HandleItemEquip{ 164912, 2229781 };
		inline constexpr REL::VariantID HasObjects{ 861256, 2229961 };
		inline constexpr REL::VariantID InitiateDoNothingPackage{ 89993, 2229807 };
		inline constexpr REL::VariantID IsCrippled{ 1238666, 2230998 };
		inline constexpr REL::VariantID IsFollowing{ 629579, 2230013 };
		inline constexpr REL::VariantID IsJumping{ 1041558, 2229640 };
		inline constexpr REL::VariantID IsPathValid{ 1522194, 2230279 };
		inline constexpr REL::VariantID IsPathing{ 989661, 2234312 };
		inline constexpr REL::VariantID IsPathingComplete{ 817283, 2230274 };
		inline constexpr REL::VariantID IsQuadruped{ 1552322, 2229614 };
		inline constexpr REL::VariantID IsSneaking{ 1173627, 2207655 };
		inline constexpr REL::VariantID Move{ 737625, 2229934 };
		inline constexpr REL::VariantID PerformAction{ 1057231, 2231177 };
		inline constexpr REL::VariantID PlaySoundByEditorName{ 763587, 2230172 };
		inline constexpr REL::VariantID RemovePerk{ 1316475, 2230122 };
		inline constexpr REL::VariantID Reset3D{ 302888, 2229913 };
		inline constexpr REL::VariantID RewardExperience{ 262786, 2230428 };
		inline constexpr REL::VariantID SetGunState{ 977675, 2231175 };
		inline constexpr REL::VariantID SetHeading{ 353571, 2229625 };
		inline constexpr REL::VariantID TrespassAlarm{ 1109888, 2229834 };
		inline constexpr REL::VariantID UpdateVoiceTimer{ 1160883, 2230178 };
		inline constexpr REL::VariantID GetBodyPartData{ 1485196, 2229571 };
		inline constexpr REL::VariantID GetEquippedItem{ 1318331, 2231089 };
		inline constexpr REL::VariantID GetCrimeTrackingFaction{ 1108024, 2229787 };
		inline constexpr REL::VariantID ForceDetect{ 131995, 2230194 };
		inline constexpr REL::VariantID GetDesirability{ 1325769, 2229946 };
		inline constexpr REL::VariantID CalcArmorRating1{ 1295757, 2230008 };
		inline constexpr REL::VariantID CalcArmorRating2{ 843406, 2230009 };
		inline constexpr REL::VariantID UnequipObject{ 1363509, 2230479 };
		inline constexpr REL::VariantID UpdateSoundCallBack{ 632367, 2229974 };
		inline constexpr REL::VariantID GetFactionRank{ 761698, 2230100 };
		inline constexpr REL::VariantID IsAngryWithPlayer{ 1433187, 2229998 };
		inline constexpr REL::VariantID RedressIfNeeded{ 323782, 2230394 };
		inline constexpr REL::VariantID UpdateSprinting{ 385539, 2230498 };
		inline constexpr REL::VariantID Jump{ 1394576, 2229650 };
		inline constexpr REL::VariantID GetMobilityCrippled{ 281477, 2230996 };
		inline constexpr REL::VariantID SPECIALModifiedCallback{ 234305, 2231020 };
		inline constexpr REL::VariantID StopInteractingQuick{ 129904, 2231227 };
		inline constexpr REL::VariantID CalculateDetectionFormula{ 589441, 2230213 };
		inline constexpr REL::VariantID DoHitMe{ 881215, 2231148 };
	}

	namespace ActorEquipManager
	{
		inline constexpr REL::VariantID Singleton{ 1174340, 2690994, 4798287 };
		inline constexpr REL::VariantID EquipObject{ 988029, 2231392 };
		inline constexpr REL::VariantID UnequipObject{ 1292493, 2231395 };
		inline constexpr REL::VariantID UnequipItem{ 1316852, 2231399 };
	}

	namespace ActorUtils
	{
		namespace ArmorRatingVisitorBase
		{
			inline constexpr REL::VariantID _operator{ 1119011, 2227206 };
			inline constexpr REL::VariantID ctor{ 1212617, 2227205 };
		}

		inline constexpr REL::VariantID GetEquippedArmorDamageResistance{ 1204866, 2227189 };
	}

	namespace ActorValue
	{
		inline constexpr REL::VariantID Singleton{ 405390, 2189587 };
	}

	namespace AIFormulas
	{
		inline constexpr REL::VariantID GetBarterValue{ 984344, 2208969 };
		inline constexpr REL::VariantID ComputePickpocketSuccess{ 915759, 2208977 };
	}

	namespace AIProcess
	{
		inline constexpr REL::VariantID GetCurrentAmmo{ 1154936, 2232300 };
		inline constexpr REL::VariantID GetCommandType{ 678523, 2231825 };
		inline constexpr REL::VariantID GetOccupiedFurniture{ 1162965, 2232401 };
		inline constexpr REL::VariantID IsWeaponSubgraphFinishedLoading{ 320183, 2231757 };
		inline constexpr REL::VariantID KnockExplosion{ 533106, 2232384 };
		inline constexpr REL::VariantID ProcessGreet{ 1174935, 2231808 };
		inline constexpr REL::VariantID RequestLoadAnimationsForWeaponChange{ 666002, 2231758 };
		inline constexpr REL::VariantID SetActorsDetectionEvent{ 1376336, 2231738 };
		inline constexpr REL::VariantID SetCurrentAmmo{ 795983, 2232302 };
		inline constexpr REL::VariantID SetCommandType{ 1555789, 2231826 };
		inline constexpr REL::VariantID SetEquippedItem{ 1200276, 2231627 };
		inline constexpr REL::VariantID SetupSpecialIdle{ 1446774, 2231704 };
		inline constexpr REL::VariantID SetWeaponBonesCulled{ 397172, 2232535 };
		inline constexpr REL::VariantID StopCurrentIdle{ 434460, 2231705 };
		inline constexpr REL::VariantID SetRunOncePackage{ 155445, 2232344 };
		inline constexpr REL::VariantID AddToProcedureIndexRunning{ 134486, 2718412 };
		inline constexpr REL::VariantID ComputeLastTimeProcessed{ 941571, 2231541 };
		inline constexpr REL::VariantID GetActorLightLevel{ 898888, 2231981 };
	}

	namespace AITimer
	{
		inline constexpr REL::VariantID fTimer{ 936770, 2698609 };
	}

	namespace AnimationSystemUtils
	{
		inline constexpr REL::VariantID WillEventChangeState{ 35074, 2214271 };
	}

	namespace ApplyChangesFunctor
	{
		inline constexpr REL::VariantID WriteDataImpl{ 1291190, 2223194 };
	}

	namespace ApplyColorUpdateEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 421543, 2707340 };
	}

	namespace BarterMenu
	{
		inline constexpr REL::VariantID ClearTradingData{ 1112285, 2222700 };
		inline constexpr REL::VariantID CompleteTrade{ 379932, 2222701 };
		inline constexpr REL::VariantID GetCapsOwedByPlayer{ 672405, 2222714 };
	}

	namespace BGSAnimationSystemUtils
	{
		inline constexpr REL::VariantID GetActiveSyncInfo{ 1349978, 2214289 };
		inline constexpr REL::VariantID InitializeActorInstant{ 672857, 2236393 };
		inline constexpr REL::VariantID IsActiveGraphInTransition{ 839650, 2214305 };
	}

	namespace BGSAttachParentArray
	{
		inline constexpr REL::VariantID SetParentGroupNumber{ 1412266, 2189125 };
	}

	namespace BGSAudio
	{
		inline constexpr REL::VariantID GetSoundDescriptor{ 1305215, 2214758 };
		inline constexpr REL::VariantID GetUIOutputModel{ 1381590, 2214800 };
	}

	namespace BGSConstructibleObject
	{
		inline constexpr REL::VariantID PlayerPassesConditions{ 293575, 2197318 };
	}

	namespace BGSCraftItemEvent
	{
		inline constexpr REL::VariantID EventIndex{ 1382765, 2663409 };
	}

	namespace BGSCreatedObjectManager
	{
		inline constexpr REL::VariantID Singleton{ 1000678, 2689006, 4796296 };
		inline constexpr REL::VariantID DecrementRef{ 230928, 2228455 };
		inline constexpr REL::VariantID IncrementRef{ 1042515, 2228454 };
	}

	namespace BGSDefaultObject
	{
		inline constexpr REL::VariantID Singleton{ 561749, 2690473, 4797762 };
		inline constexpr REL::VariantID ctor{ 1144014, 2197003 };
	}

	namespace BGSDefaultObjectManager
	{
		inline constexpr REL::VariantID Singleton{ 484974, 0 };	// Inlined NG/AE - old ids 2192850 this function get by type (also, the number increased by 1)
	}

	namespace BGSDynamicPersistenceManager
	{
		inline constexpr REL::VariantID Singleton{ 109630, 2689026, 4796312 };
		inline constexpr REL::VariantID PromoteReference{ 1300345, 2193819 };
		inline constexpr REL::VariantID DemoteReference{ 222813, 2193820 };
	}

	namespace BGSEntryPoint
	{
		inline constexpr REL::VariantID HandleEntryPoint{ 714336, 2206243 };
	}

	namespace BGSHackTerminal
	{
		inline constexpr REL::VariantID EventIndex{ 1186942, 2663434 };
	}

	namespace BGSInventoryInterface
	{
		inline constexpr REL::VariantID Singleton{ 501899, 2689299, 4796586 };
		inline constexpr REL::VariantID RequestInventoryItem{ 1200959, 2194009 };
		inline constexpr REL::VariantID ForceMergeStacks{ 1356509, 2194032 };
	}

	namespace BGSInventoryItem
	{
		inline constexpr REL::VariantID FindAndWriteStackData{ 770140, 2194123 };
		inline constexpr REL::VariantID GetDisplayFullName1{ 277641, 2194079 };
		inline constexpr REL::VariantID GetDisplayFullName2{ 623232, 2194080 };
		inline constexpr REL::VariantID GetInstanceData{ 491493, 2194072 };
		inline constexpr REL::VariantID GetInventoryValue{ 430292, 2194099 };
		inline constexpr REL::VariantID GetExtraDataAt{ 851493, 2194092 };
		inline constexpr REL::VariantID MergeStacks{ 1132179, 2194109 };
		inline constexpr REL::VariantID IsEquipped{ 806787, 2194086 };
		inline constexpr REL::VariantID GetStackCount{ 82050, 2194107 };
		inline constexpr REL::VariantID IsQuestObject{ 823447, 2194076 };
		inline constexpr REL::VariantID GetTotalWeight{ 571297, 2194074 };
	}

	namespace BGSInventoryItemUtils
	{
		inline constexpr REL::VariantID GetInventoryValue{ 23543, 2194127 };
	}

	namespace BGSInventoryList
	{
		inline constexpr REL::VariantID FindAndWriteStackDataForItem{ 1354005, 2194179 };
		inline constexpr REL::VariantID BuildFromContainer{ 551792, 2194158 };
		inline constexpr REL::VariantID ctor{ 845050, 2194153 };
		inline constexpr REL::VariantID AddItem1{ 19103, 2194159 };
		inline constexpr REL::VariantID AddItem2{ 98986, 2194160 };
		inline constexpr REL::VariantID RemoveItem1{ 400062, 2194169 };
		inline constexpr REL::VariantID GetItemCount{ 894081, 2194163 };
		inline constexpr REL::VariantID GetQuestItemCount{ 800903, 2194164 };
		inline constexpr REL::VariantID FindItemIndex{ 2111, 2194165 };
		inline constexpr REL::VariantID Clear{ 1080446, 2194162 };
	}

	namespace BGSKeyword
	{
		inline constexpr REL::VariantID TypedKeywords{ 1095775, 2692038, 4799330 };
	}

	namespace BGSKeywordForm
	{
		inline constexpr REL::VariantID AddKeyword{ 762999, 2192766 }; // Check
		inline constexpr REL::VariantID RemoveKeyword{ 921694, 2192767 }; // Check
	}

	namespace BGSListForm
	{
		inline constexpr REL::VariantID AddScriptAddedForm{ 1064874, 2203262 }; // Check
	}

	namespace BGSLoadGameBuffer
	{
		inline constexpr REL::VariantID LoadDataEndian{ 1451631, 2228270 };
	}

	namespace BGSLocalizedStringDL
	{
		inline constexpr REL::VariantID GetText{ 472297, 2194238 };
	}

	namespace BGSMessage
	{
		inline constexpr REL::VariantID AddButton{ 236744, 0 };  // Inlined in NG/AE
		inline constexpr REL::VariantID GetConvertedDescription{ 8331, 2203353 };
	}

	namespace BGSMod
	{
		namespace Attachment
		{
			namespace Mod
			{
				inline constexpr REL::VariantID FindModsForLooseMod{ 410363, 2197524 };
				inline constexpr REL::VariantID GetData{ 33658, 0 };  // Inlined in NG/AE
				inline constexpr REL::VariantID GetLooseMod{ 1359613, 2197514 };
				inline constexpr REL::VariantID SetLooseMod{ 123132, 2197558 };
			}

			inline constexpr REL::VariantID GetAllLooseMods{ 1359613, 2661620 }; // Check
		}

		namespace Container
		{
			inline constexpr REL::VariantID GetData{ 659507, 2189206 };
		}

		namespace Template
		{
			namespace Items
			{
				inline constexpr REL::VariantID CreateInstanceDataForObjectAndExtra{ 147297, 2189244 };
			}
		}
	}

	namespace BGSNumericIDIndex
	{
		inline constexpr REL::VariantID GetNumericID{ 915982, 2228199 };
		inline constexpr REL::VariantID SetNumericID{ 1527703, 2228200 };
	}

	namespace BGSObjectInstance
	{
		inline constexpr REL::VariantID ctor{ 1095748, 2197563 };
	}

	namespace BGSObjectInstanceExtra
	{
		inline constexpr REL::VariantID ctor{ 1222521, 2189015 };
		inline constexpr REL::VariantID AttachModToReference{ 3303, 2189033 };
		inline constexpr REL::VariantID HasMod{ 963890, 2189026 };
		inline constexpr REL::VariantID AddMod{ 1191757, 2189025 };
		inline constexpr REL::VariantID RemoveInvalidMods{ 1548060, 2189028 };
		inline constexpr REL::VariantID RemoveMod{ 1136607, 2189027 };
		inline constexpr REL::VariantID GetNumMods{ 1526690, 2189019 };
	}

	namespace BGSOpenCloseForm
	{
		inline constexpr REL::VariantID AutoCloseRef{ 1175698, 2192797 };
		inline constexpr REL::VariantID GetOpenState{ 1480253, 2192799 };
		inline constexpr REL::VariantID HasOnlyOpenCloseAnims{ 45754, 2192796 };
		inline constexpr REL::VariantID IsAnimating{ 495235, 2192801 };
		inline constexpr REL::VariantID IsOpenCloseEvent{ 1016560, 2192795 };
		inline constexpr REL::VariantID IsOpenCloseForm{ 860629, 2192790 };
		inline constexpr REL::VariantID SetOpenState{ 895942, 2192798 };
	}

	namespace BGSPerkRankArray
	{
		inline constexpr REL::VariantID AllocatePerkRankArray{ 888419, 2192837 };
		inline constexpr REL::VariantID ClearPerks{ 1247917, 2192836 };
	}

	namespace BGSPickLockEvent
	{
		inline constexpr REL::VariantID EventIndex{ 701969, 2663410 };
		inline constexpr REL::VariantID ctor{ 193329, 2214021 };
	}

	namespace BGSPrimitive
	{
		inline constexpr REL::VariantID CreateAbstract{ 1095144, 2194290 };
	}

	namespace BGSProjectile
	{
		inline constexpr REL::VariantID CollidesWithSmallTransparentLayer{ 1115694, 2197620 };
	}

	namespace BGSQuestInstanceText
	{
		inline constexpr REL::VariantID ParseString{ 141681, 2206630 };
	}

	namespace BGSSaveLoadFileEntry
	{
		inline constexpr REL::VariantID DeleteGame{ 91575, 2227855 };
		inline constexpr REL::VariantID LoadData{ 741580, 2227871 };
	}

	namespace BGSSaveGameBuffer
	{
		inline constexpr REL::VariantID SaveDataEndian{ 594281, 2228319 };
	}

	namespace BGSSaveLoadManager
	{
		inline constexpr REL::VariantID Singleton{ 1247320, 2697802 };
		inline constexpr REL::VariantID BuildSaveGameList{ 1342984, 2228053 };
		inline constexpr REL::VariantID GetSaveDirectoryPath{ 1569549, 2228028 };
		inline constexpr REL::VariantID QueueSaveLoadTask{ 1487308, 2228080 };
	}

	namespace BGSSaveLoadGame
	{
		inline constexpr REL::VariantID Singleton{ 177947, 2697789, 2697789 };
		inline constexpr REL::VariantID GetChange{ 688165, 2227898, 2227898 };
	}

	namespace BGSScene
	{
		inline constexpr REL::VariantID ResetAllSceneActions{ 1356678, 2206864 };
		inline constexpr REL::VariantID SetSceneActive{ 820354, 2206845 };
	}

	namespace BGSSceneActionNPCResponseDialogue
	{
		inline constexpr REL::VariantID GetCurrentTopicInfo{ 1369299, 2196825 };
		inline constexpr REL::VariantID UpdateAction1{ 1439851, 2196817 };
		inline constexpr REL::VariantID UpdateAction2{ 574707, 2196800 };
	}

	namespace BGSSceneActionPlayerDialogue
	{
		inline constexpr REL::VariantID GetCurrentTopicInfo{ 781358, 2196825 };
	}

	namespace BGSStoryEventManager
	{
		inline constexpr REL::VariantID Singleton{ 1160067, 2693504, 4800796 };
		inline constexpr REL::VariantID AddEvent{ 312533, 2214088 };
	}

	namespace BGSSynchronizedAnimationManager
	{
		inline constexpr REL::VariantID Singleton{ 1346879, 2690996, 4798289 };
		inline constexpr REL::VariantID IsReferenceInSynchronizedScene{ 915329, 2214437 };
	}

	namespace BGSTerminal
	{
		inline constexpr REL::VariantID GetHackDifficultyLockLevel{ 537273, 2197777 };
		inline constexpr REL::VariantID IsTerminalRefInUse{ 1093565, 2197779 };
		inline constexpr REL::VariantID Show{ 1069012, 2197776 };
		inline constexpr REL::VariantID Activate{ 178045, 2197778 }; // Check
	}

	namespace bhkCharacterController
	{
		inline constexpr REL::VariantID Jump{ 1067306, 2278191 };
	}

	namespace bhkNPCollisionObject
	{
		inline constexpr REL::VariantID CreateObject{ 1264820, 2277912 };
		inline constexpr REL::VariantID CopyMembers{ 1558409, 2277907 };
		inline constexpr REL::VariantID GetBody{ 1127049, 2277926 };
		inline constexpr REL::VariantID GetBodyId{ 1286222, 2277928 };
		inline constexpr REL::VariantID GetShape{ 315427, 2277929 };
		inline constexpr REL::VariantID GetTransform{ 1508189, 2277944 };
		inline constexpr REL::VariantID SetMotionType{ 200912, 2277913 };
		inline constexpr REL::VariantID SetTransform{ 178085, 2277945 };
		inline constexpr REL::VariantID Getbhk{ 730034, 2277919 };
	}

	namespace hknpBodyCinfo
	{
		inline constexpr REL::VariantID ctor{ 718403, 2255967 };
	}

	namespace bhkPhysicsSystem
	{
		inline constexpr REL::VariantID ctor{ 1338184, 2278008 };
		inline constexpr REL::VariantID AddToWorld{ 512878, 2278019 };
		inline constexpr REL::VariantID CopyMembers{ 301764, 2278007 };
	}

	namespace bhkPickData
	{
		inline constexpr REL::VariantID ctor{ 526783, 2230668 };
		inline constexpr REL::VariantID SetStartEnd{ 747470, 2236622 };
		inline constexpr REL::VariantID Reset{ 438299, 2277761 };
		inline constexpr REL::VariantID HasHit{ 1181584, 2277770 };
		inline constexpr REL::VariantID GetHitFraction{ 476687, 2277772 };
		inline constexpr REL::VariantID GetAllCollectorRayHitSize{ 1288513, 2277765 };
		inline constexpr REL::VariantID GetAllCollectorRayHitAt{ 583997, 2277766 };
		inline constexpr REL::VariantID SortAllCollectorHits{ 1274842, 2277767 };
		inline constexpr REL::VariantID GetNiAVObject{ 863406, 2277764 };
		inline constexpr REL::VariantID GetBody{ 1223055, 2277762 };
	}

	namespace bhkUtilFunctions
	{
		inline constexpr REL::VariantID FindFirstCollisionObject{ 507243, 2278093 };
		inline constexpr REL::VariantID GetAVObjectFromBodyID{ 962716, 2278085 };
	}

	namespace bhkWorld
	{
		inline constexpr REL::VariantID AddObjects{ 585426, 2277719 };
		inline constexpr REL::VariantID AddPhysicsSystem{ 82382, 2277752 };
		inline constexpr REL::VariantID IsBodyAdded{ 1062018, 2277997 };
		inline constexpr REL::VariantID RemovePhysicsSystem{ 1039226, 2277753 };
		inline constexpr REL::VariantID Activate{ 1207639, 2277728 };
		inline constexpr REL::VariantID Enable{ 343580, 2277725 };
		inline constexpr REL::VariantID EnableCollision{ 50037, 2277730 };
		inline constexpr REL::VariantID IsActive{ 848114, 2277729 };
		inline constexpr REL::VariantID RemoveObjects{ 1514984, 2277721 };
		inline constexpr REL::VariantID SetCollisionGroup{ 717116, 2277722 };
		inline constexpr REL::VariantID SetDismemberedLimb{ 1566977, 2277726 };
		inline constexpr REL::VariantID SetMotion{ 357289, 2277723 };
	}

	namespace BIPOBJECT
	{
		inline constexpr REL::VariantID dtor{ 765242, 2194330 };
	}

	namespace BSAudioManager
	{
		inline constexpr REL::VariantID Singleton{ 1321158, 2703058 };
		inline constexpr REL::VariantID GetSoundHandle{ 1419045, 2267105 };
		inline constexpr REL::VariantID GetSoundHandleByName{ 196484, 2267104 };
		inline constexpr REL::VariantID GetSoundHandleByFile{ 37039, 2267103 };
	}

	namespace BSAwardsSystemUtility
	{
		inline constexpr REL::VariantID Singleton{ 1569384, 2255115 };
	}

	namespace BSGFxDisplayObject
	{
		inline constexpr REL::VariantID RemoveChild{ 1229383, 2287327 };
	}

	namespace BSGFxObject
	{
		inline constexpr REL::VariantID AcquireFlashObjectByMemberName{ 1172680, 2287014 };
		inline constexpr REL::VariantID AcquireFlashObjectByPath{ 1065592, 2287015 };
	}

	namespace BSGFxShaderFXTarget
	{
		inline constexpr REL::VariantID ProcessEvent{ 848563, 2287033 };
		inline constexpr REL::VariantID AppendShaderFXInfos{ 544646, 2287021 };
		inline constexpr REL::VariantID CreateAndSetFiltersToColor1{ 1487925, 2287028 };
		inline constexpr REL::VariantID CreateAndSetFiltersToColor2{ 783104, 2287029 };
		inline constexpr REL::VariantID CreateAndSetFiltersToHUD{ 876001, 2287027 };
		inline constexpr REL::VariantID EnableShadedBackground{ 278402, 2287022 };
	}

	namespace BSGraphics
	{
		namespace Renderer
		{
			inline constexpr REL::VariantID IncRef{ 1337764, 2276869 };
			inline constexpr REL::VariantID DecRef{ 194808, 2276870 };
			inline constexpr REL::VariantID Begin{ 288964, 2276833 };
			inline constexpr REL::VariantID End{ 700869, 2276834 };
			inline constexpr REL::VariantID Lock{ 1087134, 2276828 }; // Check
			inline constexpr REL::VariantID Unlock{ 382332, 2276829 }; // Check
		}

		namespace RenderTargetManager
		{
			inline constexpr REL::VariantID Singleton{ 1508457, 2666735 };
			inline constexpr REL::VariantID SetEnableDynamicResolution{ 116947, 2277197 };
		}

		namespace State
		{
			inline constexpr REL::VariantID Singleton{ 600795, 2704621 };
		}

		namespace Utility
		{
			inline constexpr REL::VariantID ConvertHALFToNiPoint3Stream{ 463129, 2277113 };
			inline constexpr REL::VariantID ConvertNiPoint3ToHALFStream{ 1247946, 2277114 };
			inline constexpr REL::VariantID PackVertexData{ 156126, 2277106 };
			inline constexpr REL::VariantID UnpackVertexData{ 990748, 2277129 };
		}

		inline constexpr REL::VariantID GetRendererData{ 1235449, 2704429 };
		inline constexpr REL::VariantID GetCurrentRendererWindow{ 91810, 2704431 };
	}

	namespace BSIdleInputWatcher
	{
		inline constexpr REL::VariantID Singleton{ 487306, 2693716, 4801008 };
	}

	namespace BSInputDeviceManager
	{
		inline constexpr REL::VariantID Singleton{ 1284221, 2694300, 4807767 };
	}

	namespace BSInputEnableLayer
	{
		inline constexpr REL::VariantID DecRef{ 659989, 2268272 };
	}

	namespace BSInputEnableManager
	{
		inline constexpr REL::VariantID Singleton{ 781703, 2689007, 4796297 };
		inline constexpr REL::VariantID AllocateNewLayer{ 537494, 2268244 };
		inline constexpr REL::VariantID EnableUserEvent{ 1432984, 2268263 };
		inline constexpr REL::VariantID EnableOtherEvent{ 1419268, 2268265 };
	}

	namespace BSModelDB
	{
		inline constexpr REL::VariantID Demand1{ 1066398, 2275153 };
		inline constexpr REL::VariantID Demand2{ 1225688, 2275154 };
	}

	namespace BSPointerHandle
	{
		namespace BSPointerHandleManagerInterface
		{
			inline constexpr REL::VariantID CreateHandle{ 224532, 2188375 };
			inline constexpr REL::VariantID GetHandle{ 901626, 2188676 };
			inline constexpr REL::VariantID GetSmartPointer{ 967277, 2188681 };
		}
	}

	namespace BSRandom
	{
		inline constexpr REL::VariantID UnsignedInt{ 694400, 2267950 };
		inline constexpr REL::VariantID Float{ 1118937, 2267953 };
		inline constexpr REL::VariantID Float0To1{ 1025602, 2267954 };
		inline constexpr REL::VariantID Int{ 1212543, 2267952 };
	}

	namespace BSResource_Archive2_AsyncReaderStream
	{
		inline constexpr REL::VariantID DoOpen{ 1401160, 2269387 };
		inline constexpr REL::VariantID DoClose{ 883012, 2269380 };
		inline constexpr REL::VariantID DoClone{ 803750, 2269379 };
		inline constexpr REL::VariantID DoStartRead{ 1215072, 2269395 };
		inline constexpr REL::VariantID DoStartPacketAlignedBufferedRead{ 603387, 2269394 };
		inline constexpr REL::VariantID DoWait{ 244066, 2269399 };
	}

	namespace BSResource_Archive2_ReaderStream
	{
		inline constexpr REL::VariantID DoOpen{ 762048, 2269280 };
		inline constexpr REL::VariantID DoClose{ 109149, 2269285 };
		inline constexpr REL::VariantID DoGetKey{ 772817, 2269288 };
		inline constexpr REL::VariantID DoClone{ 855081, 2269279 };
		inline constexpr REL::VariantID DoRead{ 1359347, 2269281 };
		inline constexpr REL::VariantID DoSeek{ 1000164, 2269282 };
		inline constexpr REL::VariantID DoPrefetchAll{ 1518821, 2269390 };
		inline constexpr REL::VariantID DoCreateAsync{ 893405, 2269287 };
	}

	namespace BSResource_EntryDBBaseUtil
	{
		inline constexpr REL::VariantID ReleaseEntryAction{ 777279, 2269463 };
	}

	namespace BSResource_GlobalLocations
	{
		inline constexpr REL::VariantID Singleton{ 1573818, 2703394 };
	}

	namespace BSResource_GlobalPaths
	{
		inline constexpr REL::VariantID Singleton{ 210868, 2703393 };
	}

	namespace BSResource
	{
		inline constexpr REL::VariantID RegisterLocation{ 918408, 2269481 };
		inline constexpr REL::VariantID GetOrCreateStream{ 1143155, 2269490 };

		namespace ID
		{
			inline constexpr REL::VariantID GenerateFromPath{ 570861, 2269259 };
		}
	}

	namespace BSResourceNiBinaryStream
	{
		inline constexpr REL::VariantID Seek{ 358512, 2269834 };
		inline constexpr REL::VariantID DoRead{ 424286, 2269839 };
		inline constexpr REL::VariantID DoWrite{ 1208863, 2269840 };
		inline constexpr REL::VariantID GetBufferInfo{ 265501, 2269836 };
		inline constexpr REL::VariantID Ctor{ 1198116, 2269830 };
		inline constexpr REL::VariantID Dtor{ 1516202, 2269832 };
		inline constexpr REL::VariantID BinaryStreamWithRescan{ 543595, 2205871 };
	}

	namespace BSScaleformManager
	{
		inline constexpr REL::VariantID Singleton{ 106578, 2689600, 4796889 };
		inline constexpr REL::VariantID LoadMovie{ 1526234, 2287422 };
		inline constexpr REL::VariantID GetMovieFilename{ 1191277, 2287428 };
		inline constexpr REL::VariantID InitMovieViewport{ 206895, 2287430, 4494254 };
		inline constexpr REL::VariantID FileUncacheOnMenuOpen{ 454316, 2667999 };
		inline constexpr REL::VariantID IsNameValid{ 593721, 2287425 }; // Check
	}

	namespace BSScaleformRenderer
	{
		inline constexpr REL::VariantID Initialize{ 1403529, 2284938 };
	}

	namespace BSScaleformTranslator
	{
		inline constexpr REL::VariantID AddTranslations{ 810671, 2295298 };
	}

	namespace BSScript_Array
	{
		inline constexpr REL::VariantID ctor{ 870924, 2314915 }; // Check
	}

	namespace BSScript_ArrayWrapper
	{
		inline constexpr REL::VariantID ReplaceArray{ 445184, 2250435 };
	}

	namespace BSScript_Internal_NativeFunctionBase
	{
		inline constexpr REL::VariantID Call{ 571037, 2314780 };
	}

	namespace BSScript_Internal_Stack
	{
		inline constexpr REL::VariantID GetPageForFrame{ 1429302, 2314680 };
		inline constexpr REL::VariantID GetStackFrameVariable{ 897539, 2314681 };
	}

	namespace BSScript_Object
	{
		inline constexpr REL::VariantID DecRef{ 541793, 2314437 };
		inline constexpr REL::VariantID GetHandle{ 1452752, 2314431 }; // Check
		inline constexpr REL::VariantID IncRef{ 461710, 2314436 };
	}

	namespace BSScript_ObjectTypeInfo
	{
		inline constexpr REL::VariantID dtor{ 1047917, 2314513 }; // Check
	}

	namespace BSScript_ObjectBindPolicy
	{
		inline constexpr REL::VariantID BindObject{ 709728, 2314370 }; // Check
		inline constexpr REL::VariantID UnbindObject{ 522763, 2314372 }; // Check
	}

	namespace BSScriptUtil
	{
		inline constexpr REL::VariantID BaseObjectName{ 648543, 2702269 }; // Check
	}

	namespace BSShaderProperty
	{
		inline constexpr REL::VariantID SetMaterial{ 706318, 2316285 };
	}

	namespace BSShaderResourceManager
	{
		inline constexpr REL::VariantID Singleton{ 583240, 2703483 };
	}

	namespace BSShaderTextureSet
	{
		inline constexpr REL::VariantID CreateObject{ 993063, 2316324 };
	}

	namespace BSShaderUtil
	{
		inline constexpr REL::VariantID SetMaterialAlpha{ 211260, 2317566 };
	}

	namespace BSSoundHandle
	{
		inline constexpr REL::VariantID ClearFollowedObject{ 208569, 2267067 };
		inline constexpr REL::VariantID FadeInPlay{ 353528, 2267075 };
		inline constexpr REL::VariantID FadeOutAndRelease{ 260328, 2267076 };
		inline constexpr REL::VariantID FadeTo{ 446556, 2267074 };
		inline constexpr REL::VariantID GetDuration{ 1192939, 2267062 };
		inline constexpr REL::VariantID IsEnvelopeLoop{ 529498, 2267051 };
		inline constexpr REL::VariantID IsPaused{ 543387, 2267047 };
		inline constexpr REL::VariantID IsPlaying{ 1514207, 2267046 };
		inline constexpr REL::VariantID IsValid{ 1154607, 2267048 };
		inline constexpr REL::VariantID Pause{ 1168427, 2267044 };
		inline constexpr REL::VariantID Play{ 384073, 2267042 };
		inline constexpr REL::VariantID PlayAfter{ 557072, 2267043 };
		inline constexpr REL::VariantID SetFrequency{ 940583, 2267059 };
		inline constexpr REL::VariantID SetFrequencyVariance{ 1552262, 2267060 };
		inline constexpr REL::VariantID SetObjectToFollow{ 1179144, 2267066 };
		inline constexpr REL::VariantID SetOutputModel{ 170407, 2267053 };
		inline constexpr REL::VariantID SetPosition{ 1538508, 2267064 };
		inline constexpr REL::VariantID SetStaticAttenuation{ 581557, 2267061 };
		inline constexpr REL::VariantID Stop{ 1340948, 2267045 };
	}

	namespace BSSpinLock
	{
		namespace BSReadWriteLock
		{
			inline constexpr REL::VariantID lock_read{ 1573164, 2267897 };
			inline constexpr REL::VariantID lock_write{ 336186, 2267898 };
			inline constexpr REL::VariantID try_lock_read{ 1372435, 2267901 };
			inline constexpr REL::VariantID try_lock_write{ 1279453, 2267902 };
		}

		inline constexpr REL::VariantID lock{ 1425657, 2192245 };
		inline constexpr REL::VariantID try_lock{ 267930, 2267902 };
	}

	namespace BSStringPool
	{
		namespace Entry
		{
			inline constexpr REL::VariantID Release{ 1204430, 2268720 };
		}

		inline constexpr REL::VariantID GetEntry_char{ 507142, 2268729 };
		inline constexpr REL::VariantID GetEntry_wchar{ 345043, 2268730 };
	}

	namespace BSStringT
	{
		inline constexpr REL::VariantID Set{ 407804, 2189084, 4471693 }; // Check
	}

	namespace BSSystemFileStreamer
	{
		inline constexpr REL::VariantID UncacheAll{ 40925, 2268797 };
	}

	namespace BSTempEffectDebris
	{
		inline constexpr REL::VariantID ctor{ 1075623, 2212059 };
	}

	namespace BSTempEffectWeaponBlood
	{
		inline constexpr REL::VariantID ClearEffectForWeapon{ 718866, 2212158 };
	}

	namespace BSTEvent
	{
		namespace BSTGlobalEvent
		{
			inline constexpr REL::VariantID Singleton{ 1424022, 2688814, 4796078 };
		}
	}

	namespace BSThreadEvent
	{
		inline constexpr REL::VariantID InitSDM{ 1425097, 2268180 };
	}

	namespace BSTimer
	{
		inline constexpr REL::VariantID Singleton{ 1256126, 2703179 };
		inline constexpr REL::VariantID QGlobalTimeMultiplier{ 365546, 2666307 };
		inline constexpr REL::VariantID QGlobalTimeMultiplierTarget{ 1266509, 2666308 };
		inline constexpr REL::VariantID SetGlobalTimeMultiplier{ 1419977, 2267970 };
	}

	namespace BSUIMessageData
	{
		inline constexpr REL::VariantID SendUIBoolMessage{ 1388308, 2284981 };
		inline constexpr REL::VariantID SendUIStringMessage{ 1270833, 2284977 };
		inline constexpr REL::VariantID SendUIPtrMessage{ 1374542, 2284982 };
		inline constexpr REL::VariantID SendUIStringUIntMessage{ 99795, 2284979 };
	}

	namespace BSUtilities
	{
		inline constexpr REL::VariantID ConvertFloatToHalf{ 10414, 2212098 };
		inline constexpr REL::VariantID ConvertHalfToFloat{ 1502141, 2195843 };
		inline constexpr REL::VariantID GetObjectByName{ 843650, 2274841 };
		inline constexpr REL::VariantID GetFlattenedBoneTree{ 1539401, 2274858 }; // Check
	}

	namespace Calendar
	{
		inline constexpr REL::VariantID Singleton{ 1444952, 2689092, 4796378 };
		inline constexpr REL::VariantID GetDaysPassed{ 1209930, 2228563 }; // Check
	}

	namespace CanDisplayNextHUDMessage
	{
		inline constexpr REL::VariantID GetEventSource{ 344866, 2695041, 4802332 };
	}

	namespace CellAttachDetachEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 862142, 2192250 };
	}

	namespace ColorUpdateEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 1226590, 2695540, 4802831 };
	}

	namespace CombatBehaviorFindObject
	{
		inline constexpr REL::VariantID EvaluateArmor{ 1333384, 2241004 }; // Check
	}

	namespace CombatFormulas
	{
		inline constexpr REL::VariantID GetWeaponDisplayAccuracy{ 1137654, 2209049 };
		inline constexpr REL::VariantID GetWeaponDisplayDamage{ 1431014, 2209046 };
		inline constexpr REL::VariantID GetWeaponDisplayRange{ 1324037, 2209047 };
		inline constexpr REL::VariantID GetWeaponDisplayRateOfFire{ 1403591, 2209048 };
		inline constexpr REL::VariantID CalcTargetedLimbDamage{ 1530751, 2209033 };
		inline constexpr REL::VariantID CalcWeaponDamage{ 211411, 2209001 };
		inline constexpr REL::VariantID GetNumCrippledAttackConditions{ 170270, 2209041 };
		inline constexpr REL::VariantID CalcScopeSteadyActionPointDrain{ 380926, 2209045 };
		inline constexpr REL::VariantID CalcResistedPercentage{ 420470, 2209007 };
	}

	namespace CombatUtilities
	{
		inline constexpr REL::VariantID CalculateProjectileLOS1{ 798616, 2240617 };
		inline constexpr REL::VariantID CalculateProjectileLOS2{ 55339, 2240616 };
		inline constexpr REL::VariantID CalculateProjectileTrajectory{ 1575156, 2240611 };
		inline constexpr REL::VariantID WorldGravity{ 1378547, 2700340 };
		inline constexpr REL::VariantID IsActorUsingMelee{ 858050, 2240626 }; // Check
		inline constexpr REL::VariantID IsActorUsingUnarmed{ 1483696, 2240625 }; // Check
	}

	namespace ComparisonQualifiers
	{
		inline constexpr REL::VariantID ContainerQualifier{ 883895, 2190674 };
		inline constexpr REL::VariantID UIQualifier{ 179412, 2190675 };
	}

	namespace Console
	{
		inline constexpr REL::VariantID ExecuteCommand{ 1061864, 2248537 };
		inline constexpr REL::VariantID GetCurrentPickIndex{ 1407033, 2701382 };
		inline constexpr REL::VariantID GetPickRef{ 170742, 2701395 };
		inline constexpr REL::VariantID GetPickRefs{ 875116, 2701391 };
		inline constexpr REL::VariantID SetCurrentPickREFR{ 79066, 2248551 };
		inline constexpr REL::VariantID DisplayRef{ 1107782, 2248550 }; // Check
	}

	namespace ConsoleLog
	{
		inline constexpr REL::VariantID Singleton{ 689441, 2690148, 4797437 };
		inline constexpr REL::VariantID AddString{ 764, 2248593 };
		inline constexpr REL::VariantID Print{ 799546, 2248591 };
	}

	namespace ContainerMenu
	{
		inline constexpr REL::VariantID TakeAllItems{ 1323703, 2248619 };
	}

	namespace ContainerMenuBase
	{
		namespace ItemSorter
		{
			inline constexpr REL::VariantID IncrementSort{ 1307263, 2222848 };
		}
	}

	namespace ControlMap
	{
		inline constexpr REL::VariantID Singleton{ 325206, 2692014, 4799307 };
		inline constexpr REL::VariantID PopInputContext{ 74587, 2268336 };
		inline constexpr REL::VariantID PushInputContext{ 1404410, 2268335 };
		inline constexpr REL::VariantID RemapButton{ 11351, 2268330 };
		inline constexpr REL::VariantID SaveRemappings{ 1141541, 2268331 };
		inline constexpr REL::VariantID SetTextEntryMode{ 1270079, 2268339, 4491359 };
	}

	namespace CurrentRadiationSourceCount
	{
		inline constexpr REL::VariantID GetEventSource{ 696410, 2696196, 4803487 };
	}

	namespace DEFAULT_OBJECT_DATA
	{
		inline constexpr REL::VariantID GetDefaultObjectData{ 838886 }; // Same
	}

	namespace DialogueCameraState
	{
		inline constexpr REL::VariantID UpdateActorsAndAction{ 24497, 2214898 }; // Check
	}

	namespace DialogueMenuUtils
	{
		inline constexpr REL::VariantID OpenMenu{ 1562703, 2249658 };
		inline constexpr REL::VariantID CloseMenu{ 764651, 2249659 };
		inline constexpr REL::VariantID ShowButtons{ 191452, 2249663 };
		inline constexpr REL::VariantID ShowSpeechChallengeAnim{ 868159, 2249665 };
	}

	namespace DoBeforeNewOrLoadCompletedEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 787908, 2695542, 4802833 };
	}

	namespace EffectItem
	{
		inline constexpr REL::VariantID GetDescription{ 949839, 2189612 };
	}

	namespace EquippedWeaponData
	{
		inline constexpr REL::VariantID SetupFireSounds{ 1468462, 2232275 };
	}

	namespace ExamineMenu
	{
		inline constexpr REL::VariantID BuildWeaponScrappingArray{ 646841, 2223077 };
		inline constexpr REL::VariantID GetSelectedIndex{ 776503, 2223022 };
		inline constexpr REL::VariantID ShowConfirmMenu{ 443081, 2223081 };
		inline constexpr REL::VariantID ConsumeSelectedItems{ 686586, 2223052 };
		inline constexpr REL::VariantID BuildConfirmed{ 1396668, 2223013 };
		inline constexpr REL::VariantID GetBuildConfirmQuestion{ 1360189, 2223057 }; // Check
	}

	namespace ExteriorCellSingleton
	{
		inline constexpr REL::VariantID Singleton{ 128691, 2689084, 4796370 };
	}

	namespace ExtraDataList
	{
		inline constexpr REL::VariantID CreateInstanceData{ 1280130, 2190185 };
		inline constexpr REL::VariantID GetLegendaryMod{ 178075, 2190180 };
		inline constexpr REL::VariantID SetBendableSplineInfo{ 894306, 2190623 };
		inline constexpr REL::VariantID SetDisplayNameFromInstanceData{ 457340, 2190179 };
		inline constexpr REL::VariantID SetOverrideName{ 222303, 2190167 };
		inline constexpr REL::VariantID SetStartingWorldOrCell{ 603621, 2190506 };
		inline constexpr REL::VariantID GetHealthPerc{ 196530, 2190226 };
		inline constexpr REL::VariantID SetHealthPerc{ 1208294, 2190124 };
		inline constexpr REL::VariantID ClearFavorite{ 254434, 2190191 };
		inline constexpr REL::VariantID IsFavorite{ 786568, 2190189 };
		inline constexpr REL::VariantID IsDamaged{ 116730, 2190224 }; // Check
		inline constexpr REL::VariantID CompareList{ 585876, 2190098 };
		inline constexpr REL::VariantID SetFavorite{ 534268, 2190188 }; // Check
		inline constexpr REL::VariantID GetPrimitive{ 1271508, 2190427 }; // Check
		inline constexpr REL::VariantID Ctor{ 1329859, 2190088 };
		inline constexpr REL::VariantID CopyList{ 561304, 2190094 };
		inline constexpr REL::VariantID SetCount{ 1460465, 2190125 };
	}

	namespace ExtraTextDisplayData
	{
		inline constexpr REL::VariantID GetDisplayName{ 1523343, 2191318 };
	}

	namespace FavoritesManager
	{
		inline constexpr REL::VariantID Singleton{ 198281, 2694399, 4801690 };
		inline constexpr REL::VariantID IsComponentFavorite{ 352046, 2248752 };
		inline constexpr REL::VariantID UseQuickkeyItem{ 303130, 2248744 };
		inline constexpr REL::VariantID HandleEvent{ 1049251, 2248740 }; // Check
		inline constexpr REL::VariantID Call{ 1566377, 2248766 }; // Check
		inline constexpr REL::VariantID ClearCurrentAmmoCount{ 541959, 2248745 };
	}

	namespace FlatScreenModel
	{
		inline constexpr REL::VariantID Singleton{ 847741, 2694427, 4801718 };
	}

	namespace GameMenuBase
	{
		inline constexpr REL::VariantID SetIsTopButtonBar{ 1367353, 2223204 };
		inline constexpr REL::VariantID OnMenuDisplayStateChanged{ 1274450, 2223205 };
		inline constexpr REL::VariantID CacheShaderFXQuadsForRenderer_Impl{ 863029, 2223200 };
		inline constexpr REL::VariantID TransferCachedShaderFXQuadsForRenderer{ 65166, 2223201 };
		inline constexpr REL::VariantID SetViewportRect{ 1554334, 2223202 };
		inline constexpr REL::VariantID AppendShaderFXInfos{ 583584, 2223203 };
		inline constexpr REL::VariantID SetUpButtonBar{ 531584, 2223197 };
	}

	namespace GamePlayFormulas
	{
		inline constexpr REL::VariantID CanHackGateCheck{ 269668, 2209069 };
		inline constexpr REL::VariantID CanPickLockGateCheck{ 1160841, 2209066 };
		inline constexpr REL::VariantID GetExperienceReward{ 853500, 2209076 };
		inline constexpr REL::VariantID GetLockXPReward{ 880926, 2209070 };
		inline constexpr REL::VariantID CalculateItemHealthDamageBonus{ 612133, 2209087 };
		inline constexpr REL::VariantID CalcSpringingActionPoints{ 1053786, 2209071 };
		inline constexpr REL::VariantID GetSweetSpotAngle{ 293819, 2209082 }; // Check
		inline constexpr REL::VariantID GetPartialPickAngle{ 1078228, 2209083 }; // Check
		inline constexpr REL::VariantID GetPickBreakSeconds{ 984807, 2209084 }; // Check
		inline constexpr REL::VariantID GetHackingWordCount{ 190347, 2209067 };
		inline constexpr REL::VariantID CalculateItemValue{ 960532, 2209074 }; // Check
		inline constexpr REL::VariantID GetForceLockChance{ 549534, 2209065 }; // Check
	}

	namespace GameScript
	{
		namespace HandlePolicy
		{
			inline constexpr REL::VariantID DropSaveLoadRemapData{ 647657, 2249995 };
			inline constexpr REL::VariantID GetInventoryObjFromHandle{ 66597, 2249989 };
			inline constexpr REL::VariantID UpdatePersistence{ 11020, 2249991 };
		}

		namespace ObjectBindPolicy
		{
			inline constexpr REL::VariantID EndSaveLoad{ 1558837, 2250482 };
		}

		namespace GameVM
		{
			inline constexpr REL::VariantID Singleton{ 996227, 2689134, 4796420 };
			inline constexpr REL::VariantID QueuePostRenderCall{ 34412, 2251314 };
			inline constexpr REL::VariantID RegisterForAllGameEvents{ 680454, 2251353 };
			inline constexpr REL::VariantID SendEventToObjectAndRelated{ 367992, 2251344 };
		}

		inline constexpr REL::VariantID LogFormError{ 1081933, 2251028 };
		inline constexpr REL::VariantID BindCObject{ 81787, 2249771 };
	}

	namespace GameUIModel
	{
		inline constexpr REL::VariantID Singleton{ 17419, 2695523, 4802814 };
		inline constexpr REL::VariantID SetGameColors{ 523665, 2220911 };
		inline constexpr REL::VariantID UpdateDataModels{ 1269653, 2220905 };
	}

	namespace GridCellArray
	{
		inline constexpr REL::VariantID Get{ 1330136, 2194566 }; // Check
	}

	namespace hkContainerHeapAllocator
	{
		inline constexpr REL::VariantID Singleton{ 409811, 2665426 };
	}

	namespace hkDisplaySerializeOStream
	{
		inline constexpr REL::VariantID ctor{ 371904, 2266374 };
	}

	namespace hkHardwareInfo
	{
		inline constexpr REL::VariantID CalcNumHardwareThreads{ 1392261, 2260765 };
	}

	namespace hkIArchive
	{
		inline constexpr REL::VariantID ctor2{};
	}

	namespace hkMemoryRouter
	{
		inline constexpr REL::VariantID tlsSlotID{ 878080, 2787927 };
	}

	namespace hkMonitorStream
	{
		inline constexpr REL::VariantID tlsSlotID{ 1467653, 2702743 };
		inline constexpr REL::VariantID Clear{ 667853, 2257312 };
		inline constexpr REL::VariantID Resize{ 854140, 2257310 };
	}

	namespace hknpCapsuleShape
	{
		inline constexpr REL::VariantID Init{ 647475, 2257217 };
		inline constexpr REL::VariantID CreateCapsuleShape{ 1316723, 2257207 };
	}

	namespace hknpConvexPolytopeShape
	{
		inline constexpr REL::VariantID CreateFromVerticesInternal{ 720930, 2257201 };
	}

	namespace hknpConvexShape
	{
		inline constexpr REL::VariantID CreateFromAabb{ 1144958, 2257125 };
		inline constexpr REL::VariantID CreateFromCylinder{ 39591, 2257126 };
		inline constexpr REL::VariantID CreateFromHalfExtents{ 972150, 2257124 };
		inline constexpr REL::VariantID CreateFromIndexedVertices{ 308779, 2257123 };
		inline constexpr REL::VariantID CreateFromVertices{ 1389383, 2257122 };
	}

	namespace hknpMotionCinfo
	{
		inline constexpr REL::VariantID ctor{ 1248706, 2258369 };
		inline constexpr REL::VariantID InitializeAsKeyFramed{ 1204390, 2258373 };
	}

	namespace hknpPhysicsSystem
	{
		inline constexpr REL::VariantID AddToWorld{ 189542, 2255984 };
		inline constexpr REL::VariantID RemoveFromWorld{ 773481, 2255985 };
	}

	namespace hknpSphereShape
	{
		inline constexpr REL::VariantID CreateSphereShape{ 1116388, 2256377 };
	}

	namespace hknpTriangleShape
	{
		inline constexpr REL::VariantID CreateEmptyTriangleShape{ 1355865, 2256226 };
		inline constexpr REL::VariantID SetVertices{ 1435439, 2256228 };
	}

	namespace hknpBSWorld
	{
		inline constexpr REL::VariantID AccessMotion{ 979298, 2277680 };
		inline constexpr REL::VariantID ActivateBody{ 1259285, 2277677 };
		inline constexpr REL::VariantID SetBodyAngularVelocity{ 1020581, 2277672 };
		inline constexpr REL::VariantID SetBodyKeyframed{ 527037, 2277684 };
		inline constexpr REL::VariantID SetBodyLinearVelocity{ 1552859, 2277671 };
		inline constexpr REL::VariantID SetBodyStatic{ 1058828, 2277683 };
		inline constexpr REL::VariantID SetBodyVelocity{ 1300379, 2277670 };
	}

	namespace hknpWorld
	{
		inline constexpr REL::VariantID AddBodies{ 1454855, 2255681 };
		inline constexpr REL::VariantID CreateBody{ 72830, 2255680 };
		inline constexpr REL::VariantID CreateMotion{ 1574333, 2255688 };
		inline constexpr REL::VariantID DisableBodyFlags{ 1506647, 2255636 };
		inline constexpr REL::VariantID EnableBodyFlags{ 987833, 2255635 };
		inline constexpr REL::VariantID GetActionManager{ 1548356, 2255709 };
		inline constexpr REL::VariantID SetBodyCollisionFilterInfo{ 1114792, 2255626 };
		inline constexpr REL::VariantID SetBodyMotion{ 618186, 2255634 };
	}

	namespace hkOArchive
	{
		inline constexpr REL::VariantID ctor2{ 650732, 2259258 };
	}

	namespace hkProcessFactory
	{
		inline constexpr REL::VariantID Singleton{ 111134, 2702978 };
		inline constexpr REL::VariantID CreateProcess{ 1489919, 2263377 };
	}

	namespace hkReferencedObject
	{
		inline constexpr REL::VariantID AddReference{ 866015, 2259213 };
		inline constexpr REL::VariantID RemoveReference{ 1379897, 2195747 };
	}

	namespace hkStringPtr
	{
		inline constexpr REL::VariantID ctor{ 1051944, 2255573 };
	}

	namespace hkVisualDebugger
	{
		inline constexpr REL::VariantID ctor{ 1503084, 2263295 };
		inline constexpr REL::VariantID AddContext{ 136058, 2263313 };
		inline constexpr REL::VariantID AddDefaultProcess{ 926323, 2263302 };
		inline constexpr REL::VariantID Serve{ 1036317, 2263296 };
		inline constexpr REL::VariantID Shutdown{ 156043, 2263300 };
	}

	namespace hkVtableClassRegistry
	{
		inline constexpr REL::VariantID Singleton{ 1281220, 2702926 };
	}

	namespace HolotapeMenu
	{
		inline constexpr REL::VariantID ShowHolotapeInPipboy{ 217953, 2224028 };
		inline constexpr REL::VariantID ShowHolotapeInTerminal{ 390509, 2224029 };
	}

	namespace HUDExperienceMeter
	{
		inline constexpr REL::VariantID UpdateDisplayObject{ 246555, 2220382 }; // Check
	}

	namespace HUDMenu
	{
		inline constexpr REL::VariantID SetHUDColor{ 505571, 2248918 };
	}

	namespace HUDMenuUtils
	{
		inline constexpr REL::VariantID GetGameplayHUDColor{ 34363, 2248840 };
		inline constexpr REL::VariantID GetGameplayHUDBackgroundColor{ 698605, 2248845 };
		inline constexpr REL::VariantID WorldPtToScreenPt3{ 1132313, 2222464 };
	}

	namespace HUDModeEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 683142, 2694697, 4801988 };
	}

	namespace IAnimationGraphManagerHolder
	{
		inline constexpr REL::VariantID SetGraphVariableBool{ 1437197, 2214543 };
		inline constexpr REL::VariantID SetGraphVariableFloat{ 27400, 2214545 };
		inline constexpr REL::VariantID SetGraphVariableInt{ 732926, 2214544 };
		inline constexpr REL::VariantID RevertAnimationGraphManager{ 41382, 2214541 };
	}

	namespace IFormFactory
	{
		inline constexpr REL::VariantID Factories{ 228366, 2689177, 4796464 };
	}

	namespace ImageSpaceEffectFullScreenBlur
	{
		inline constexpr REL::VariantID Disable{ 372489, 2713225 };
	}

	namespace ImageSpaceEffectGetHit
	{
		inline constexpr REL::VariantID Disable{ 1523708, 2713237 };
	}

	namespace ImageSpaceEffectHDR
	{
		inline constexpr REL::VariantID UsePipboyScreenMask{ 405286, 2678029, 4784697 };
	}

	namespace ImageSpaceEffectPipboyScreen
	{
		inline constexpr REL::VariantID PowerArmorPipboy{ 1065542, 2713203 };
	}

	namespace ImageSpaceEffectRadialBlur
	{
		inline constexpr REL::VariantID Disable{ 1500590, 2713225 };
	}

	namespace ImageSpaceEffectTemporalAA
	{
		inline constexpr REL::VariantID MaskS{ 775377, 2713052 };
	}

	namespace ImageSpaceManager
	{
		inline constexpr REL::VariantID Singleton{ 161743, 2712627 };
	}

	namespace ImageSpaceModifierInstance
	{
		inline constexpr REL::VariantID Stop{ 1298537, 2199897 };
	}

	namespace ImageSpaceModifierInstanceDOF
	{
		inline constexpr REL::VariantID Trigger{ 1158048, 2199922 };
	}

	namespace ImageSpaceModifierInstanceForm
	{
		inline constexpr REL::VariantID Trigger1{ 179769, 2199906 };
		inline constexpr REL::VariantID Trigger2{ 1216312, 2199907 };
		inline constexpr REL::VariantID Stop1{ 217873, 2199909 };
		inline constexpr REL::VariantID Stop2{ 549773, 2199910 };
	}

	namespace IMenu
	{
		inline constexpr REL::VariantID ShouldHandleEvent{ 1241790, 2287392 };
		inline constexpr REL::VariantID PassesRenderConditionText{ 937304, 2287379 };
		inline constexpr REL::VariantID OnSetSafeRect{ 964859, 2287375 };
		inline constexpr REL::VariantID ProcessScaleformEvent{ 150211, 2287395 };
		inline constexpr REL::VariantID RefreshPlatform{ 1071829, 2287374 };
	}

	namespace Interface3D
	{
		inline constexpr REL::VariantID Create{ 88488, 2222519 };
		inline constexpr REL::VariantID GetByName{ 140387, 2222524 };
		inline constexpr REL::VariantID MainScreen_AddPointLight{ 1335675, 2222531 };
		inline constexpr REL::VariantID MainScreen_AddSpotLight{ 1056093, 2222532 };
		inline constexpr REL::VariantID MainScreen_ClearLights{ 683530, 2222533 };
		inline constexpr REL::VariantID MainScreen_SetScreenAttached3D{ 817727, 2222528 };
		inline constexpr REL::VariantID MainScreen_SetWorldAttached3D{ 724778, 2222529 };
		inline constexpr REL::VariantID Offscreen_AddLight{ 1267198, 2222543 };
		inline constexpr REL::VariantID Offscreen_Clear3D{ 1533262, 2222542 };
		inline constexpr REL::VariantID Offscreen_ClearLights{ 1039253, 2222545 };
		inline constexpr REL::VariantID Offscreen_GetRenderTargetHeight{ 1280897, 2222538 };
		inline constexpr REL::VariantID Offscreen_GetRenderTargetWidth{ 403273, 2222537 };
		inline constexpr REL::VariantID Offscreen_Set3D{ 43983, 2222541 };
		inline constexpr REL::VariantID Offscreen_SetDebugMode{ 1277879, 2222549 };
		inline constexpr REL::VariantID Offscreen_SetDirectionalLight{ 987144, 2222544 };
		inline constexpr REL::VariantID Enable{ 326895, 2222522 };
		inline constexpr REL::VariantID Disable{ 938370, 2222523 };
		inline constexpr REL::VariantID DisableAll{ 1470633, 2222521 };
		inline constexpr REL::VariantID Release{ 74749, 2222520 };
		inline constexpr REL::VariantID SetViewport{ 825410, 2222552 };
	}

	namespace Inventory3DManager
	{
		inline constexpr REL::VariantID Begin3D{ 662659, 2249084 };
		inline constexpr REL::VariantID ClearModel{ 63218, 2249113 };
		inline constexpr REL::VariantID DisableRendering{ 255893, 2249099 };
		inline constexpr REL::VariantID EnableRendering{ 176578, 2249098 };
		inline constexpr REL::VariantID End3D{ 1512675, 2249086 };
		inline constexpr REL::VariantID SetModelScale{ 1319701, 2249097 };
		inline constexpr REL::VariantID SetModelScreenPosition{ 2967, 2249096 };
	}

	namespace InventoryItemDisplayData
	{
		inline constexpr REL::VariantID ctor{ 679373, 2222612 };
	}

	namespace InventoryUserUIUtils
	{
		inline constexpr REL::VariantID AddItemCardInfoEntry{ 489521, 2222648 };
		inline constexpr REL::VariantID PopulateItemCardInfo_Helper{ 475551, 2222625 }; // Check
	}

	namespace ItemCrafted
	{
		inline constexpr REL::VariantID NotifyOfItemCrafted{ 788895, 2232809 };
		inline constexpr REL::VariantID RegisterSink{ 1320496, 2232807 };
		inline constexpr REL::VariantID UnregisterSink{ 1054567, 2232808 };
	}

	namespace LevelIncrease
	{
		inline constexpr REL::VariantID GetEventSource{ 349120, 2233736 };
	}

	namespace LoadingMenu
	{
		inline constexpr REL::VariantID StartTestingLoadMenu{ 933696, 2249224 };
		inline constexpr REL::VariantID PopulateLoadScreens{ 316170, 2249232 };
	}

	namespace LockpickingMenu
	{
		inline constexpr REL::VariantID OpenLockpickingMenu{ 129892, 2249263 };
		inline constexpr REL::VariantID SendLockInfoToMenu{ 1256681, 2249267 }; // Check
		inline constexpr REL::VariantID DamageLockpick{ 1118991, 2249275 };
	}

	namespace LocksPicked
	{
		inline constexpr REL::VariantID GetEventSource{ 594991, 2249292 };
	}

	namespace MagicTarget
	{
		inline constexpr REL::VariantID IsTakingHealthDamageFromActiveEffect{ 999442, 2226397 };
		inline constexpr REL::VariantID IsTakingRadDamageFromActiveEffect{ 1079111, 2226398 };
	}

	namespace Main
	{
		inline constexpr REL::VariantID Singleton{ 756304, 2698043 };
		inline constexpr REL::VariantID WorldRootCamera{ 384264, 2228956 };
		inline constexpr REL::VariantID WorldRootNode{ 569647, 2698065 };
		inline constexpr REL::VariantID QGameSystemsShouldUpdate{ 779552, 2698031 };
		inline constexpr REL::VariantID QGameDataLoaded{ 1004331, 2698032 };
		inline constexpr REL::VariantID SetCameraFOV{ 1348813, 2228973 };
	}

	namespace MapMarkerData
	{
		inline constexpr REL::VariantID GetLocationName{ 50980, 2191791 };
	}

	namespace MemoryManager
	{
		namespace AutoScrapBuffer
		{
			inline constexpr REL::VariantID ctor{ 1571567, 2267865 };
			inline constexpr REL::VariantID dtor{ 68625, 2267867 };
		}

		inline constexpr REL::VariantID Singleton{ 343176, 2193197, 4471522 };
		inline constexpr REL::VariantID Allocate{ 652767, 2267872 };
		inline constexpr REL::VariantID Deallocate{ 1582181, 2267874 };
		inline constexpr REL::VariantID GetThreadScrapHeap{ 1495205, 2267850 };
		inline constexpr REL::VariantID Reallocate{ 1502917, 2267873 };
		inline constexpr REL::VariantID RegisterMemoryManager{ 453212, 2228356 };
		inline constexpr REL::VariantID Size{ 1453698, 2267858 };
	}

	namespace MenuControls
	{
		inline constexpr REL::VariantID ForcePauseGame{ 1307560, 2249384 };
		inline constexpr REL::VariantID ForceResumeGame{ 668856, 2249385 };
		inline constexpr REL::VariantID RegisterHandler{ 827678, 2249387 };
		inline constexpr REL::VariantID Singleton{ 520890, 2689089, 4796375 };
		inline constexpr REL::VariantID UnregisterHandler{ 734727, 2249388 };
	}

	namespace MenuCursor
	{
		inline constexpr REL::VariantID Singleton{ 695696, 2696546, 4803837 };
		inline constexpr REL::VariantID CenterCursor{ 1107298, 2287473 };
		inline constexpr REL::VariantID RegisterCursor{ 1318193, 2287475 };
		inline constexpr REL::VariantID SetCursorConstraintsRaw{ 907092, 2287480 };
		inline constexpr REL::VariantID UnregisterCursor{ 1225249, 2287486 };
		inline constexpr REL::VariantID PipboyConstraintTLX{ 280077 };
		inline constexpr REL::VariantID PipboyConstraintTLY{ 938173 };
		inline constexpr REL::VariantID PipboyConstraintWidth{ 13663 };
		inline constexpr REL::VariantID PipboyConstraintHeight{ 672503 };
		inline constexpr REL::VariantID PipboyConstraintTLX_PowerArmor{ 1330184 };
		inline constexpr REL::VariantID PipboyConstraintTLY_PowerArmor{ 406096 };
		inline constexpr REL::VariantID PipboyConstraintWidth_PowerArmor{ 1064265 };
		inline constexpr REL::VariantID PipboyConstraintHeight_PowerArmor{ 140195 };
	}

	namespace MenuTopicManager
	{
		inline constexpr REL::VariantID Singleton{ 1391114, 2688800, 4796064 };
	}

	namespace MessageBoxMenu
	{
		inline constexpr REL::VariantID ShowMessage{ 442479, 2249469 };
	}

	namespace MessageMenuManager
	{
		inline constexpr REL::VariantID Singleton{ 959572, 2689087, 4796373 };
		inline constexpr REL::VariantID Create{ 89563, 2249456 };
	}

	namespace Moon
	{
		inline constexpr REL::VariantID Init{ 114988, 2208804 };
	}

	namespace NiAlphaProperty
	{
		inline constexpr REL::VariantID SetDestBlendMode{ 725249, 0 };  // Inlined in NG/AE
		inline constexpr REL::VariantID SetSrcBlendMode{ 976961, 0 };   // Inlined in NG/AE
		inline constexpr REL::VariantID SetTestMode{ 976961, 0 };       // Inlined in NG/AE
		inline constexpr REL::VariantID SetAlphaBlending{ 645586, 0 };  // Inlined in NG/AE
		inline constexpr REL::VariantID SetAlphaTesting{ 645586, 0 };   // Inlined in NG/AE
	}

	namespace NiCamera
	{
		inline constexpr REL::VariantID BoundInFrustum{ 781526, 2194525 };
		inline constexpr REL::VariantID WorldPtToScreenPt3{ 109441, 2270344 };
		inline constexpr REL::VariantID ViewPointToRay{ 460613, 2270338 };
	}

	namespace NiControllerManager
	{
		inline constexpr REL::VariantID GetNiControllerManager{ 1013515, 2271798 };
		inline constexpr REL::VariantID GetSequenceByName{ 846648, 2192808 };
	}

	namespace NiControllerSequence
	{
		inline constexpr REL::VariantID Activate{ 829033, 2271861 };
	}

	namespace NiMatrix3
	{
		inline constexpr REL::VariantID ToEulerAnglesXYZ{ 34114, 2269806 };
		inline constexpr REL::VariantID ToEulerAnglesXZY{ 1164218, 2269807 };
		inline constexpr REL::VariantID ToEulerAnglesYXZ{ 272989, 2269808 };
		inline constexpr REL::VariantID ToEulerAnglesYZX{ 963829, 2269809 };
		inline constexpr REL::VariantID ToEulerAnglesZYX{ 511385, 2269810 };
		inline constexpr REL::VariantID ToEulerAnglesZXY{ 606387, 2269824 };
		inline constexpr REL::VariantID FromEulerAnglesXYZ{ 124665, 2269813 };
		inline constexpr REL::VariantID FromEulerAnglesXZY{ 1319962, 2269814 };
		inline constexpr REL::VariantID FromEulerAnglesYXZ{ 55761, 2269815 };
		inline constexpr REL::VariantID FromEulerAnglesYZX{ 373803, 2269816 };
		inline constexpr REL::VariantID FromEulerAnglesZYX{ 1569564, 2269817 };
		inline constexpr REL::VariantID FromEulerAnglesZXY{ 326460, 2269825 };
	}

	namespace NiNode
	{
		inline constexpr REL::VariantID ChildrenVTable{ 390064 };
	}

	namespace NiPoint3
	{
		inline constexpr REL::VariantID GetZAngleFromVector{ 1450064, 2269788 };
	}

	namespace NiRefObject
	{
		inline constexpr REL::VariantID Objects{ 1161724, 2703471 };
	}

	namespace NiTexture
	{
		inline constexpr REL::VariantID SetAllowDegrade{ 948181, 2270148 };
	}

	namespace nsHUDNotifications
	{
		inline constexpr REL::VariantID IsQuestNotification{ 1291377, 2222465 };
		inline constexpr REL::VariantID IsObjectiveNotification{ 1025216, 2222466 };
		inline constexpr REL::VariantID IsLocationDiscoveredNotification{ 759615, 2222467 };
	}

	namespace nsHUDTypes
	{
		namespace NotificationInfo
		{
			inline constexpr REL::VariantID ctor{ 283986, 2223292 };
		}
	}

	namespace nsStatsMenuUtils
	{
		inline constexpr REL::VariantID GetEffectDisplayInfo{ 294691, 2224586 };
	}

	namespace PerkPointIncreaseEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 685859, 2697359, 4804734 };
	}

	namespace PerkUtilities
	{
		inline constexpr REL::VariantID RemoveGrenadeTrajectory{ 672186, 2233303 };
	}

	namespace PipboyDataManager
	{
		inline constexpr REL::VariantID Singleton{ 1178971, 2689086, 4796372 };
	}

	namespace PipboyDataGroup
	{
		inline constexpr REL::VariantID LockDataGroup{ 287528, 2225147 };
		inline constexpr REL::VariantID UnlockDataGroup{ 1165260, 2225148 };
	}

	namespace PipboyInventoryData
	{
		inline constexpr REL::VariantID AddItemCardInfoEntry{ 1191786, 2225267 };
		inline constexpr REL::VariantID BaseAddItemCardInfoEntry{ 1150364, 2225270 };
		inline constexpr REL::VariantID InitializeItem{ 482276, 2225264 };
		inline constexpr REL::VariantID PopulateItemCardInfo{ 54211, 2225266 };
		inline constexpr REL::VariantID QueueItemCardRepopulate{ 1034299, 2225311 };
		inline constexpr REL::VariantID RepopulateItemCardOnSection{ 892255, 2225279 };
	}

	namespace PipboyInventoryMenu
	{
		inline constexpr REL::VariantID UpdateData{ 762897, 2224143 };
		inline constexpr REL::VariantID SetQuickkey{ 539741, 2224159 };
	}

	namespace PipboyInventoryUtils
	{
		inline constexpr REL::VariantID DoSlotsOverlap{ 1035436, 2225230 };
		inline constexpr REL::VariantID FillDamageTypeInfo{ 928518, 2225234 };
		inline constexpr REL::VariantID FillResistTypeInfo{ 1578434, 2225235 };
	}

	namespace PipboyLightEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 1140080, 2696280, 4803571 };
	}

	namespace PipboyLogMenu
	{
		inline constexpr REL::VariantID UpdateData{ 672256, 2224052 };
	}

	namespace PipboyManager
	{
		inline constexpr REL::VariantID Singleton{ 553234, 2691945, 4799238 };
		inline constexpr REL::VariantID AddMenuToPipboy{ 394568, 2225453 };
		inline constexpr REL::VariantID ClosedownPipboy{ 731410, 2225480 };
		inline constexpr REL::VariantID EnablePipboyShader{ 662519, 2225484 };
		inline constexpr REL::VariantID InitPipboy{ 1477369, 2225479 };
		inline constexpr REL::VariantID LowerPipboy{ 1444875, 2225454 };
		inline constexpr REL::VariantID OnPipboyCloseAnim{ 1231000, 2225457 };
		inline constexpr REL::VariantID OnPipboyCloseAnimFailed{ 1362084, 2225459 };
		inline constexpr REL::VariantID OnPipboyClosed{ 592088, 2225458 };
		inline constexpr REL::VariantID OnPipboyOpenAnim{ 1500318, 2225448 };
		inline constexpr REL::VariantID OnPipboyOpenAnimFailed{ 702357, 2225449 };
		inline constexpr REL::VariantID OnPipboyOpened{ 1299608, 2225450 };
		inline constexpr REL::VariantID PlayItemAnimOnClose{ 377837, 2225461 };
		inline constexpr REL::VariantID PlayPipboyCloseAnim{ 273927, 2225456 };
		inline constexpr REL::VariantID PlayPipboyGenericOpenAnim{ 809076, 2225447 };
		inline constexpr REL::VariantID PlayPipboyLoadHolotapeAnim{ 477096, 2225446 };
		inline constexpr REL::VariantID PlayPipboyOpenAnim{ 663900, 2225444 };
		inline constexpr REL::VariantID PlayPipboyOpenTerminalAnim{ 743427, 2225445 };
		inline constexpr REL::VariantID ProcessLoweringReason{ 302903, 2225486 };
		inline constexpr REL::VariantID QPipboyActive{ 470886, 2225460 };
		inline constexpr REL::VariantID RaisePipboy{ 726763, 2225455 };
		inline constexpr REL::VariantID RefreshPipboyRenderSurface{ 81339, 2225478 };
		inline constexpr REL::VariantID StartAnimationGraphListening{ 714713, 2225490 };
		inline constexpr REL::VariantID StopAnimationGraphListening{ 621144, 2225491 };
		inline constexpr REL::VariantID UpdateCursorConstraint{ 900802, 2225488 };
	}

	namespace PipboyMapMenu
	{
		inline constexpr REL::VariantID UpdateData{ 92696, 2224074 };
	}

	namespace PipboyMenu
	{
		inline constexpr REL::VariantID RefreshMapMarkers{ 434114, 2224187 };
	}

	namespace PipboyObject
	{
		inline constexpr REL::VariantID AddMember{ 1499797, 2225699 };
	}

	namespace PipboyPerksMenu
	{
		inline constexpr REL::VariantID UpdateData{ 783380, 2224224 };
	}

	namespace PipboyPlayerInfoData
	{
		inline constexpr REL::VariantID UpdateCarryWeightData{ 338096, 2225788 };
	}

	namespace PipboyPlayerInfoMenu
	{
		inline constexpr REL::VariantID UpdateData{ 426990, 2224248 };
	}

	namespace PipboyPrimitiveValue
	{
		namespace uint32
		{
			inline constexpr REL::VariantID ctor{ 201797, 2225324 };
		}

		namespace boolean
		{
			inline constexpr REL::VariantID ctor{ 1158654, 2225327 };
		}
	}

	namespace PipboyQuestMenu
	{
		inline constexpr REL::VariantID UpdateData{ 1495929, 2224231 };
	}

	namespace PipboyRadioMenu
	{
		inline constexpr REL::VariantID UpdateData{ 713423, 2224242 };
	}

	namespace PipboySpecialMenu
	{
		inline constexpr REL::VariantID UpdateData{ 1426810, 2224256 };
	}

	namespace PipboyStatsMenu
	{
		inline constexpr REL::VariantID UpdateData{ 332518, 2224261 };
		inline constexpr REL::VariantID UpdateActiveEffects{ 236337, 2224262 };
	}

	namespace PipboySubMenu
	{
		inline constexpr REL::VariantID ProcessEvent{ 893703, 2224270 };
	}

	namespace PipboyValue
	{
		inline constexpr REL::VariantID ctor{ 531562, 2225915 };
	}

	namespace PipboyWorkshopMenu
	{
		inline constexpr REL::VariantID UpdateData{ 1370368, 2224273 };
	}

	namespace PlayerCamera
	{
		inline constexpr REL::VariantID Singleton{ 1171980, 2688801, 4796065 };
		inline constexpr REL::VariantID GetCameraPosition{ 1574794, 2248334 };
		inline constexpr REL::VariantID PopState{ 120998, 2248424 };
		inline constexpr REL::VariantID PushState{ 746523, 2248422 };
		inline constexpr REL::VariantID ToggleFreeCameraMode{ 224913, 2248368 };
		inline constexpr REL::VariantID SetState{ 858847, 2214742 };
		inline constexpr REL::VariantID StartFurnitureMode{ 10202, 2248373 };
		inline constexpr REL::VariantID StartPipboyMode{ 998069, 2248358 };
		inline constexpr REL::VariantID StopPipboyMode{ 811954, 2248359 };
		inline constexpr REL::VariantID QCameraEquals{ 839543, 2248421 };
	}

	namespace PlayerCharacter
	{
		namespace ScopedInventoryChangeMessageContext
		{
			inline constexpr REL::VariantID ctor{ 1512872, 2233292 };
			inline constexpr REL::VariantID dtor{ 542100, 2233293 };
		}

		inline constexpr REL::VariantID Singleton{ 303410, 2690919, 4798212 };
		inline constexpr REL::VariantID GetPlayerHandle{ 522947, 2698072 };
		inline constexpr REL::VariantID GetDifficultyLevel{ 922962, 2233056 };
		inline constexpr REL::VariantID IsGodMode{ 1032309, 2232986 };
		inline constexpr REL::VariantID IsHolotapePlaying{ 530826, 2233206 };
		inline constexpr REL::VariantID IsImmortal{ 500346, 2232988 };
		inline constexpr REL::VariantID IsPipboyLightOn{ 426550, 2233202 };
		inline constexpr REL::VariantID PauseHolotape{ 1567456, 2233208 };
		inline constexpr REL::VariantID PlayHolotape{ 1581042, 2233207 };
		inline constexpr REL::VariantID QueueFastTravel{ 556824, 2232918 };
		inline constexpr REL::VariantID RemoveLastUsedPowerArmor{ 1488486, 2233024 };
		inline constexpr REL::VariantID SelectPerk{ 1397326, 2232926 };
		inline constexpr REL::VariantID SetAIControlledPackage{ 1431972, 2233087 };
		inline constexpr REL::VariantID SetEscaping{ 25528, 2233142 };
		inline constexpr REL::VariantID SetVATSCriticalCount{ 327338, 2233219 };
		inline constexpr REL::VariantID ShowPipboyLight{ 1304102, 2233203 };
		inline constexpr REL::VariantID SetLastDialogueInput{ 696117, 2233190 };
		inline constexpr REL::VariantID ClearPrison{ 920390, 2233196 };
		inline constexpr REL::VariantID ReloadWeapon{ 458406, 2232907 };
		inline constexpr REL::VariantID SetPerkCount{ 616372, 2233187 };
		inline constexpr REL::VariantID HasLOSToTarget{ 449775, 2233004 };
		inline constexpr REL::VariantID TryUnlockObject{ 1341574, 2233040 };
		inline constexpr REL::VariantID EnableRadio{ 741937, 2233211 };
	}

	namespace PlayerControls
	{
		inline constexpr REL::VariantID Singleton{ 544871, 2692013, 4799306 };
		inline constexpr REL::VariantID DoAction{ 818081, 2234796 };
		inline constexpr REL::VariantID DoRegisterHandler{ 177801, 2234822 };
	}

	namespace PlayerCrosshairModeEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 1231665, 2694517, 4801808 };
	}

	namespace PowerArmor
	{
		inline constexpr REL::VariantID ActorInPowerArmor{ 1176757, 2219437 };
		inline constexpr REL::VariantID GetArmorKeyword{ 961172, 2194743 };
		inline constexpr REL::VariantID GetBatteryKeyword{ 1493537, 2194740 };
		inline constexpr REL::VariantID GetDefaultBatteryObject{ 1279247, 2194761 };
		inline constexpr REL::VariantID IsPowerArmorBattery{ 1012910, 2219422 };
		inline constexpr REL::VariantID SyncFurnitureVisualsToInventory{ 1078979, 2219453 };
		inline constexpr REL::VariantID fNewBatteryCapacity{ 1353498 };
	}

	namespace PowerArmorGeometry
	{
		inline constexpr REL::VariantID Singleton{ 1365745, 2694441, 4801732 };
		inline constexpr REL::VariantID HidePipboyPAGeometry{ 976332, 2248877 };
		inline constexpr REL::VariantID ShowPipboyPAGeometry{ 19066, 2248876 };
	}

	namespace PowerArmorLightData
	{
		inline constexpr REL::VariantID GetEventSource{ 120809, 2701547 };
	}

	namespace PowerArmorModMenu
	{
		inline constexpr REL::VariantID ShowBuildFailureMessage{ 1215152, 2224322 };
	}

	namespace PowerUtils
	{
		inline constexpr REL::VariantID ItemIsPowerConnection{ 17704, 2195078, 2195078 };
		inline constexpr REL::VariantID ItemIsPowerReceiver{ 210626, 2195060, 2195060 };
	}

	namespace ProcessLists
	{
		inline constexpr REL::VariantID Singleton{ 1569706, 2688869, 4796160 };
		inline constexpr REL::VariantID AreHostileActorsNear{ 1053584, 2234105 };
		inline constexpr REL::VariantID IsActorTargetingREFinPackage{ 559542, 2234033 };
		inline constexpr REL::VariantID RequestHighestDetectionLevelAgainstActor{ 1036693, 2234111 };
	}

	namespace RadioManager
	{
		inline constexpr REL::VariantID QPlayerRadioEnabled{ 811921, 2227674 };
		inline constexpr REL::VariantID QCurrentPlayerFreq{ 227903, 2227670 };
		inline constexpr REL::VariantID EnablePlayerRadio{ 653012, 2227673 };
	}

	namespace REFR_LOCK
	{
		inline constexpr REL::VariantID GetLevel{ 689812, 2191019 };
		inline constexpr REL::VariantID GetLockLevel{ 782953, 2191018 };
		inline constexpr REL::VariantID SetLocked{ 157617, 2191020 };
		inline constexpr REL::VariantID IsBroken{ 1034859, 2191021 };
		inline constexpr REL::VariantID NumericValueToEnum{ 1380232, 2191023 };
		inline constexpr REL::VariantID IsInaccessible{ 502938, 2191022 };
	}

	namespace SavefileMetadata
	{
		inline constexpr REL::VariantID FillDataFromFileName{ 116131, 2228156 };
	}

	namespace ScrapHeap
	{
		inline constexpr REL::VariantID Allocate{ 1085394, 2267983 };
		inline constexpr REL::VariantID Deallocate{ 923307, 2267984 };
	}

	namespace ScreenSplatter
	{
		inline constexpr REL::VariantID Clear{ 1478405, 2194783 };
		inline constexpr REL::VariantID Update{ 907893, 2194781 };
	}

	namespace SCRIPT_FUNCTION
	{
		inline constexpr REL::VariantID ConsoleFunctions{ 901511 }; // Same
		inline constexpr REL::VariantID ScriptFunctions{ 75173 }; // Same
		inline constexpr REL::VariantID CompileFunction{ 638606, 2204343 };
	}

	namespace Script
	{
		inline constexpr REL::VariantID ParseParameters{ 1607, 2204298 };
		inline constexpr REL::VariantID CompileAndRun{ 526625, 2204287 };
		inline constexpr REL::VariantID GetProcessScripts{ 44950, 2204310 };
		inline constexpr REL::VariantID SetProcessScripts{ 1188642, 2204309 };
	}

	namespace SendHUDMessage
	{
		inline constexpr REL::VariantID PopHUDMode{ 1495042, 2222444 };
		inline constexpr REL::VariantID PushHUDMode{ 1321764, 2222443 };
		inline constexpr REL::VariantID SetPowerArmorMode{ 361745, 2222459 };
		inline constexpr REL::VariantID ShowHUDMessage{ 1163005, 2222440 };
	}

	namespace Setting
	{
		namespace INISettingCollection
		{
			inline constexpr REL::VariantID Singleton{ 791183, 2704108 };
		}

		namespace INIPrefSettingCollection
		{
			inline constexpr REL::VariantID Singleton{ 767844, 2703234 };
		}

		namespace GameSettingCollection
		{
			inline constexpr REL::VariantID Singleton{ 8308, 2690301, 4797590 };
			inline constexpr REL::VariantID InitCollection{ 948832, 2188690 };
		}
	}

	namespace SitWaitMenu
	{
		inline constexpr REL::VariantID OnEnterFurniture{ 562238, 2249503 };
		inline constexpr REL::VariantID OnExitFurniture{ 454795, 2249504 };
	}

	namespace Sky
	{
		inline constexpr REL::VariantID Singleton{ 484694, 2192448 };
		inline constexpr REL::VariantID ForceWeather{ 698558, 2208861 };
		inline constexpr REL::VariantID ResetWeather{ 6511, 2208860 };
	}

	namespace SplineUtils
	{
		inline constexpr REL::VariantID DisconnectSpline{ 750682, 2195074 };
		inline constexpr REL::VariantID ConnectSpline{ 59311, 2195073 };
	}

	namespace StartMenuBase
	{
		inline constexpr REL::VariantID GetMenuDifficultyLevel{ 1089336, 2224540 };
	}

	namespace ILStringMap
	{
		inline constexpr REL::VariantID Map{ 1497866, 2661471 };
	}

	namespace SubtitleManager
	{
		inline constexpr REL::VariantID Singleton{ 740511, 2689088, 4796374 };
		inline constexpr REL::VariantID RWLock{ 589656, 2701704 };
		inline constexpr REL::VariantID ShowSubtitle{ 875508, 2249542 };
		inline constexpr REL::VariantID UpdateSubtitles{ 381778, 2249545 };
		inline constexpr REL::VariantID DisplayNextSubtitle{ 102120, 2249551 };
	}

	namespace SWFToCodeFunctionHandler
	{
		inline constexpr REL::VariantID MapCodeMethodToASFunction{ 1263128, 2287419 };
		inline constexpr REL::VariantID RegisterCodeObject{ 67637, 2287415 };
	}

	namespace TaskQueueInterface
	{
		inline constexpr REL::VariantID Singleton{ 7491, 2698331 };
		inline constexpr REL::VariantID QueueClosePipboy{ 69886, 2229289 };
		inline constexpr REL::VariantID QueueRebuildBendableSpline{ 198419, 2229302 };
		inline constexpr REL::VariantID QueueShow1stPerson{ 994377, 2229268 };
		inline constexpr REL::VariantID QueueShowPipboy{ 1133684, 2229288 };
		inline constexpr REL::VariantID QueueUpdate3D{ 581890, 2229234 };
		inline constexpr REL::VariantID QueueWeaponFire{ 15449, 2229186 };
		inline constexpr REL::VariantID TaskUnpackFunc{ 1546751, 2229323 };
	}

	namespace TerminalHacked
	{
		inline constexpr REL::VariantID GetEventSource{ 425579, 2233751 };
	}

	namespace TES
	{
		inline constexpr REL::VariantID Singleton{ 1194835, 2698044 };
	}

	namespace TESActivateEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 166230, 2201819 };
	}

	namespace TESActorBaseData
	{
		inline constexpr REL::VariantID GetLevel{ 151866, 2192891 };
	}

	namespace TESAmmo
	{
		inline constexpr REL::VariantID GetReloadsWithAmmoRef{ 1035622, 2197864 };
	}

	namespace TESAudio
	{
		namespace ScriptedMusicState
		{
			inline constexpr REL::VariantID Singleton{ 1216761, 2691985, 4799278 };
			inline constexpr REL::VariantID Remove{ 544747, 2226976 };
		}
	}

	namespace TESBoundObject
	{
		inline constexpr REL::VariantID ApplyMods{ 113585, 2198325 };
	}

	namespace TESCellAttachDetachEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 311270, 2201823 };
	}

	namespace TESCellFullyLoadedEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 1534478, 2201824 };
	}

	namespace TESCondition
	{
		inline constexpr REL::VariantID IsTrue{ 1275731, 2211989 };
		inline constexpr REL::VariantID IsTrueForAllButFunction{ 1182457, 0 };  // Inlined in NG/AE
		inline constexpr REL::VariantID ClearAllConditionItems{ 757713, 2211988 };
	}

	namespace TESConditionItem
	{
		inline constexpr REL::VariantID GetComparisonValue{ 1373349, 2212007 };
		inline constexpr REL::VariantID IsTrue1{ 1453240, 2212008 };
		inline constexpr REL::VariantID IsTrue2{ 43452, 2212009 };
	}

	namespace TESContainerChangedEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 242538, 2201832 };
	}

	namespace TESDataHandler
	{
		inline constexpr REL::VariantID Singleton{ 711558, 2688883, 4796135 };
		inline constexpr REL::VariantID AddFormToDataHandler{ 350112, 2192271 };
		inline constexpr REL::VariantID CheckModsLoaded{ 1432894, 2192323 };
		inline constexpr REL::VariantID CreateReferenceAtLocation{ 500304, 2192301 };
		inline constexpr REL::VariantID IsFormIDInuse{ 1448838, 2192351 };
	}

	namespace TESDeathEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 1465690, 2201833 };
	}

	namespace TESDescription
	{
		inline constexpr REL::VariantID GetDescription{ 523613, 2193019 };
	}

	namespace TESEnterSneakingEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 28138, 2201837 };
	}

	namespace TESFile
	{
		inline constexpr REL::VariantID CloseTES{ 766555, 2192491 };
		inline constexpr REL::VariantID GetChunkData{ 188910, 2192538 };
		inline constexpr REL::VariantID GetTESChunk{ 641361, 2192536 };
		inline constexpr REL::VariantID NextChunk{ 1252764, 2192537 };
		inline constexpr REL::VariantID NextForm{ 644374, 2192527 };
		inline constexpr REL::VariantID NextGroup{ 339855, 2192522 };
		inline constexpr REL::VariantID OpenTES{ 728465, 2192490 };
	}

	namespace TESForm
	{
		inline constexpr REL::VariantID FormSortFunc{ 705530, 0 }; // Inlined in NG/AE
		inline constexpr REL::VariantID AddCompileIndex{ 1315637, 2193152 };
		inline constexpr REL::VariantID AllForms{ 422985, 2689178, 4796465 };
		inline constexpr REL::VariantID AllFormsMapLock{ 691815, 2689189, 4796476 };
		inline constexpr REL::VariantID AllFormsByEditorID{ 642758, 2689179, 4796466 };
		inline constexpr REL::VariantID AllFormsEditorIDMapLock{ 910917, 2689190, 4796477 };
		inline constexpr REL::VariantID GetFile{ 1376557, 2193103 };
		inline constexpr REL::VariantID GetFormByNumericID{ 796114, 2193092 };
		inline constexpr REL::VariantID GetFormEnumString{ 1309967 }; // Same
		inline constexpr REL::VariantID GetFormTypeFromString{ 565203, 2193108 };
		inline constexpr REL::VariantID SetTemporary{ 482454, 2193125 };
	}

	namespace TESFormDeleteEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 1396562, 2201842 };
	}

	namespace TESFullName
	{
		inline constexpr REL::VariantID SparseFullNameMap{ 226372, 2661402 };
	}

	namespace TESFurniture
	{
		inline constexpr REL::VariantID GetContainer{ 1049933, 2198043 };
	}

	namespace TESFurnitureEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 678665, 2201844 };
	}

	namespace TESHarvestEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 11738, 2193351 };
	}

	namespace TESHealthForm
	{
		inline constexpr REL::VariantID GetFormHealth{ 1515099, 2193227 };
	}

	namespace TESHitEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 1411899, 2201886 };
	}

	namespace TESInitScriptEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 1542080, 2201846 };
	}

	namespace TESLeveledList
	{
		inline constexpr REL::VariantID AddLeveledObject{ 1163308, 2193258 };
		inline constexpr REL::VariantID AddScriptAddedLeveledObject{ 860553, 2193269 };
		inline constexpr REL::VariantID CalculateCurrentFormList{ 603811, 2193259 };
		inline constexpr REL::VariantID CalculateCurrentFormListForRef{ 507405, 2193260 };
		inline constexpr REL::VariantID GetUseAll{ 233875, 2193253 };
	}

	namespace TESLoadGameEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 823570, 2201848 };
	}

	namespace TESLocationClearedEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 1372013, 2201849 };
	}

	namespace TESMagicEffectApplyEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 1327824, 2201851 };
	}

	namespace TESNPC
	{
		inline constexpr REL::VariantID AlternateHeadPartListMap{ 1306546, 2662368 };
		inline constexpr REL::VariantID GetSex{ 1257181, 2207107 };
		inline constexpr REL::VariantID GetFacialBoneMorphIntensity{ 272217, 2207416 };
		inline constexpr REL::VariantID SetHairColor{ 1201742, 2207426 };
		inline constexpr REL::VariantID GetShortName{ 1221705, 2207405 };
		inline constexpr REL::VariantID GetXPValue{ 1134136, 2207384 };
	}

	namespace TESObjectARMO
	{
		inline constexpr REL::VariantID Protects{ 1028523, 2198535 };
	}

	namespace TESObjectCELL
	{
		inline constexpr REL::VariantID GetbhkWorld{ 1326073, 2200260 };
		inline constexpr REL::VariantID GetCantWaitHere{ 376940, 2200287 };
		inline constexpr REL::VariantID GetDataX{ 445210, 2200213 };
		inline constexpr REL::VariantID GetDataY{ 1322816, 2200214 };
		inline constexpr REL::VariantID GetEncounterZone{ 1414637, 2200242 };
		inline constexpr REL::VariantID GetLocation{ 868663, 2200179 };
		inline constexpr REL::VariantID GetOwner{ 910422, 2200236 };
		inline constexpr REL::VariantID GetRegionList{ 1565031, 2200265 };
		inline constexpr REL::VariantID Pick{ 434717, 2200263 };
		inline constexpr REL::VariantID SetCullCellMarkers{ 192259, 2192259 };
		inline constexpr REL::VariantID UpdateAllRefsLoaded{ 374552, 2200415 };
		inline constexpr REL::VariantID AttatchReference3D{ 125567, 2200620 };
		inline constexpr REL::VariantID RemoveReference{ 650855, 2200299 };
		inline constexpr REL::VariantID UpdateAllDecals{ 283071, 2200643 };
		inline constexpr REL::VariantID DefaultWater{ 289864, 2691845, 4799138 };
	}

	namespace TESObjectCONT
	{
		inline constexpr REL::VariantID GetActivateText{ 917378, 2198653 };
	}

	namespace TESObjectDOOR
	{
		inline constexpr REL::VariantID GetActivateText{ 978619, 2198690 };
		inline constexpr REL::VariantID CalculateDoFavor{ 1428007, 2198692 };
	}

	namespace TESObjectLoadedEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 609604, 2201853 };
	}

	namespace TESObjectREFR
	{
		inline constexpr REL::VariantID ActivateRef{ 753531, 2201147 };
		inline constexpr REL::VariantID AddInventoryItem{ 78185, 2200949 };
		inline constexpr REL::VariantID AddKeyword{ 640674, 2200860 };
		inline constexpr REL::VariantID AddLockChange{ 1578706, 2200731 };
		inline constexpr REL::VariantID ApplyArtObject{ 357908, 2205200 };
		inline constexpr REL::VariantID ApplyEffectShader{ 652173, 2205201 };
		inline constexpr REL::VariantID Enable{ 1396707, 2201150 };
		inline constexpr REL::VariantID FindReferenceFor3D{ 766937, 2201082 };
		inline constexpr REL::VariantID GetCurrentLocation{ 1135470, 2201163 };
		inline constexpr REL::VariantID GetDisplayFullName{ 1212056, 2201126 };
		inline constexpr REL::VariantID GetDistanceFromPoint{ 1332593, 2201172 };
		inline constexpr REL::VariantID GetDistanceFromReference{ 268887, 2201174 };
		inline constexpr REL::VariantID GetDistanceSqFromPoint{ 361891, 2201173 };
		inline constexpr REL::VariantID GetDistanceSqFromReference{ 1225962, 2201175 };
		inline constexpr REL::VariantID GetEditorDead{ 941388, 2200781 };
		inline constexpr REL::VariantID GetOwner{ 1323437, 2202616 };
		inline constexpr REL::VariantID GetGoldAmount{ 564356, 2200923 };
		inline constexpr REL::VariantID GetHandle{ 1573130, 2201196 };
		inline constexpr REL::VariantID GetHasOwner{ 1016277, 2202622 };
		inline constexpr REL::VariantID GetInventoryObjectCount{ 333415, 2200939 };
		inline constexpr REL::VariantID GetLinkedRef{ 897287, 2202683 };
		inline constexpr REL::VariantID GetLock{ 930785, 2202648 };
		inline constexpr REL::VariantID GetWeightInContainer{ 1377567, 2201001 };
		inline constexpr REL::VariantID HasContainer{ 1213017, 2201022 };
		inline constexpr REL::VariantID IsAnOwner{ 933798, 2202635 };
		inline constexpr REL::VariantID IsCrimeToActivate{ 836011, 2201180 };
		inline constexpr REL::VariantID IsLeveledCreature{ 1341859, 2202655 };
		inline constexpr REL::VariantID MarkAsDeleted{ 192661, 2201156 };
		inline constexpr REL::VariantID MoveRefToNewSpace{ 629658, 2201149 };
		inline constexpr REL::VariantID RemoveKeyword{ 281170, 2200861 };
		inline constexpr REL::VariantID SetAngleOnReference{ 1049748, 2201134 };
		inline constexpr REL::VariantID SetLinkedRef{ 192840, 2202684 };
		inline constexpr REL::VariantID SetLocationOnReference{ 1101832, 2201138 };
		inline constexpr REL::VariantID SetWantsDelete{ 761346, 2201199 };
		inline constexpr REL::VariantID RebuildBendableSpline{ 340319, 2201119 };
		inline constexpr REL::VariantID GetItemCount{ 635042, 2200996 };
		inline constexpr REL::VariantID UpdateReference3D{ 1568075, 2201071 };
		inline constexpr REL::VariantID GetObjectCenter{ 777738, 2201092 };
		inline constexpr REL::VariantID SetScale{ 817930, 2200893 };
		inline constexpr REL::VariantID IsMarker{ 364958, 2201157 };
		inline constexpr REL::VariantID IsDecalRef{ 686485, 2200794 };
		inline constexpr REL::VariantID UpdateDynamicNavmesh{ 1518094, 2201206 };
		inline constexpr REL::VariantID GetMapMarkerData{ 253640, 2202644 };
		inline constexpr REL::VariantID GetRelevantWaterHeight{ 6866, 2201189 };
		inline constexpr REL::VariantID CanBeMoved{ 683719, 2200898 };
	}

	namespace TESObjectWEAP
	{
		inline constexpr REL::VariantID GetMeleeAttackSpeed{ 817670, 2198957 };
		inline constexpr REL::VariantID GetMeleeAttackSpeedLabel{ 178784, 2198959 };
		inline constexpr REL::VariantID Fire{ 1056037, 2198960 };
	}

	namespace TESQuest
	{
		inline constexpr REL::VariantID SetStage{ 952799, 2207743 };
		inline constexpr REL::VariantID GetAliasedRef{ 847223, 2207810 };
	}

	namespace TESResponse
	{
		inline constexpr REL::VariantID GetResponseText{ 992447, 2208288 };
		inline constexpr REL::VariantID GetSpeakerIdle{ 606043, 2208293 };
	}

	namespace TESSpellList
	{
		inline constexpr REL::VariantID AddSpell{ 1312083, 2193395 };
	}

	namespace TESSwitchRaceCompleteEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 976262, 2201874 };
	}

	namespace TESTopic
	{
		inline constexpr REL::VariantID InitDialogueItem{ 49777, 2208360 };
	}

	namespace TESTopicInfo
	{
		inline constexpr REL::VariantID GetParentInfoGroup{ 163538, 2208435 };
		inline constexpr REL::VariantID GetChallengeLevel{ 613016, 2208441 };
		inline constexpr REL::VariantID GetSuccessLevel{ 1023955, 2208444 };
		inline constexpr REL::VariantID StartSceneOnEnd{ 376042, 2208466 };
		inline constexpr REL::VariantID GetScene{ 820897, 2208453 };
		inline constexpr REL::VariantID StartScene{ 269139, 2208469 };
		inline constexpr REL::VariantID GetSpeaker{ 1048628, 2208449 };
	}

	namespace TESValueForm
	{
		inline constexpr REL::VariantID GetFormValue{ 885783, 2193435 };
	}

	namespace TESWaterDisplacement
	{
		inline constexpr REL::VariantID SetStencilBit{ 705016, 2213923 };
	}

	namespace TESWeightForm
	{
		inline constexpr REL::VariantID GetFormWeight{ 1321341, 2193446 };
	}

	namespace TESWorldSpace
	{
		inline constexpr REL::VariantID DefaultWater{ 289864, 2691845, 4799138 };
		inline constexpr REL::VariantID GetSkyCell{ 903020, 2202924 };
	}

	namespace UI
	{
		inline constexpr REL::VariantID Singleton{ 548587, 2689028, 4796314 };
		inline constexpr REL::VariantID CustomRendererHasQuads{ 1401451, 2284757 };
		inline constexpr REL::VariantID GetMenuMapRWLock{ 578487, 2707105 };
		inline constexpr REL::VariantID RefreshCursor{ 1436639, 2284772 };
		inline constexpr REL::VariantID RegisterMenu{ 1519575, 2284766 };
		inline constexpr REL::VariantID UpdateControllerType{ 175796, 2284768 };
	}

	namespace UIAdvanceMenusFunctionCompleteEvent
	{
		inline constexpr REL::VariantID GetEventSource{ 1067039, 2694428, 4801719 };
	}

	namespace UIMessageQueue
	{
		inline constexpr REL::VariantID Singleton{ 82123, 2689091, 4796377 };
		inline constexpr REL::VariantID AddMessage{ 1182019, 2284929 };
	}

	namespace UIUtils
	{
		inline constexpr REL::VariantID GetComparisonItems{ 593818, 0 };  // Inlined in NG/AE
		inline constexpr REL::VariantID PlayPipboySound{ 1320952, 2249706 };
		inline constexpr REL::VariantID PlayMenuSound{ 1227993, 2249707 };
		inline constexpr REL::VariantID UpdateGamepadDependentButtonCodes{ 190238, 2249714, 4483350 };
		inline constexpr REL::VariantID ShowCraftingMenu{ 877836, 2249718 };
		inline constexpr REL::VariantID HasRequiredInventoryForCraftingMenu{ 639634, 2249717 };
	}

	namespace VATS
	{
		inline constexpr REL::VariantID Singleton{ 570121, 2690444, 4797733 };
		inline constexpr REL::VariantID CanActivateCriticals{ 1057603, 2237208 };
	}

	namespace WorkbenchMenuBase
	{
		inline constexpr REL::VariantID UpdateOptimizedAutoBuildInv{ 769581, 2224955 };
		inline constexpr REL::VariantID QCurrentModChoiceData{ 583103, 2224958 };
		inline constexpr REL::VariantID ShowBuildFailureMessage{ 316889, 2224959 };
	}

	namespace Workshop
	{
		namespace PlacementItemData
		{
			inline constexpr REL::VariantID Set{ 1460935, 2195643 };
		}

		namespace WorkshopMenuNode
		{
			inline constexpr REL::VariantID FindAndSetSelectedNode{ 1309368, 2195022 };
		}

		inline constexpr REL::VariantID FindNearestValidWorkshop{ 905705, 2194970 };
		inline constexpr REL::VariantID FreeBuild{ 1583365, 2194924 };
		inline constexpr REL::VariantID GetSelectedWorkshopMenuNode{ 763948, 2195024 };
		inline constexpr REL::VariantID InitializePlacementReference{ 1577199, 0 }; // Inlined in NG/AE
		inline constexpr REL::VariantID IsLocationWithinBuildableArea{ 990965, 2194956 };
		inline constexpr REL::VariantID PlaceCurrentReference{ 1058211, 2195155 };
		inline constexpr REL::VariantID RegisterForItemDestroyed{ 1367004, 2194930 };
		inline constexpr REL::VariantID RegisterForItemMoved{ 835323, 2194932 };
		inline constexpr REL::VariantID RegisterForItemPlaced{ 849008, 2194926 };
		inline constexpr REL::VariantID RegisterForWorkshopModeEvent{ 275798, 2194946 };
		inline constexpr REL::VariantID RequestExitWorkshop{ 209891, 2195135 };
		inline constexpr REL::VariantID ScrapReference{ 636327, 2195125 };
		inline constexpr REL::VariantID SetSelectedEditItem{ 2562, 2195119 };
		inline constexpr REL::VariantID StartWorkshop{ 171771, 2195134 };
		inline constexpr REL::VariantID ToggleEditMode{ 1243386, 2195130 };
		inline constexpr REL::VariantID UnregisterForItemDestroyed{ 1101379, 2194931 };
		inline constexpr REL::VariantID UnregisterForItemMoved{ 569432, 2194933 };
		inline constexpr REL::VariantID UnregisterForItemPlaced{ 583255, 2194927 };
		inline constexpr REL::VariantID UnregisterForWorkshopModeEvent{ 9236, 2194947 };
		inline constexpr REL::VariantID UpdateActiveEdit{ 69261, 2195210 };
		inline constexpr REL::VariantID WorkshopCanShowRecipe{ 239190, 2194978 }; // old ID wrong: 2195115
		inline constexpr REL::VariantID CurrentPlacementItemData{ 1279207, 2689973, 4797261 };
		inline constexpr REL::VariantID CurrentRow{ 833923, 2689868, 4797160 };
		inline constexpr REL::VariantID PlacementItem{ 526727, 2689906, 4797235 };
	}

	namespace WorkshopMenu
	{
		inline constexpr REL::VariantID CheckAndSetItemForPlacement{ 1541862, 2224991 };
		inline constexpr REL::VariantID UpdateButtonText{ 1089189, 2224992 };
	}

	inline constexpr REL::VariantID NiFree{ 242362, 2268539 };
	inline constexpr REL::VariantID NiMalloc{ 974443, 2268535 };
	inline constexpr REL::VariantID RTDynamicCast{ 84112, 2725673, 4818455 };
	inline constexpr REL::VariantID IsPlayerInDialogue{ 60126, 2188686 };
}

namespace Scaleform::ID
{
	namespace GFx::ASStringManager
	{
		inline constexpr REL::VariantID CreateStringNode{ 419974, 2290014 };
	}

	namespace GFx::Loader
	{
		inline constexpr REL::VariantID CreateMovie{ 912291, 2284723 };
	}

	namespace GFx::Movie
	{
		inline constexpr REL::VariantID Release{ 404814, 2287230 };
	}

	namespace GFx::Value
	{
		inline constexpr REL::VariantID ObjectAddRef{ 244786, 2286228 };
		inline constexpr REL::VariantID ObjectRelease{ 856221, 2286229 };
		inline constexpr REL::VariantID HasMember{ 788691, 2286078 };
		inline constexpr REL::VariantID GetArraySize{ 254218, 2285791 };
		inline constexpr REL::VariantID GetMember{ 1517430, 2285936, 4494126 };
		inline constexpr REL::VariantID GetElement{ 827659, 2285881 };
		inline constexpr REL::VariantID SetElement{ 433707, 2286575 };
		inline constexpr REL::VariantID SetMember{ 1360149, 2286589 };
		inline constexpr REL::VariantID Invoke{ 655847, 2286101 };
		inline constexpr REL::VariantID PushBack{ 1330475, 2286424 };
		inline constexpr REL::VariantID RemoveElements{ 1286586, 2286475 };
		inline constexpr REL::VariantID VisitMembers{ 1276961, 2286786 };
		inline constexpr REL::VariantID GetDisplayInfo{ 498814, 2285873 };
	}

	namespace Memory
	{
		inline constexpr REL::VariantID SetGlobalHeap{ 939898, 2707353 };
		inline constexpr REL::VariantID GetGlobalHeap{ 939898, 2707353 };
	}

	namespace SysAlloc
	{
		inline constexpr REL::VariantID InitHeapEngine{ 1047970, 2284532, 4493927 };
		inline constexpr REL::VariantID ShutdownHeapEngine{ 1207169, 2284536 };
	}
}
