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
	io.IniFilename = nullptr;
	io.MouseDrawCursor = false;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	if (!ImGui_ImplWin32_Init(gameWindow)) return;
	if (!ImGui_ImplDX11_Init(device.Get(), context.Get())) return;

	originalWndProc = reinterpret_cast<WNDPROC>(
		SetWindowLongPtrA(gameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&HookedWndProc)));

	initialized.store(true);
	logger::info("[OAR] ImGui initialized");
}

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

	if (!mainOpen && !anyIndependentOpen) return;

	// Force-release cursor clip EVERY frame while menu is open.
	// The ClipCursor IAT hook often fails (another plugin hooks it first),
	// so we cannot rely on it. Brute-force release ensures the game's
	// per-frame ClipCursor calls are always overridden.
	if (mainOpen) {
		::ClipCursor(nullptr);
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	bool anyInteractiveOpen = false;
	for (auto& win : windows) {
		if (win && win->IsIndependent() && win->IsOpen()) {
			anyInteractiveOpen = true;
			break;
		}
	}

	auto& io = ImGui::GetIO();
	io.MouseDrawCursor = mainOpen || anyInteractiveOpen;

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

	if (a_msg == WM_KEYDOWN) {
		auto scanCode = (a_lParam >> 16) & 0xFF;
		bool isFirstPress = (a_lParam & 0x40000000) == 0;
		auto* settings = Settings::GetSingleton();
		bool shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

		if (isFirstPress && scanCode == settings->iToggleKey &&
			(!settings->bRequireShift || shiftHeld)) {
			bool willBeOpen = !ui->menuOpen.load();

			// Pre-set ignoreKeyboardMouse BEFORE toggling so the game's
			// input pipeline sees the flag this frame (DirectInput reads happen
			// after WndProc dispatch on the same thread)
			auto* controlMap = RE::ControlMap::GetSingleton();
			if (controlMap) {
				controlMap->ignoreKeyboardMouse = willBeOpen;
			}

			ui->ToggleMenu();
			s_consumeNextChar = true;
			s_consumeKeyUpScanCode = scanCode;
			return 0;
		}
	}

	bool anyUIOpen = ui->menuOpen.load();
	if (!anyUIOpen) {
		for (auto& win : ui->windows) {
			if (win && win->IsOpen() && win->IsIndependent()) {
				anyUIOpen = true;
				break;
			}
		}
	}

	// Aggressively release cursor constraint on EVERY WndProc dispatch while menu is open.
	// WndProc fires many times per frame (once per Windows message), so this reliably
	// overrides the game's own ClipCursor calls even when the IAT hook fails.
	if (ui->menuOpen.load()) {
		::ClipCursor(nullptr);
	}

	if (anyUIOpen) {
		ImGui_ImplWin32_WndProcHandler(a_hwnd, a_msg, a_wParam, a_lParam);

		if (ui->menuOpen.load()) {
			switch (a_msg) {
			case WM_MOUSEMOVE:
			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			case WM_RBUTTONDBLCLK:
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			case WM_MBUTTONDBLCLK:
			case WM_MOUSEWHEEL:
			case WM_MOUSEHWHEEL:
			case WM_XBUTTONDOWN:
			case WM_XBUTTONUP:
			case WM_INPUT:
			case WM_KEYDOWN:
			case WM_KEYUP:
			case WM_CHAR:
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
				return 0;
			}
		} else if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard) {
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

	if (ui->menuOpen.load()) {
		return TRUE;
	}
	if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
		return TRUE;
	}

	if (OriginalClipCursor) {
		return OriginalClipCursor(a_rect);
	}
	return TRUE;
}

void UIManager::ToggleMenu()
{
	SetMenuOpen(!menuOpen.load());
}

void UIManager::SetMenuOpen(bool a_open)
{
	menuOpen.store(a_open);

	auto* controlMap = RE::ControlMap::GetSingleton();
	if (controlMap) {
		controlMap->ignoreKeyboardMouse = a_open;
	}

	if (a_open) {
		// Always release cursor constraint using the raw Win32 API.
		// The IAT hook may or may not be installed; this guarantees release.
		::ClipCursor(nullptr);

		// Move OS cursor to screen center so it's immediately visible and usable.
		// Without this, the cursor can be stuck at the last game-warped position.
		if (gameWindow) {
			RECT rc;
			if (GetClientRect(gameWindow, &rc)) {
				POINT center = { (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
				ClientToScreen(gameWindow, &center);
				::SetCursorPos(center.x, center.y);
			}
		}

		auto& io = ImGui::GetIO();
		io.MouseDrawCursor = true;
		UIMain::GetSingleton()->SetOpen(true);
	} else {
		auto& io = ImGui::GetIO();
		io.MouseDrawCursor = false;
	}
}
