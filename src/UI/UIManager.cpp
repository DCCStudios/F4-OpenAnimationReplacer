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

void UIManager::InstallHooks()
{
	auto& trampoline = F4SE::GetTrampoline();

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
	auto user32 = GetModuleHandleA("user32.dll");
	if (user32) {
		OriginalClipCursor = reinterpret_cast<ClipCursorFn>(
			GetProcAddress(user32, "ClipCursor"));
	}

	if (!OriginalClipCursor) {
		logger::warn("[OAR] Could not resolve ClipCursor from user32.dll, skipping hook");
		return;
	}

	try {
		REL::Relocation<std::uintptr_t> iatEntry{ REL::ID(641385) };
		auto iatAddr = iatEntry.address();
		auto storedFn = *reinterpret_cast<std::uintptr_t*>(iatAddr);

		if (storedFn == reinterpret_cast<std::uintptr_t>(OriginalClipCursor)) {
			REL::safe_write(iatAddr, reinterpret_cast<std::uintptr_t>(&HookedClipCursor));
			logger::info("[OAR] ClipCursor IAT hook installed");
		} else {
			logger::warn("[OAR] ClipCursor IAT entry mismatch (expected {:X}, found {:X}), skipping hook",
				reinterpret_cast<std::uintptr_t>(OriginalClipCursor), storedFn);
			OriginalClipCursor = nullptr;
		}
	} catch (...) {
		logger::warn("[OAR] ClipCursor IAT hook failed (REL::ID lookup), skipping");
		OriginalClipCursor = nullptr;
	}
}

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

	logger::info("[OAR] D3D11 device created - hooking Present");

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

void UIManager::InitImGui(IDXGISwapChain* a_swapChain)
{
	if (initialized.load()) return;

	ID3D11Device* dev = nullptr;
	a_swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&dev));
	if (!dev) return;

	device.Attach(dev);
	device->GetImmediateContext(context.GetAddressOf());

	DXGI_SWAP_CHAIN_DESC desc{};
	a_swapChain->GetDesc(&desc);
	gameWindow = desc.OutputWindow;

	ID3D11Texture2D* backBuffer = nullptr;
	a_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
	if (backBuffer) {
		device->CreateRenderTargetView(backBuffer, nullptr, mainRTV.GetAddressOf());
		backBuffer->Release();
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	UICommon::ApplyOARStyle();

	auto& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
	io.IniFilename = nullptr;
	io.MouseDrawCursor = false;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	if (!ImGui_ImplWin32_Init(gameWindow)) return;
	if (!ImGui_ImplDX11_Init(device.Get(), context.Get())) return;

	// Save the full window rect for ClipCursor override (mirrors F4SE-Menu-Framework-3)
	if (gameWindow) {
		GetWindowRect(gameWindow, &savedWindowRect);
	}

	originalWndProc = reinterpret_cast<WNDPROC>(
		SetWindowLongPtrA(gameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&HookedWndProc)));

	initialized.store(true);
	logger::info("[OAR] ImGui initialized");
}

void UIManager::RenderFrame()
{
	// Process pending background jobs (config saves, reloads, etc.)
	JobQueue::GetSingleton()->ProcessAll();

	// Transition lock state (mirrors F4SE-Menu-Framework-3 GameLock pattern)
	UpdateLockState();

	bool mainOpen = menuOpen.load();

	bool anyIndependentOpen = false;
	for (auto& win : windows) {
		if (win && win->IsIndependent() && (win->IsOpen() || win->ShouldDrawOverlay())) {
			anyIndependentOpen = true;
			break;
		}
	}

	if (!mainOpen && !anyIndependentOpen) return;

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	if (mainOpen && gameWindow) {
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
	context->OMSetRenderTargets(1, mainRTV.GetAddressOf(), nullptr);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

LRESULT CALLBACK UIManager::HookedWndProc(HWND a_hwnd, UINT a_msg, WPARAM a_wParam, LPARAM a_lParam)
{
	auto* ui = UIManager::GetSingleton();

	if (!ui->initialized.load()) {
		if (ui->originalWndProc)
			return CallWindowProcA(ui->originalWndProc, a_hwnd, a_msg, a_wParam, a_lParam);
		return DefWindowProcA(a_hwnd, a_msg, a_wParam, a_lParam);
	}

	if (a_msg == WM_KILLFOCUS) {
		ImGui::GetIO().ClearInputKeys();
	}

	// Consume the WM_CHAR that follows a consumed WM_KEYDOWN (TranslateMessage queues it)
	if (a_msg == WM_CHAR && s_consumeNextChar) {
		s_consumeNextChar = false;
		return 0;
	}

	// Consume the WM_KEYUP for the toggle key so the game never sees the full press cycle
	if (a_msg == WM_KEYUP && s_consumeKeyUpScanCode != 0) {
		auto scanCode = (a_lParam >> 16) & 0xFF;
		if (scanCode == s_consumeKeyUpScanCode) {
			s_consumeKeyUpScanCode = 0;
			return 0;
		}
	}

	// Handle toggle hotkey - always process this regardless of menu state
	if (a_msg == WM_KEYDOWN) {
		auto scanCode = (a_lParam >> 16) & 0xFF;
		bool isFirstPress = (a_lParam & 0x40000000) == 0;
		auto* settings = Settings::GetSingleton();
		bool shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

		if (isFirstPress && scanCode == settings->iToggleKey &&
			(!settings->bRequireShift || shiftHeld)) {
			ui->ToggleMenu();
			s_consumeNextChar = true;
			s_consumeKeyUpScanCode = scanCode;
			return 0;
		}
	}

	// When blocking (main menu open): route ALL messages to ImGui and
	// do NOT forward to the game's window procedure at all.
	// This mirrors F4SE-Menu-Framework-3's approach.
	if (ui->menuOpen.load()) {
		if (a_msg == WM_SETCURSOR) {
			::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
			return TRUE;
		}
		ImGui_ImplWin32_WndProcHandler(a_hwnd, a_msg, a_wParam, a_lParam);
		return true;
	}

	// When only independent windows are open (anim log, debug overlay),
	// let ImGui handle input it wants but still forward to the game.
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

BOOL WINAPI UIManager::HookedClipCursor(const RECT* a_rect)
{
	auto* ui = UIManager::GetSingleton();

	// When the menu is blocking, pass NULL to completely remove any clip
	// constraint so the cursor can reach the full screen/window edges.
	if (ui->menuOpen.load()) {
		return OriginalClipCursor(nullptr);
	}

	return OriginalClipCursor(a_rect);
}

void UIManager::HookSetCursorPos()
{
	auto user32 = GetModuleHandleA("user32.dll");
	if (!user32) return;

	OriginalSetCursorPos = reinterpret_cast<SetCursorPosFn>(
		GetProcAddress(user32, "SetCursorPos"));

	if (!OriginalSetCursorPos) {
		logger::warn("[OAR] Could not resolve SetCursorPos from user32.dll, skipping hook");
		return;
	}

	// IAT hook: find the game's import of SetCursorPos and redirect it
	auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(GetModuleHandleA(nullptr));
	auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
		reinterpret_cast<uint8_t*>(dosHeader) + dosHeader->e_lfanew);
	auto& importDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

	if (!importDir.VirtualAddress) {
		logger::warn("[OAR] No import directory found for SetCursorPos hook");
		return;
	}

	auto* importDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
		reinterpret_cast<uint8_t*>(dosHeader) + importDir.VirtualAddress);

	bool hooked = false;
	for (; importDesc->Name; importDesc++) {
		auto* dllName = reinterpret_cast<const char*>(
			reinterpret_cast<uint8_t*>(dosHeader) + importDesc->Name);

		if (_stricmp(dllName, "user32.dll") != 0 && _stricmp(dllName, "USER32.dll") != 0 &&
			_stricmp(dllName, "USER32.DLL") != 0)
			continue;

		auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
			reinterpret_cast<uint8_t*>(dosHeader) + importDesc->FirstThunk);

		for (; thunk->u1.Function; thunk++) {
			auto funcAddr = static_cast<uintptr_t>(thunk->u1.Function);
			if (funcAddr == reinterpret_cast<uintptr_t>(OriginalSetCursorPos)) {
				DWORD oldProtect;
				VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect);
				thunk->u1.Function = reinterpret_cast<uintptr_t>(&HookedSetCursorPos);
				VirtualProtect(&thunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);
				hooked = true;
				logger::info("[OAR] SetCursorPos IAT hook installed");
				break;
			}
		}
		if (hooked) break;
	}

	if (!hooked) {
		logger::warn("[OAR] SetCursorPos IAT entry not found in game exe, skipping hook");
		OriginalSetCursorPos = nullptr;
	}
}

BOOL WINAPI UIManager::HookedSetCursorPos(int a_x, int a_y)
{
	auto* ui = UIManager::GetSingleton();

	// Suppress the game's cursor centering when our menu is open
	if (ui->menuOpen.load()) {
		return TRUE;
	}

	return OriginalSetCursorPos(a_x, a_y);
}

void UIManager::ToggleMenu()
{
	SetMenuOpen(!menuOpen.load());
}

void UIManager::SetMenuOpen(bool a_open)
{
	menuOpen.store(a_open);

	if (a_open) {
		currentLockState = LockState::Locked;
		if (Settings::GetSingleton()->bPauseOnMenuOpen) {
			if (auto* ui = RE::UI::GetSingleton()) {
				ui->menuMode += 1;
			}
		}
		UIMain::GetSingleton()->SetOpen(true);
	} else {
		currentLockState = LockState::Unlocked;
		if (Settings::GetSingleton()->bPauseOnMenuOpen) {
			if (auto* ui = RE::UI::GetSingleton()) {
				if (ui->menuMode > 0) {
					ui->menuMode -= 1;
				}
			}
		}
	}
}

void UIManager::UpdateLockState()
{
	if (currentLockState == previousLockState)
		return;

	auto& io = ImGui::GetIO();

	if (currentLockState == LockState::Locked) {
		auto* controlMap = RE::ControlMap::GetSingleton();
		if (controlMap) {
			controlMap->ignoreKeyboardMouse = true;
		}

		if (previousLockState == LockState::Unlocked || previousLockState == LockState::None) {
			::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
			::ShowCursor(TRUE);
		}

		io.MouseDrawCursor = true;
		io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
		io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	} else if (currentLockState == LockState::Unlocked) {
		auto* controlMap = RE::ControlMap::GetSingleton();
		if (controlMap) {
			controlMap->ignoreKeyboardMouse = false;
		}

		if (previousLockState == LockState::Locked) {
			::ShowCursor(FALSE);
		}

		io.MouseDrawCursor = false;
		io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
		io.ClearInputKeys();
	}

	previousLockState = currentLockState;
}
