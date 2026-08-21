#include "UI/UIWindow.h"

#include "UI/Localization.h"
#include "UI/UIManager.h"

namespace
{
	constexpr float kViewportMargin = 16.0f;

	float GetEffectiveUIScale()
	{
		return UIManager::GetSingleton()->GetEffectiveUIScale();
	}

	// A compensating resize can exceed the game viewport at high text scales.
	// Keep the window inside the usable area while retaining as much content
	// space as the current resolution allows.
	ImVec2 ClampToViewport(ImVec2 a_size)
	{
		if (const auto* viewport = ImGui::GetMainViewport()) {
			a_size.x = std::min(a_size.x, std::max(1.0f, viewport->WorkSize.x - kViewportMargin));
			a_size.y = std::min(a_size.y, std::max(1.0f, viewport->WorkSize.y - kViewportMargin));
		}
		return a_size;
	}
}

void UIWindow::TryDraw()
{
	if (!ShouldDraw()) return;

	const float uiScale = GetEffectiveUIScale();
	const ImGuiWindowFlags flags = GetWindowFlags();
	const ImVec2 defaultSize = GetDefaultSize();
	ImGui::SetNextWindowSize(
		ClampToViewport(ImVec2(defaultSize.x * uiScale, defaultSize.y * uiScale)),
		ImGuiCond_FirstUseEver);

	bool open = isOpen;
	std::string localizedTitle;
	const char* windowLabel = title;
	if (title && !std::string_view(title).starts_with("##")) {
		localizedTitle = UICommon::T(title);
		localizedTitle += "###OARWindow";
		localizedTitle += std::to_string(static_cast<int32_t>(windowID));
		windowLabel = localizedTitle.c_str();
	}
	const bool visible = ImGui::Begin(windowLabel, &open, flags);

	// Always-auto-resize overlays already follow their scaled text content.
	// Normal editor windows need an explicit proportional resize when the user
	// changes text size or monitor DPI so their effective content area remains consistent.
	if ((flags & ImGuiWindowFlags_AlwaysAutoResize) == 0 &&
		appliedUIScale > 0.0f &&
		std::abs(appliedUIScale - uiScale) > 0.001f) {
		const float ratio = uiScale / appliedUIScale;
		const ImVec2 currentSize = ImGui::GetWindowSize();
		ImGui::SetWindowSize(
			ClampToViewport(ImVec2(currentSize.x * ratio, currentSize.y * ratio)),
			ImGuiCond_Always);
	}
	appliedUIScale = uiScale;

	if (!visible) {
		ImGui::End();
		if (!open && isOpen) SetOpen(false);
		return;
	}

	if (!open && isOpen) {
		SetOpen(false);
	}

	DrawContents();
	ImGui::End();
}

void UIWindow::SetOpen(bool a_open)
{
	if (isOpen == a_open) return;
	isOpen = a_open;
	if (a_open) OnOpen();
	else OnClose();
}
