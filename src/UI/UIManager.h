#pragma once

#include "UI/UIWindow.h"
#include <d3d11.h>
#include <dxgi.h>

struct ImGuiContext;

class UIManager
{
public:
	static UIManager* GetSingleton()
	{
		static UIManager singleton;
		return &singleton;
	}

	// D3D11 / WndProc / ClipCursor hooks — must be called from F4SEPlugin_Load
	// (before the game creates its D3D11 device)
	static void InstallHooks();

	// BSInputDevice::Poll VTable hooks — must be called from kGameDataReady
	// (after BSInputDeviceManager is valid)
	static void InstallDevicePollHooks();

	void ToggleMenu();
	bool IsMenuOpen() const { return menuOpen.load(); }
	void SetMenuOpen(bool a_open);

	UIWindow* GetWindow(WindowID a_id);
	void ToggleWindow(WindowID a_id);
	void ShowWelcomeBanner();

	// Bethesda RE::InputEvent → ImGui key / mouse / char translation.
	// Ported from F4SE Menu Framework 3 (Input.cpp).  Call from a
	// BSInputEventHandler or wherever the engine's input event list is available.
	static void TranslateInputEvent(RE::InputEvent* const* a_event);

private:
	UIManager();

	void InitWindows();

	static void HookClipCursor();
	static void HookSetCursorPos();

	static HRESULT WINAPI HookedPresent(IDXGISwapChain* a_swapChain, UINT a_syncInterval, UINT a_flags);
	static LRESULT CALLBACK HookedWndProc(HWND a_hwnd, UINT a_msg, WPARAM a_wParam, LPARAM a_lParam);
	static BOOL WINAPI HookedClipCursor(const RECT* a_rect);
	static BOOL WINAPI HookedSetCursorPos(int X, int Y);

	static HRESULT WINAPI HookedD3D11CreateDeviceAndSwapChain(
		IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software,
		UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels,
		UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
		IDXGISwapChain** ppSwapChain, ID3D11Device** ppDevice,
		D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext);

	void InitImGui(IDXGISwapChain* a_swapChain);
	void RenderFrame();

	// ── State ──────────────────────────────────────────────────────────
	std::atomic<bool> initialized{ false };
	std::atomic<bool> menuOpen{ false };
	bool windowsInitialized{ false };

	std::vector<std::unique_ptr<UIWindow>> windows;

	HWND gameWindow{ nullptr };
	WNDPROC originalWndProc{ nullptr };

	// ── Present VTable hook ────────────────────────────────────────────
	using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
	static inline PresentFn OriginalPresent{ nullptr };

	// ── ClipCursor IAT hook ────────────────────────────────────────────
	using ClipCursorFn = BOOL(WINAPI*)(const RECT*);
	static inline ClipCursorFn OriginalClipCursor{ nullptr };
	static inline RECT savedWindowRect{};

	// ── SetCursorPos IAT hook ─────────────────────────────────────────
	using SetCursorPosFn = BOOL(WINAPI*)(int, int);
	static inline SetCursorPosFn OriginalSetCursorPos{ nullptr };

	// ── GameLock — 4-state model (matches F4SE Menu Framework 3) ──────
	//   None:     initial state before any menu interaction
	//   Locked:   main editor open — game input blocked, cursor visible
	//   Unlocked: all menus closed — game input restored, cursor hidden
	//   Resume:   non-blocking overlay active — game input restored, no cursor toggle
	enum class LockState { None, Locked, Unlocked, Resume };
	LockState previousLockState{ LockState::None };
	LockState currentLockState{ LockState::None };
	void UpdateLockState();

	// ── BSInputDevice::Poll VTable hooks ──────────────────────────────
	// Per-frame toggle of ImGui input flags, ported from F4SE Menu
	// Framework 3 (Hooks.cpp DevicePollHook).
	struct DevicePollHook {
		static void __fastcall KeyboardThunk(RE::BSInputDevice* a_device, float a_pollDelta);
		static void __fastcall MouseThunk(RE::BSInputDevice* a_device, float a_pollDelta);
		static inline void (*OriginalKeyboardPoll)(RE::BSInputDevice*, float){ nullptr };
		static inline void (*OriginalMousePoll)(RE::BSInputDevice*, float){ nullptr };
	};

	static void EnableImGuiInput();
	static void DisableImGuiInput();

	// ── D3D11 create hook ─────────────────────────────────────────────
	using D3D11CreateFn = HRESULT(WINAPI*)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
		const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*,
		IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
	static inline D3D11CreateFn OriginalD3D11Create{ nullptr };
};
