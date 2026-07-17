#include "Settings.h"
#include "Hooks.h"
#include "OpenAnimationReplacer.h"
#include "ActiveClip.h"
#include "AnimationCache.h"
#include "AnimationLog.h"
#include "Functions.h"
#include "Parsing.h"
#include "UI/UIManager.h"
#include "Offsets.h"
#include "FormRegistry.h"

namespace Plugin
{
	static constexpr auto NAME    = "OpenAnimationReplacer"sv;
	static constexpr auto VERSION = REL::Version{ 1, 0, 0 };
}

namespace StructProbe
{
	template<typename T>
	struct HkArrayAccessor {
		T* data;
		int32_t size;
		int32_t capacityAndFlags;
	};

	template<typename T>
	static const HkArrayAccessor<T>& AsAccessor(const RE::hkArray<T, RE::hkContainerHeapAllocator>& arr) {
		return reinterpret_cast<const HkArrayAccessor<T>&>(arr);
	}

	static const char* SafeStr(const RE::hkStringPtr& s) {
		return s.data() ? s.data() : "(null)";
	}

	static void Log(const std::string& msg) { logger::info("{}", msg); }
	static void LogW(const std::string& msg) { logger::warn("{}", msg); }
	static void LogE(const std::string& msg) { logger::error("{}", msg); }

	static void LogStringData(RE::hkbCharacterStringData* sd)
	{
		auto S = [](const char* label, const char* val) {
			return fmt::format("[OAR-Probe]   {} = '{}'", label, val);
		};
		auto I = [](const char* label, int32_t val) {
			return fmt::format("[OAR-Probe]   {} = {}", label, val);
		};

		Log(S("name", SafeStr(sd->name)));
		Log(S("rigName", SafeStr(sd->rigName)));
		Log(S("ragdollName", SafeStr(sd->ragdollName)));
		Log(S("behaviorFilename", SafeStr(sd->behaviorFilename)));

		auto& defSkins = AsAccessor(sd->deformableSkinNames);
		auto& rigSkins = AsAccessor(sd->rigidSkinNames);
		auto& animNames = AsAccessor(sd->animationNames);
		auto& bundleData = AsAccessor(sd->animationBundleNameData);
		auto& bundleFile = AsAccessor(sd->animationBundleFilenameData);
		auto& charProps = AsAccessor(sd->characterPropertyNames);

		Log(I("deformableSkinNames.size", defSkins.size));
		Log(I("rigidSkinNames.size", rigSkins.size));
		Log(I("animationNames.size", animNames.size));
		Log(I("animationBundleNameData.size", bundleData.size));
		Log(I("animationBundleFilenameData.size", bundleFile.size));
		Log(I("characterPropertyNames.size", charProps.size));

		if (animNames.size > 0 && animNames.data) {
			auto& f = animNames.data[0];
			Log(S("animNames[0].fileName", SafeStr(f.fileName)));
			Log(S("animNames[0].meshName", SafeStr(f.meshName)));
		}
		if (bundleData.size > 0 && bundleData.data) {
			auto& f = bundleData.data[0];
			Log(S("bundleNameData[0].fileName", SafeStr(f.fileName)));
			Log(S("bundleNameData[0].meshName", SafeStr(f.meshName)));
		}
	}

	static void RawScanStringData(RE::hkbCharacterStringData* sd)
	{
		auto* rawBytes = reinterpret_cast<uint8_t*>(sd);
		logger::info("[OAR-Probe] === Raw string scan for layout verification ===");
		for (int off = 0x10; off < 0x120; off += 8) {
			auto* ptr = *reinterpret_cast<const char**>(rawBytes + off);
			if (ptr && !IsBadReadPtr(ptr, 1)) {
				if (ptr[0] >= 0x20 && ptr[0] < 0x7F) {
					char buf[64] = {};
					strncpy_s(buf, ptr, 60);
					auto msg = fmt::format("[OAR-Probe]   +0x{:02X}: '{}'", off, buf);
					logger::info("{}", msg);
				}
			}
		}
		logger::info("[OAR-Probe] === End raw scan ===");
	}

	RE::hkbCharacterStringData* FindStringDataByVtable(void* baseObj, int scanStart, int scanEnd)
	{
		uintptr_t expectedVtbl = Offsets::hkbCharacterStringData_vtbl.address();
		auto* bytes = reinterpret_cast<uint8_t*>(baseObj);
		for (int off = scanStart; off < scanEnd; off += 8) {
			auto* candidate = *reinterpret_cast<void**>(bytes + off);
			if (!candidate || IsBadReadPtr(candidate, 8)) continue;
			auto vtbl = *reinterpret_cast<uintptr_t*>(candidate);
			if (vtbl == expectedVtbl) {
				Log(fmt::format("[OAR-Probe] Found hkbCharacterStringData at base+0x{:X}", off));
				return reinterpret_cast<RE::hkbCharacterStringData*>(candidate);
			}
		}
		return nullptr;
	}

	void VerifyHavokLayouts()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			LogW("[OAR-Probe] No player singleton");
			return;
		}

		RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
		if (!player->GetAnimationGraphManagerImpl(manager) || !manager) {
			LogW("[OAR-Probe] No animation graph manager");
			return;
		}

		if (manager->graph.empty()) {
			LogW("[OAR-Probe] No graphs in manager");
			return;
		}

		auto& graph = manager->graph[0];
		auto* character = &graph->character;
		Log(fmt::format("[OAR-Probe] hkbCharacter at {:X}, name='{}'",
			reinterpret_cast<uintptr_t>(character), SafeStr(character->name)));

		auto* setup = character->setup._ptr;
		if (!setup) {
			LogW("[OAR-Probe] hkbCharacterSetup is null");
			return;
		}
		Log(fmt::format("[OAR-Probe] hkbCharacterSetup at {:X}",
			reinterpret_cast<uintptr_t>(setup)));

		auto* typedSetup = reinterpret_cast<RE::hkbCharacterSetup*>(setup);
		RE::hkbCharacterStringData* stringData = nullptr;

		if (typedSetup->data._ptr) {
			Log(fmt::format("[OAR-Probe] hkbCharacterData at {:X}",
				reinterpret_cast<uintptr_t>(typedSetup->data._ptr)));

			auto* charData = typedSetup->data._ptr;
			if (charData->stringData._ptr) {
				uintptr_t vtbl = *reinterpret_cast<uintptr_t*>(charData->stringData._ptr);
				if (vtbl == Offsets::hkbCharacterStringData_vtbl.address()) {
					stringData = charData->stringData._ptr;
					Log(fmt::format("[OAR-Probe] stringData found via typed path at {:X}",
						reinterpret_cast<uintptr_t>(stringData)));
				} else {
					LogW(fmt::format("[OAR-Probe] typed stringData vtbl mismatch: {:X} vs {:X}",
						vtbl, Offsets::hkbCharacterStringData_vtbl.address()));
				}
			}

			if (!stringData) {
				Log("[OAR-Probe] Scanning hkbCharacterData for stringData by vtable...");
				stringData = FindStringDataByVtable(charData, 0x10, 0x100);
			}
		}

		if (!stringData) {
			Log("[OAR-Probe] Scanning hkbCharacterSetup for nested stringData...");
			auto* setupBytes = reinterpret_cast<uint8_t*>(setup);
			for (int i = 0x10; i < 0x80; i += 8) {
				auto* nested = *reinterpret_cast<void**>(setupBytes + i);
				if (!nested || IsBadReadPtr(nested, 8)) continue;
				stringData = FindStringDataByVtable(nested, 0x10, 0x80);
				if (stringData) {
					Log(fmt::format("[OAR-Probe] Found via setup+0x{:X}->nested", i));
					break;
				}
			}
		}

		if (!stringData) {
			LogE("[OAR-Probe] Could not find hkbCharacterStringData");
			return;
		}

		Log(fmt::format("[OAR-Probe] hkbCharacterStringData at {:X}",
			reinterpret_cast<uintptr_t>(stringData)));
		LogStringData(stringData);
		RawScanStringData(stringData);

		Log(fmt::format("[OAR-Probe] hkbClipGenerator vtable at {:X}",
			Offsets::hkbClipGenerator_vtbl.address()));
		Log(fmt::format("[OAR-Probe] hkbCharacterStringData vtable at {:X}",
			Offsets::hkbCharacterStringData_vtbl.address()));
	}

	void ScanForLoadClipsCallSite()
	{
		Log("[OAR-Probe] === Scanning for loadClips call site ===");

		auto moduleBase = REL::Module::get().base();
		auto textSeg = REL::Module::get().segment(REL::Segment::Name::text);
		auto moduleEnd = textSeg.address() + textSeg.size();
		auto moduleSize = moduleEnd - moduleBase;
		Log(fmt::format("[OAR-Probe] Module base={:X}, text end={:X}", moduleBase, moduleEnd));

		auto bsAnimGraphVtbl = Offsets::ptr_VisitGraph.address();
		auto bindingSetVtbl = REL::Relocation<uintptr_t>{ REL::ID(802975) }.address();
		auto stringDataVtbl = Offsets::hkbCharacterStringData_vtbl.address();
		Log(fmt::format("[OAR-Probe] hkbAnimationBindingSet vtable at {:X}", bindingSetVtbl));

		auto bsGraphVtblBase = REL::Relocation<uintptr_t>{ REL::ID(742655) }.address();
		Log(fmt::format("[OAR-Probe] BShkbAnimationGraph vtable base at {:X}", bsGraphVtblBase));

		Log("[OAR-Probe] BShkbAnimationGraph vtable entries:");
		for (int i = 0; i < 40; i++) {
			auto* vtblEntry = reinterpret_cast<uintptr_t*>(bsGraphVtblBase + i * 8);
			if (IsBadReadPtr(vtblEntry, 8)) break;
			auto funcAddr = *vtblEntry;
			if (funcAddr < moduleBase || funcAddr >= moduleBase + moduleSize) continue;
			Log(fmt::format("[OAR-Probe]   vtbl[{:2}] = {:X} (rva {:X})", i, funcAddr, funcAddr - moduleBase));
		}

		Log("[OAR-Probe] Scanning vtable functions for CALL instructions...");
		for (int i = 0; i < 40; i++) {
			auto* vtblEntry = reinterpret_cast<uintptr_t*>(bsGraphVtblBase + i * 8);
			if (IsBadReadPtr(vtblEntry, 8)) break;
			auto funcAddr = *vtblEntry;
			if (funcAddr < moduleBase || funcAddr >= moduleBase + moduleSize) continue;

			auto* funcBytes = reinterpret_cast<uint8_t*>(funcAddr);
			int callCount = 0;

			for (int off = 0; off < 1024; off++) {
				if (IsBadReadPtr(funcBytes + off, 5)) break;
				if (funcBytes[off] == 0xC3 && off > 10) break;
				if (funcBytes[off] == 0xCC && off > 10) break;

				if (funcBytes[off] == 0xE8) {
					int32_t rel = *reinterpret_cast<int32_t*>(funcBytes + off + 1);
					uintptr_t target = funcAddr + off + 5 + rel;
					if (target >= moduleBase && target < moduleBase + moduleSize) {
						Log(fmt::format("[OAR-Probe]   vtbl[{:2}]+0x{:03X}: CALL {:X} (rva {:X})",
							i, off, target, target - moduleBase));
						callCount++;
					}
				}
			}
			if (callCount > 5) {
				Log(fmt::format("[OAR-Probe]   vtbl[{:2}] has {} CALLs - possible loading function", i, callCount));
			}
		}

		Log("[OAR-Probe] === Cross-referencing hkbAnimationBindingSet vtable ===");
		auto* textStart = reinterpret_cast<uint8_t*>(moduleBase);
		auto leaPattern = [&](uint8_t* code, int len) -> bool {
			for (int i = 0; i < len - 7; i++) {
				if (code[i] == 0x48 && code[i+1] == 0x8D) {
					int32_t disp = *reinterpret_cast<int32_t*>(code + i + 3);
					uintptr_t leaTarget = reinterpret_cast<uintptr_t>(code + i + 7) + disp;
					if (leaTarget == bindingSetVtbl || leaTarget == stringDataVtbl) {
						return true;
					}
				}
			}
			return false;
		};

		for (int i = 0; i < 40; i++) {
			auto* vtblEntry = reinterpret_cast<uintptr_t*>(bsGraphVtblBase + i * 8);
			if (IsBadReadPtr(vtblEntry, 8)) break;
			auto funcAddr = *vtblEntry;
			if (funcAddr < moduleBase || funcAddr >= moduleBase + moduleSize) continue;

			auto* funcBytes = reinterpret_cast<uint8_t*>(funcAddr);
			for (int off = 0; off < 1024; off++) {
				if (IsBadReadPtr(funcBytes + off, 5)) break;
				if (funcBytes[off] == 0xC3 && off > 10) break;

				if (funcBytes[off] == 0xE8) {
					int32_t rel = *reinterpret_cast<int32_t*>(funcBytes + off + 1);
					uintptr_t target = funcAddr + off + 5 + rel;
					if (target < moduleBase || target >= moduleBase + moduleSize) continue;

					auto* targetBytes = reinterpret_cast<uint8_t*>(target);
					if (!IsBadReadPtr(targetBytes, 256) && leaPattern(targetBytes, 256)) {
						Log(fmt::format("[OAR-Probe] *** XREF HIT: vtbl[{:2}]+0x{:03X} -> {:X} references BindingSet/StringData vtable",
							i, off, target));
					}
				}
			}
		}

		Log("[OAR-Probe] === End loadClips scan ===");
	}
}

namespace
{
	void InitializeLogging()
	{
		auto path = F4SE::log::log_directory();
		if (!path) {
			F4SE::stl::report_and_fail("Failed to find F4SE log directory"sv);
		}
		*path /= std::format("{}.log"sv, Plugin::NAME);

		auto log = std::make_shared<spdlog::logger>(
			"global"s,
			std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
		log->set_level(spdlog::level::trace);
		log->flush_on(spdlog::level::info);
		set_default_logger(std::move(log));
	}

	void MessageCallback(F4SE::MessagingInterface::Message* msg)
	{
		if (!msg) return;

		switch (msg->type) {
		case F4SE::MessagingInterface::kGameDataReady:
		{
			logger::info("[OAR] kGameDataReady - initializing");
			Settings::GetSingleton()->Load();
			AnimationLog::GetSingleton()->SetMaxEntries(Settings::GetSingleton()->iMaxLogEntries);
			RegisterAllFunctions();
			Hooks::Install();
			Parsing::ParseAllMods();

			Hooks::LoadClipsHooks::TryDeferredInjection();

			if (Settings::GetSingleton()->bVerboseLogging) {
				StructProbe::VerifyHavokLayouts();
				StructProbe::ScanForLoadClipsCallSite();
			}

			// BSInputDevice::Poll hooks require BSInputDeviceManager to be valid,
			// which is only guaranteed after game data is ready (not during F4SEPlugin_Load)
			UIManager::InstallDevicePollHooks();

			UIManager::GetSingleton()->ShowWelcomeBanner();
			logger::info("[OAR] Initialization complete");
			break;
		}

		case F4SE::MessagingInterface::kPreLoadGame:
			logger::info("[OAR] kPreLoadGame - clearing runtime state");
			SetGameFullyLoaded(false);
			// Un-replace everything while the recorded originals are still
			// valid — clips that carry a replacement across the load become
			// unrecoverable orphans otherwise (see RestoreAllActiveReplacements).
			RestoreAllActiveReplacements();
			ActiveClipManager::GetSingleton()->ClearAll();
			ClearCharacterCache();
			ClearClipRuntimeState();
			AnimationCache::GetSingleton()->InvalidateRuntimeClones();
			break;

		case F4SE::MessagingInterface::kPostLoadGame:
			logger::info("[OAR] kPostLoadGame");
			Settings::GetSingleton()->Load();
			AnimationLog::GetSingleton()->SetMaxEntries(Settings::GetSingleton()->iMaxLogEntries);
			ClearCharacterCache();
			FormRegistry::GetSingleton()->InvalidateCache();
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				RegisterActorCharacter(player);
			}
			PopulateKnownStringData();
			RefreshWeaponAnimFolder();
			RegisterWeaponEquipListener();
			if (!HasActiveReplacements()) {
				Hooks::LoadClipsHooks::TryDeferredInjection();
			}
			SetGameFullyLoaded(true);
			logger::info("[OAR] Game fully loaded, clip hooks active");
			break;

		case F4SE::MessagingInterface::kNewGame:
			logger::info("[OAR] kNewGame");
			ActiveClipManager::GetSingleton()->ClearAll();
			Settings::GetSingleton()->Load();
			ClearCharacterCache();
			ClearClipRuntimeState();
			FormRegistry::GetSingleton()->InvalidateCache();
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				RegisterActorCharacter(player);
			}
			PopulateKnownStringData();
			RefreshWeaponAnimFolder();
			RegisterWeaponEquipListener();
			SetGameFullyLoaded(true);
			logger::info("[OAR] New game started, clip hooks active");
			break;

		default:
			break;
		}
	}
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* F4SE, F4SE::PluginInfo* info)
{
	info->infoVersion = F4SE::PluginInfo::kVersion;
	info->name        = Plugin::NAME.data();
	info->version     = 1;

	if (F4SE->IsEditor()) {
		logger::critical("[OAR] Loaded in editor, skipping");
		return false;
	}

	const auto ver = F4SE->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical("[OAR] Unsupported runtime version {}", ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* F4SE)
{
	InitializeLogging();
	logger::info("{} v{}.{}.{} loading", Plugin::NAME,
		Plugin::VERSION[0], Plugin::VERSION[1], Plugin::VERSION[2]);

	F4SE::Init(F4SE);

	F4SE::AllocTrampoline(1024);

	// Load settings early so the toggle hotkey is correct from the start
	Settings::GetSingleton()->Load();

	// D3D11 hooks MUST be installed here in F4SEPlugin_Load — before the game
	// creates its D3D11 device. Installing them in kGameDataReady is too late
	// because the device has already been created by then.
	UIManager::InstallHooks();

	auto* messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageCallback)) {
		logger::critical("[OAR] Failed to register messaging listener");
		return false;
	}

	logger::info("[OAR] Plugin loaded, waiting for kGameDataReady");
	return true;
}
