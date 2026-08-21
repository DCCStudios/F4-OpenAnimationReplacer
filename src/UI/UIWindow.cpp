#include "UI/UIWindow.h"

#include "Settings.h"
#include "UI/Localization.h"

namespace
{
	constexpr float kViewportMargin = 16.0f;

	float GetTextScale()
	{
		return static_cast<float>(Settings::GetSingleton()->iTextSizePercent) / 100.0f;
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

	const float textScale = GetTextScale();
	const ImGuiWindowFlags flags = GetWindowFlags();
	const ImVec2 defaultSize = GetDefaultSize();
	ImGui::SetNextWindowSize(
		ClampToViewport(ImVec2(defaultSize.x * textScale, defaultSize.y * textScale)),
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
	// changes text size so their effective content area remains consistent.
	if ((flags & ImGuiWindowFlags_AlwaysAutoResize) == 0 &&
		appliedTextScale > 0.0f &&
		std::abs(appliedTextScale - textScale) > 0.001f) {
		const float ratio = textScale / appliedTextScale;
		const ImVec2 currentSize = ImGui::GetWindowSize();
		ImGui::SetWindowSize(
			ClampToViewport(ImVec2(currentSize.x * ratio, currentSize.y * ratio)),
			ImGuiCond_Always);
	}
	appliedTextScale = textScale;

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
