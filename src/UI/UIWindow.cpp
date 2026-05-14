#include "UI/UIWindow.h"

void UIWindow::TryDraw()
{
	if (!ShouldDraw()) return;

	ImGui::SetNextWindowSize(GetDefaultSize(), ImGuiCond_FirstUseEver);

	bool open = isOpen;
	if (!ImGui::Begin(title, &open, GetWindowFlags())) {
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
