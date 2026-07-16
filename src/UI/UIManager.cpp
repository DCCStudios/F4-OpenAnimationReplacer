#include "UI/UIManager.h"
#include "UI/UIMain.h"
#include "UI/UIAnimationLog.h"
#include "UI/UIAnimationEventLog.h"
#include "UI/UIWelcomeBanner.h"
#include "UI/UIAnimationQueue.h"
#include "UI/UIDebugOverlay.h"
#include "UI/UICommon.h"
#include "Jobs.h"
#include "Offsets.h"
#include "Settings.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================================
// DirectInput scan codes — same as Bethesda engine keyboard button codes.
// Ported from F4SE Menu Framework 3 (Input.cpp).
// ============================================================================
namespace DIK {
	enum : std::uint32_t {
		kEscape       = 0x01,
		kNum1         = 0x02,
		kNum2         = 0x03,
		kNum3         = 0x04,
		kNum4         = 0x05,
		kNum5         = 0x06,
		kNum6         = 0x07,
		kNum7         = 0x08,
		kNum8         = 0x09,
		kNum9         = 0x0A,
		kNum0         = 0x0B,
		kMinus        = 0x0C,
		kEquals       = 0x0D,
		kBackspace    = 0x0E,
		kTab          = 0x0F,
		kQ            = 0x10,
		kW            = 0x11,
		kE            = 0x12,
		kR            = 0x13,
		kT            = 0x14,
		kY            = 0x15,
		kU            = 0x16,
		kI            = 0x17,
		kO            = 0x18,
		kP            = 0x19,
		kBracketLeft  = 0x1A,
		kBracketRight = 0x1B,
		kEnter        = 0x1C,
		kLeftControl  = 0x1D,
		kA            = 0x1E,
		kS            = 0x1F,
		kD            = 0x20,
		kF            = 0x21,
		kG            = 0x22,
		kH            = 0x23,
		kJ            = 0x24,
		kK            = 0x25,
		kL            = 0x26,
		kSemicolon    = 0x27,
		kApostrophe   = 0x28,
		kTilde        = 0x29,
		kLeftShift    = 0x2A,
		kBackslash    = 0x2B,
		kZ            = 0x2C,
		kX            = 0x2D,
		kC            = 0x2E,
		kV            = 0x2F,
		kB            = 0x30,
		kN            = 0x31,
		kM            = 0x32,
		kComma        = 0x33,
		kPeriod       = 0x34,
		kSlash        = 0x35,
		kRightShift   = 0x36,
		kKP_Multiply  = 0x37,
		kLeftAlt      = 0x38,
		kSpacebar     = 0x39,
		kCapsLock     = 0x3A,
		kF1           = 0x3B,
		kF2           = 0x3C,
		kF3           = 0x3D,
		kF4           = 0x3E,
		kF5           = 0x3F,
		kF6           = 0x40,
		kF7           = 0x41,
		kF8           = 0x42,
		kF9           = 0x43,
		kF10          = 0x44,
		kNumLock      = 0x45,
		kScrollLock   = 0x46,
		kKP_7         = 0x47,
		kKP_8         = 0x48,
		kKP_9         = 0x49,
		kKP_Subtract  = 0x4A,
		kKP_4         = 0x4B,
		kKP_5         = 0x4C,
		kKP_6         = 0x4D,
		kKP_Plus      = 0x4E,
		kKP_1         = 0x4F,
		kKP_2         = 0x50,
		kKP_3         = 0x51,
		kKP_0         = 0x52,
		kKP_Decimal   = 0x53,
		kF11          = 0x57,
		kF12          = 0x58,
		kKP_Enter     = 0x9C,
		kRightControl = 0x9D,
		kKP_Divide    = 0xB5,
		kPrintScreen  = 0xB7,
		kRightAlt     = 0xB8,
		kPause        = 0xC5,
		kHome         = 0xC7,
		kUp           = 0xC8,
		kPageUp       = 0xC9,
		kLeft         = 0xCB,
		kRight        = 0xCD,
		kEnd          = 0xCF,
		kDown         = 0xD0,
		kPageDown     = 0xD1,
		kInsert       = 0xD2,
		kDelete       = 0xD3,
		kLeftWin      = 0xDB,
		kRightWin     = 0xDC,
	};
}

// Mouse button codes as used by the Bethesda engine
namespace MouseKey {
	enum : std::uint32_t {
		kLeftButton   = 0,
		kRightButton  = 1,
		kMiddleButton = 2,
		kButton3      = 3,
		kButton4      = 4,
		kButton5      = 5,
		kButton6      = 6,
		kButton7      = 7,
		kWheelUp      = 8,
		kWheelDown    = 9,
	};
}

// XInput button masks used by Bethesda engine for gamepad
namespace GamepadKey {
	enum : std::uint32_t {
		kUp            = 0x0001,
		kDown          = 0x0002,
		kLeft          = 0x0004,
		kRight         = 0x0008,
		kStart         = 0x0010,
		kBack          = 0x0020,
		kLeftThumb     = 0x0040,
		kRightThumb    = 0x0080,
		kLeftShoulder  = 0x0100,
		kRightShoulder = 0x0200,
		kA             = 0x1000,
		kB             = 0x2000,
		kX             = 0x4000,
		kY             = 0x8000,
	};
}

// ============================================================================
// DIK scan code → ImGuiKey mapping (full table from F4SE Menu Framework 3)
// ============================================================================
static ImGuiKey ParseKeyFromKeyboard(std::uint32_t a_key)
{
	switch (a_key) {
		case DIK::kTab:           return ImGuiKey_Tab;
		case DIK::kLeft:          return ImGuiKey_LeftArrow;
		case DIK::kRight:         return ImGuiKey_RightArrow;
		case DIK::kUp:            return ImGuiKey_UpArrow;
		case DIK::kDown:          return ImGuiKey_DownArrow;
		case DIK::kPageUp:        return ImGuiKey_PageUp;
		case DIK::kPageDown:      return ImGuiKey_PageDown;
		case DIK::kHome:          return ImGuiKey_Home;
		case DIK::kEnd:           return ImGuiKey_End;
		case DIK::kInsert:        return ImGuiKey_Insert;
		case DIK::kDelete:        return ImGuiKey_Delete;
		case DIK::kBackspace:     return ImGuiKey_Backspace;
		case DIK::kSpacebar:      return ImGuiKey_Space;
		case DIK::kEnter:         return ImGuiKey_Enter;
		case DIK::kEscape:        return ImGuiKey_Escape;
		case DIK::kLeftControl:   return ImGuiKey_LeftCtrl;
		case DIK::kLeftShift:     return ImGuiKey_LeftShift;
		case DIK::kLeftAlt:       return ImGuiKey_LeftAlt;
		case DIK::kLeftWin:       return ImGuiKey_LeftSuper;
		case DIK::kRightControl:  return ImGuiKey_RightCtrl;
		case DIK::kRightShift:    return ImGuiKey_RightShift;
		case DIK::kRightAlt:      return ImGuiKey_RightAlt;
		case DIK::kRightWin:      return ImGuiKey_RightSuper;
		case DIK::kNum0:          return ImGuiKey_0;
		case DIK::kNum1:          return ImGuiKey_1;
		case DIK::kNum2:          return ImGuiKey_2;
		case DIK::kNum3:          return ImGuiKey_3;
		case DIK::kNum4:          return ImGuiKey_4;
		case DIK::kNum5:          return ImGuiKey_5;
		case DIK::kNum6:          return ImGuiKey_6;
		case DIK::kNum7:          return ImGuiKey_7;
		case DIK::kNum8:          return ImGuiKey_8;
		case DIK::kNum9:          return ImGuiKey_9;
		case DIK::kA:             return ImGuiKey_A;
		case DIK::kB:             return ImGuiKey_B;
		case DIK::kC:             return ImGuiKey_C;
		case DIK::kD:             return ImGuiKey_D;
		case DIK::kE:             return ImGuiKey_E;
		case DIK::kF:             return ImGuiKey_F;
		case DIK::kG:             return ImGuiKey_G;
		case DIK::kH:             return ImGuiKey_H;
		case DIK::kI:             return ImGuiKey_I;
		case DIK::kJ:             return ImGuiKey_J;
		case DIK::kK:             return ImGuiKey_K;
		case DIK::kL:             return ImGuiKey_L;
		case DIK::kM:             return ImGuiKey_M;
		case DIK::kN:             return ImGuiKey_N;
		case DIK::kO:             return ImGuiKey_O;
		case DIK::kP:             return ImGuiKey_P;
		case DIK::kQ:             return ImGuiKey_Q;
		case DIK::kR:             return ImGuiKey_R;
		case DIK::kS:             return ImGuiKey_S;
		case DIK::kT:             return ImGuiKey_T;
		case DIK::kU:             return ImGuiKey_U;
		case DIK::kV:             return ImGuiKey_V;
		case DIK::kW:             return ImGuiKey_W;
		case DIK::kX:             return ImGuiKey_X;
		case DIK::kY:             return ImGuiKey_Y;
		case DIK::kZ:             return ImGuiKey_Z;
		case DIK::kF1:            return ImGuiKey_F1;
		case DIK::kF2:            return ImGuiKey_F2;
		case DIK::kF3:            return ImGuiKey_F3;
		case DIK::kF4:            return ImGuiKey_F4;
		case DIK::kF5:            return ImGuiKey_F5;
		case DIK::kF6:            return ImGuiKey_F6;
		case DIK::kF7:            return ImGuiKey_F7;
		case DIK::kF8:            return ImGuiKey_F8;
		case DIK::kF9:            return ImGuiKey_F9;
		case DIK::kF10:           return ImGuiKey_F10;
		case DIK::kF11:           return ImGuiKey_F11;
		case DIK::kF12:           return ImGuiKey_F12;
		case DIK::kApostrophe:    return ImGuiKey_Apostrophe;
		case DIK::kComma:         return ImGuiKey_Comma;
		case DIK::kMinus:         return ImGuiKey_Minus;
		case DIK::kPeriod:        return ImGuiKey_Period;
		case DIK::kSlash:         return ImGuiKey_Slash;
		case DIK::kSemicolon:     return ImGuiKey_Semicolon;
		case DIK::kEquals:        return ImGuiKey_Equal;
		case DIK::kBracketLeft:   return ImGuiKey_LeftBracket;
		case DIK::kBackslash:     return ImGuiKey_Backslash;
		case DIK::kBracketRight:  return ImGuiKey_RightBracket;
		case DIK::kTilde:         return ImGuiKey_GraveAccent;
		case DIK::kCapsLock:      return ImGuiKey_CapsLock;
		case DIK::kScrollLock:    return ImGuiKey_ScrollLock;
		case DIK::kNumLock:       return ImGuiKey_NumLock;
		case DIK::kPrintScreen:   return ImGuiKey_PrintScreen;
		case DIK::kPause:         return ImGuiKey_Pause;
		case DIK::kKP_0:          return ImGuiKey_Keypad0;
		case DIK::kKP_1:          return ImGuiKey_Keypad1;
		case DIK::kKP_2:          return ImGuiKey_Keypad2;
		case DIK::kKP_3:          return ImGuiKey_Keypad3;
		case DIK::kKP_4:          return ImGuiKey_Keypad4;
		case DIK::kKP_5:          return ImGuiKey_Keypad5;
		case DIK::kKP_6:          return ImGuiKey_Keypad6;
		case DIK::kKP_7:          return ImGuiKey_Keypad7;
		case DIK::kKP_8:          return ImGuiKey_Keypad8;
		case DIK::kKP_9:          return ImGuiKey_Keypad9;
		case DIK::kKP_Decimal:    return ImGuiKey_KeypadDecimal;
		case DIK::kKP_Divide:     return ImGuiKey_KeypadDivide;
		case DIK::kKP_Multiply:   return ImGuiKey_KeypadMultiply;
		case DIK::kKP_Subtract:   return ImGuiKey_KeypadSubtract;
		case DIK::kKP_Plus:       return ImGuiKey_KeypadAdd;
		case DIK::kKP_Enter:      return ImGuiKey_KeypadEnter;
		default:                  return ImGuiKey_None;
	}
}

// XInput gamepad button → ImGuiKey mapping
static ImGuiKey ParseKeyFromGamepad(std::uint32_t a_key)
{
	switch (a_key) {
		case GamepadKey::kUp:            return ImGuiKey_GamepadDpadUp;
		case GamepadKey::kDown:          return ImGuiKey_GamepadDpadDown;
		case GamepadKey::kLeft:          return ImGuiKey_GamepadDpadLeft;
		case GamepadKey::kRight:         return ImGuiKey_GamepadDpadRight;
		case GamepadKey::kStart:         return ImGuiKey_GamepadStart;
		case GamepadKey::kBack:          return ImGuiKey_GamepadBack;
		case GamepadKey::kLeftThumb:     return ImGuiKey_GamepadL3;
		case GamepadKey::kRightThumb:    return ImGuiKey_GamepadR3;
		case GamepadKey::kLeftShoulder:  return ImGuiKey_GamepadL1;
		case GamepadKey::kRightShoulder: return ImGuiKey_GamepadR1;
		case GamepadKey::kA:             return ImGuiKey_GamepadFaceDown;
		case GamepadKey::kB:             return ImGuiKey_GamepadFaceRight;
		case GamepadKey::kX:             return ImGuiKey_GamepadFaceLeft;
		case GamepadKey::kY:             return ImGuiKey_GamepadFaceUp;
		default:                         return ImGuiKey_None;
	}
}

// Translate a single Bethesda RE::ButtonEvent into the appropriate ImGui input call
static void TranslateButtonEvent(ImGuiIO& io, const RE::ButtonEvent* a_button)
{
	if (!a_button->HasIDCode()) {
		return;
	}

	bool pressed = a_button->value != 0.0f;

	switch (*a_button->device) {
		case RE::INPUT_DEVICE::kKeyboard: {
			auto imKey = ParseKeyFromKeyboard(static_cast<std::uint32_t>(a_button->idCode));
			io.AddKeyEvent(imKey, pressed);
		} break;

		case RE::INPUT_DEVICE::kMouse: {
			auto key = static_cast<std::uint32_t>(a_button->idCode);
			switch (key) {
				case MouseKey::kWheelUp:
					io.AddMouseWheelEvent(0, a_button->value);
					break;
				case MouseKey::kWheelDown:
					io.AddMouseWheelEvent(0, a_button->value * -1);
					break;
				default:
					io.AddMouseButtonEvent(key, pressed);
					break;
			}
		} break;

		case RE::INPUT_DEVICE::kGamepad: {
			auto imKey = ParseKeyFromGamepad(static_cast<std::uint32_t>(a_button->idCode));
			io.AddKeyEvent(imKey, pressed);
		} break;

		default:
			break;
	}
}

// ============================================================================
// UIManager implementation
// ============================================================================

UIManager::UIManager()
{
	InitWindows();
}

void UIManager::InitWindows()
{
	if (windowsInitialized) return;

	windows.resize(static_cast<size_t>(WindowID::kCount));
	windows[static_cast<size_t>(WindowID::kMain)] = nullptr;
	windows[static_cast<size_t>(WindowID::kAnimationLog)] = std::make_unique<UIAnimationLog>();
	windows[static_cast<size_t>(WindowID::kAnimationEventLog)] = std::make_unique<UIAnimationEventLog>();
	windows[static_cast<size_t>(WindowID::kWelcomeBanner)] = std::make_unique<UIWelcomeBanner>();
	windows[static_cast<size_t>(WindowID::kAnimationQueue)] = std::make_unique<UIAnimationQueue>();
	windows[static_cast<size_t>(WindowID::kDebugOverlay)] = std::make_unique<UIDebugOverlay>();

	windowsInitialized = true;
}

UIWindow* UIManager::GetWindow(WindowID a_id)
{
	auto idx = static_cast<size_t>(a_id);
	if (a_id == WindowID::kMain) return UIMain::GetSingleton();
	if (idx < windows.size() && windows[idx]) return windows[idx].get();
	return nullptr;
}

void UIManager::ToggleWindow(WindowID a_id)
{
	auto* win = GetWindow(a_id);
	if (win) win->Toggle();
}

void UIManager::ShowWelcomeBanner()
{
	auto* banner = dynamic_cast<UIWelcomeBanner*>(GetWindow(WindowID::kWelcomeBanner));
	if (banner) banner->Show();
}

// ============================================================================
// Hook installation
// ============================================================================

void UIManager::InstallHooks()
{
	auto& trampoline = F4SE::GetTrampoline();

	// D3D11CreateDeviceAndSwapChain call-site hook (must run before device creation)
	OriginalD3D11Create = reinterpret_cast<D3D11CreateFn>(
		trampoline.write_call<5>(
			Offsets::ptr_D3D11CreateDevice.address() + Offsets::D3D11Create_Offset,
			reinterpret_cast<uintptr_t>(&HookedD3D11CreateDeviceAndSwapChain)
		)
	);
	logger::info("[OAR] D3D11CreateDeviceAndSwapChain hook installed");

	HookClipCursor();
	HookSetCursorPos();
}

void UIManager::HookClipCursor()
{
	try {
		REL::Relocation<std::uintptr_t> iatEntry{ REL::ID(641385) };
		auto iatAddr = iatEntry.address();

		// Save whatever is currently in the IAT entry (may be user32!ClipCursor
		// or another plugin's hook — we chain through it either way).
		OriginalClipCursor = reinterpret_cast<ClipCursorFn>(
			*reinterpret_cast<std::uintptr_t*>(iatAddr));

		if (!OriginalClipCursor) {
			logger::warn("[OAR] ClipCursor IAT entry is null, skipping hook");
			return;
		}

		REL::safe_write(iatAddr, reinterpret_cast<std::uintptr_t>(&HookedClipCursor));
		logger::info("[OAR] ClipCursor IAT hook installed (chaining through {:X})",
			reinterpret_cast<std::uintptr_t>(OriginalClipCursor));
	} catch (...) {
		logger::warn("[OAR] ClipCursor IAT hook failed (REL::ID lookup), skipping");
		OriginalClipCursor = nullptr;
	}
}

void UIManager::HookSetCursorPos()
{
	auto* exeBase = reinterpret_cast<std::uint8_t*>(GetModuleHandleA(nullptr));
	auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(exeBase);
	auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(exeBase + dos->e_lfanew);
	auto& importDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

	if (importDir.Size == 0) {
		logger::warn("[OAR] No import directory found, SetCursorPos hook skipped");
		return;
	}

	auto realSetCursorPos = reinterpret_cast<std::uintptr_t>(
		GetProcAddress(GetModuleHandleA("user32.dll"), "SetCursorPos"));
	if (!realSetCursorPos) {
		logger::warn("[OAR] Could not resolve SetCursorPos from user32.dll");
		return;
	}

	auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(exeBase + importDir.VirtualAddress);
	for (; desc->Name; ++desc) {
		auto* thunkRef = reinterpret_cast<std::uintptr_t*>(exeBase + desc->FirstThunk);
		for (; *thunkRef; ++thunkRef) {
			if (*thunkRef == realSetCursorPos) {
				DWORD oldProtect;
				VirtualProtect(thunkRef, sizeof(void*), PAGE_READWRITE, &oldProtect);
				OriginalSetCursorPos = reinterpret_cast<SetCursorPosFn>(*thunkRef);
				*thunkRef = reinterpret_cast<std::uintptr_t>(&HookedSetCursorPos);
				VirtualProtect(thunkRef, sizeof(void*), oldProtect, &oldProtect);
				logger::info("[OAR] SetCursorPos IAT hook installed");
				return;
			}
		}
	}
	logger::warn("[OAR] SetCursorPos not found in IAT, hook skipped");
}

// ============================================================================
// BSInputDevice::Poll VTable hooks
// Ported from F4SE Menu Framework 3 — hooks VTable[1] (Poll) on the keyboard
// and mouse BSInputDevice singletons so we can toggle ImGui's input flags
// every frame in sync with the actual device poll cycle.
// ============================================================================

void UIManager::InstallDevicePollHooks()
{
	auto* mgr = RE::BSInputDeviceManager::GetSingleton();
	if (!mgr) {
		logger::error("[OAR] BSInputDeviceManager::GetSingleton() returned null — DevicePoll hooks skipped");
		return;
	}

	// Hook keyboard device Poll (VTable index 1)
	auto* kbd = mgr->devices[static_cast<std::int32_t>(RE::INPUT_DEVICE::kKeyboard)];
	if (kbd) {
		void** vTable = *reinterpret_cast<void***>(kbd);
		DWORD oldProtect;
		VirtualProtect(&vTable[1], sizeof(void*), PAGE_READWRITE, &oldProtect);
		DevicePollHook::OriginalKeyboardPoll = reinterpret_cast<decltype(DevicePollHook::OriginalKeyboardPoll)>(vTable[1]);
		vTable[1] = reinterpret_cast<void*>(&DevicePollHook::KeyboardThunk);
		VirtualProtect(&vTable[1], sizeof(void*), oldProtect, &oldProtect);
		logger::info("[OAR] Keyboard BSInputDevice::Poll VTable hook installed");
	} else {
		logger::warn("[OAR] Keyboard BSInputDevice not found, skipping Poll hook");
	}

	// Hook mouse device Poll (VTable index 1)
	auto* mouse = mgr->devices[static_cast<std::int32_t>(RE::INPUT_DEVICE::kMouse)];
	if (mouse) {
		void** vTable = *reinterpret_cast<void***>(mouse);
		DWORD oldProtect;
		VirtualProtect(&vTable[1], sizeof(void*), PAGE_READWRITE, &oldProtect);
		DevicePollHook::OriginalMousePoll = reinterpret_cast<decltype(DevicePollHook::OriginalMousePoll)>(vTable[1]);
		vTable[1] = reinterpret_cast<void*>(&DevicePollHook::MouseThunk);
		VirtualProtect(&vTable[1], sizeof(void*), oldProtect, &oldProtect);
		logger::info("[OAR] Mouse BSInputDevice::Poll VTable hook installed");
	} else {
		logger::warn("[OAR] Mouse BSInputDevice not found, skipping Poll hook");
	}
}

void __fastcall UIManager::DevicePollHook::KeyboardThunk(RE::BSInputDevice* a_device, float a_pollDelta)
{
	OriginalKeyboardPoll(a_device, a_pollDelta);

	auto* ui = UIManager::GetSingleton();
	if (!ui->initialized.load()) return;

	if (ui->menuOpen.load()) {
		EnableImGuiInput();
	} else {
		DisableImGuiInput();
	}
}

void __fastcall UIManager::DevicePollHook::MouseThunk(RE::BSInputDevice* a_device, float a_pollDelta)
{
	OriginalMousePoll(a_device, a_pollDelta);

	auto* ui = UIManager::GetSingleton();
	if (!ui->initialized.load()) return;

	if (ui->menuOpen.load()) {
		EnableImGuiInput();
	} else {
		DisableImGuiInput();
	}
}

void UIManager::EnableImGuiInput()
{
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
	io.ConfigNavCaptureKeyboard = true;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
}

void UIManager::DisableImGuiInput()
{
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
	io.ConfigNavCaptureKeyboard = false;
	io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
}

// ============================================================================
// Bethesda input event translation
// Walks the engine's RE::InputEvent linked list and converts each
// RE::ButtonEvent / RE::CharacterEvent into the corresponding ImGui call.
// ============================================================================

void UIManager::TranslateInputEvent(RE::InputEvent* const* a_event)
{
	auto& io = ImGui::GetIO();

	for (auto event = *a_event; event; event = event->next) {
		if (auto button = event->As<RE::ButtonEvent>()) {
			TranslateButtonEvent(io, button);
		} else if (auto charEvent = event->As<RE::CharacterEvent>()) {
			io.AddInputCharacter(charEvent->charCode);
		}
	}
}

// ============================================================================
// D3D11 hooks
// ============================================================================

HRESULT WINAPI UIManager::HookedD3D11CreateDeviceAndSwapChain(
	IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software,
	UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels,
	UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
	IDXGISwapChain** ppSwapChain, ID3D11Device** ppDevice,
	D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext)
{
	HRESULT hr = OriginalD3D11Create(pAdapter, DriverType, Software, Flags,
		pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc,
		ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);

	if (FAILED(hr)) return hr;
	if (!ppSwapChain || !*ppSwapChain) return hr;

	logger::info("[OAR] D3D11 device created — hooking Present");

	// Hook IDXGISwapChain::Present via VTable[8]
	auto* vtbl = *reinterpret_cast<void***>(*ppSwapChain);
	OriginalPresent = reinterpret_cast<PresentFn>(vtbl[8]);

	DWORD oldProtect;
	VirtualProtect(&vtbl[8], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
	vtbl[8] = reinterpret_cast<void*>(&HookedPresent);
	VirtualProtect(&vtbl[8], sizeof(void*), oldProtect, &oldProtect);

	return hr;
}

HRESULT WINAPI UIManager::HookedPresent(IDXGISwapChain* a_swapChain, UINT a_syncInterval, UINT a_flags)
{
	auto* ui = UIManager::GetSingleton();

	if (!ui->initialized.load()) {
		ui->InitImGui(a_swapChain);
	}

	if (ui->initialized.load()) {
		ui->RenderFrame();
	}

	return OriginalPresent(a_swapChain, a_syncInterval, a_flags);
}

// ============================================================================
// ImGui initialization — simplified to match F4SE Menu Framework 3.
// No render target view creation; DX11 draws into whatever target Present
// already has bound.  ConfigFlags set to framework defaults; DevicePollHook
// manages NoMouse dynamically each frame.
// ============================================================================

void UIManager::InitImGui(IDXGISwapChain* a_swapChain)
{
	if (initialized.load()) return;

	// Get device & context from the swap chain (released after init — ImGui
	// backends hold their own AddRef'd copies internally)
	ID3D11Device* dev = nullptr;
	a_swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&dev));
	if (!dev) return;

	ID3D11DeviceContext* ctx = nullptr;
	dev->GetImmediateContext(&ctx);

	DXGI_SWAP_CHAIN_DESC desc{};
	a_swapChain->GetDesc(&desc);
	gameWindow = desc.OutputWindow;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	UICommon::ApplyOARStyle();

	auto& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	io.IniFilename = nullptr;
	io.MouseDrawCursor = true;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	if (!ImGui_ImplWin32_Init(gameWindow)) {
		dev->Release();
		ctx->Release();
		return;
	}
	if (!ImGui_ImplDX11_Init(dev, ctx)) {
		dev->Release();
		ctx->Release();
		return;
	}

	// Save the full window rect for ClipCursor override — prevents cursor
	// from escaping the game window (mirrors F4SE Menu Framework 3)
	if (gameWindow) {
		GetWindowRect(gameWindow, &savedWindowRect);
	}

	// Subclass the game's WndProc for toggle key / input forwarding
	originalWndProc = reinterpret_cast<WNDPROC>(
		SetWindowLongPtrA(gameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&HookedWndProc)));

	// Release our raw pointers — the ImGui backends already called AddRef
	dev->Release();
	ctx->Release();

	initialized.store(true);
	logger::info("[OAR] ImGui initialized");
}

// ============================================================================
// Per-frame render loop
// Determines lock state from current window state, updates GameLock, then
// renders all visible windows.  No OMSetRenderTargets — DX11 draws into
// whatever render target Present already has bound (matches framework).
// ============================================================================

void UIManager::RenderFrame()
{
	// Process pending background jobs (config saves, reloads, etc.)
	JobQueue::GetSingleton()->ProcessAll();

	bool mainOpen = menuOpen.load();

	bool anyIndependentOpen = false;
	for (auto& win : windows) {
		if (win && win->IsIndependent() && (win->IsOpen() || win->ShouldDrawOverlay())) {
			anyIndependentOpen = true;
			break;
		}
	}

	// Determine and apply lock state each frame (matches framework GameLock pattern)
	if (mainOpen) {
		currentLockState = LockState::Locked;
	} else if (anyIndependentOpen) {
		currentLockState = LockState::Resume;
	} else {
		currentLockState = LockState::Unlocked;
	}
	UpdateLockState();

	if (!mainOpen && !anyIndependentOpen) return;

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	// Always update display size from actual client rect (framework pattern)
	if (gameWindow) {
		RECT clientRect;
		if (GetClientRect(gameWindow, &clientRect)) {
			auto& io = ImGui::GetIO();
			io.DisplaySize = ImVec2(
				static_cast<float>(clientRect.right - clientRect.left),
				static_cast<float>(clientRect.bottom - clientRect.top));
		}
	}

	ImGui::NewFrame();

	auto& io = ImGui::GetIO();
	io.MouseDrawCursor = mainOpen;

	if (mainOpen) {
		UIMain::GetSingleton()->TryDraw();
	}

	for (auto& win : windows) {
		if (win) win->TryDraw();
	}

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

// ============================================================================
// WndProc hook — simplified to match F4SE Menu Framework 3 pattern.
// Toggle key via scan code, ESC closes blocking menu, forward-to-ImGui
// when blocking.  No WM_SETCURSOR override or consume-char tracking.
// ============================================================================

LRESULT CALLBACK UIManager::HookedWndProc(HWND a_hwnd, UINT a_msg, WPARAM a_wParam, LPARAM a_lParam)
{
	auto* ui = UIManager::GetSingleton();

	if (!ui->initialized.load()) {
		if (ui->originalWndProc)
			return CallWindowProcA(ui->originalWndProc, a_hwnd, a_msg, a_wParam, a_lParam);
		return DefWindowProcA(a_hwnd, a_msg, a_wParam, a_lParam);
	}

	// Reset ImGui key state when the window loses focus
	if (a_msg == WM_KILLFOCUS) {
		ImGui::GetIO().ClearInputKeys();
	}

	// Toggle hotkey and ESC close via WM_KEYDOWN scan code
	if (a_msg == WM_KEYDOWN || a_msg == WM_SYSKEYDOWN) {
		auto scanCode = (a_lParam >> 16) & 0xFF;
		bool isFirstPress = (a_lParam & 0x40000000) == 0;
		auto* settings = Settings::GetSingleton();
		bool shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
		auto* mainUi = UIMain::GetSingleton();
		const bool capturingKey = mainUi && mainUi->IsCapturingToggleKey();

		// Settings "Change activation key" capture: next non-modifier keypress
		// becomes the new toggle bind. Escape cancels without closing the menu.
		if (capturingKey && isFirstPress) {
			if (a_wParam == VK_ESCAPE) {
				mainUi->SetCapturingToggleKey(false);
				return 0;
			}
			// Ignore pure modifiers — they are controlled by "Require Shift".
			const bool isModifier =
				scanCode == DIK::kLeftShift || scanCode == DIK::kRightShift ||
				scanCode == DIK::kLeftControl || scanCode == DIK::kRightControl ||
				scanCode == DIK::kLeftAlt || scanCode == DIK::kRightAlt ||
				scanCode == DIK::kLeftWin || scanCode == DIK::kRightWin;
			if (!isModifier && scanCode != 0) {
				mainUi->ApplyCapturedToggleKey(scanCode);
				return 0;
			}
			return 0;
		}

		// Toggle key — open/close the main OAR editor
		if (isFirstPress && scanCode == settings->iToggleKey &&
			(!settings->bRequireShift || shiftHeld)) {
			ui->ToggleMenu();
			return 0;
		}

		// ESC closes the blocking menu (matches framework pattern)
		if (a_wParam == VK_ESCAPE && isFirstPress && ui->menuOpen.load()) {
			ui->SetMenuOpen(false);
			return 0;
		}
	}

	// When blocking (main menu open): forward ALL messages to ImGui and
	// do NOT forward to the game's window procedure at all.
	if (ui->menuOpen.load()) {
		ImGui_ImplWin32_WndProcHandler(a_hwnd, a_msg, a_wParam, a_lParam);
		return true;
	}

	// When only independent windows are open (anim log, debug overlay):
	// let ImGui see input, but still forward to the game.  Selectively
	// block game input if ImGui wants to capture the event.
	bool anyIndependentOpen = false;
	for (auto& win : ui->windows) {
		if (win && win->IsOpen() && win->IsIndependent()) {
			anyIndependentOpen = true;
			break;
		}
	}

	if (anyIndependentOpen) {
		ImGui_ImplWin32_WndProcHandler(a_hwnd, a_msg, a_wParam, a_lParam);
		if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard) {
			switch (a_msg) {
			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			case WM_MOUSEWHEEL:
			case WM_KEYDOWN:
			case WM_KEYUP:
			case WM_CHAR:
				return 0;
			}
		}
	}

	if (ui->originalWndProc)
		return CallWindowProcA(ui->originalWndProc, a_hwnd, a_msg, a_wParam, a_lParam);
	return DefWindowProcA(a_hwnd, a_msg, a_wParam, a_lParam);
}

// ============================================================================
// ClipCursor hook — when menu is blocking, clip cursor to the full window
// rect instead of the game's smaller region.  Prevents the cursor from
// escaping the game window while allowing it to reach all edges.
// Passes savedWindowRect (not nullptr) — matches F4SE Menu Framework 3.
// ============================================================================

BOOL WINAPI UIManager::HookedClipCursor(const RECT* a_rect)
{
	auto* ui = UIManager::GetSingleton();

	if (ui->menuOpen.load() && a_rect) {
		return OriginalClipCursor(&savedWindowRect);
	}

	return OriginalClipCursor(a_rect);
}

// ============================================================================
// SetCursorPos hook — when menu is blocking, suppress the game's attempts
// to recenter the cursor for FPS mouse-look.  Without this, the game snaps
// the cursor to window center every frame, making the ImGui cursor stuck.
// ============================================================================

BOOL WINAPI UIManager::HookedSetCursorPos(int X, int Y)
{
	auto* ui = UIManager::GetSingleton();

	if (ui->menuOpen.load()) {
		return TRUE;
	}

	return OriginalSetCursorPos(X, Y);
}

// ============================================================================
// Menu open/close — manages RE::UI::menuMode for game pause and tells
// the main window to open/close.  Lock state is determined per-frame
// in RenderFrame, not here.
// ============================================================================

void UIManager::ToggleMenu()
{
	SetMenuOpen(!menuOpen.load());
}

void UIManager::SetMenuOpen(bool a_open)
{
	menuOpen.store(a_open);

	if (a_open) {
		if (Settings::GetSingleton()->bPauseOnMenuOpen) {
			if (auto* ui = RE::UI::GetSingleton()) {
				ui->menuMode += 1;
			}
		}
		UIMain::GetSingleton()->SetOpen(true);
	} else {
		if (Settings::GetSingleton()->bPauseOnMenuOpen) {
			if (auto* ui = RE::UI::GetSingleton()) {
				if (ui->menuMode > 0) {
					ui->menuMode -= 1;
				}
			}
		}
	}
}

// ============================================================================
// GameLock state machine — 4-state model matching F4SE Menu Framework 3.
// Called once per frame from RenderFrame with currentLockState already set.
//   Locked  → ignoreKeyboardMouse = true,  ShowCursor(TRUE)
//   Unlocked→ ignoreKeyboardMouse = false, ShowCursor(FALSE), ClearInputKeys
//   Resume  → ignoreKeyboardMouse = false  (no cursor toggle — for non-blocking overlays)
// ============================================================================

void UIManager::UpdateLockState()
{
	if (currentLockState == previousLockState)
		return;

	LockState prev = previousLockState;
	previousLockState = currentLockState;

	auto* controlMap = RE::ControlMap::GetSingleton();

	if (currentLockState == LockState::Locked) {
		if (controlMap) {
			controlMap->ignoreKeyboardMouse = true;
		}
		// Only show cursor on transition from Unlocked/None → Locked
		if (prev == LockState::Unlocked || prev == LockState::None) {
			::ShowCursor(TRUE);
		}

	} else if (currentLockState == LockState::Unlocked) {
		if (controlMap) {
			controlMap->ignoreKeyboardMouse = false;
		}
		// Only hide cursor on transition from Locked → Unlocked
		if (prev == LockState::Locked) {
			::ShowCursor(FALSE);
		}
		auto& io = ImGui::GetIO();
		io.ClearInputKeys();

	} else if (currentLockState == LockState::Resume) {
		// Non-blocking overlays — restore game input without cursor toggle
		if (controlMap) {
			controlMap->ignoreKeyboardMouse = false;
		}
	}
}
