#pragma once

#include "UI/UIWindow.h"
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

struct ImGuiContext;

class UIManager
{
public:
	static UIManager* GetSingleton()
	{
		static UIManager singleton;
		return &singleton;
	}

	static void InstallHooks();

	void ToggleMenu();
	bool IsMenuOpen() const { return menuOpen.load(); }
	void SetMenuOpen(bool a_open);

	UIWindow* GetWindow(WindowID a_id);
	void ToggleWindow(WindowID a_id);
	void ShowWelcomeBanner();

private:
	UIManager();

	void InitWindows();

	static void HookClipCursor();
	static void HookSetCursorPos();

	static HRESULT WINAPI HookedPresent(IDXGISwapChain* a_swapChain, UINT a_syncInterval, UINT a_flags);
	static LRESULT CALLBACK HookedWndProc(HWND a_hwnd, UINT a_msg, WPARAM a_wParam, LPARAM a_lParam);
	static BOOL WINAPI HookedClipCursor(const RECT* a_rect);
	static BOOL WINAPI HookedSetCursorPos(int a_x, int a_y);

	static HRESULT WINAPI HookedD3D11CreateDeviceAndSwapChain(
		IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software,
		UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels,
		UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
		IDXGISwapChain** ppSwapChain, ID3D11Device** ppDevice,
		D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext);

	void InitImGui(IDXGISwapChain* a_swapChain);
	void RenderFrame();

	std::atomic<bool> initialized{ false };
	std::atomic<bool> menuOpen{ false };
	bool windowsInitialized{ false };

	std::vector<std::unique_ptr<UIWindow>> windows;

	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mainRTV;
	HWND gameWindow{ nullptr };
	WNDPROC originalWndProc{ nullptr };

	using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
	static inline PresentFn OriginalPresent{ nullptr };

	using ClipCursorFn = BOOL(WINAPI*)(const RECT*);
	static inline ClipCursorFn OriginalClipCursor{ nullptr };

	using SetCursorPosFn = BOOL(WINAPI*)(int, int);
	static inline SetCursorPosFn OriginalSetCursorPos{ nullptr };

	static inline RECT savedWindowRect{};

	enum class LockState { None, Locked, Unlocked };
	LockState previousLockState{ LockState::None };
	LockState currentLockState{ LockState::None };
	void UpdateLockState();

	static inline bool s_consumeNextChar{ false };
	static inline uint32_t s_consumeKeyUpScanCode{ 0 };

	using D3D11CreateFn = HRESULT(WINAPI*)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
		const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*,
		IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
	static inline D3D11CreateFn OriginalD3D11Create{ nullptr };
};
