#include "UI/UIAnimationQueue.h"
#include "OpenAnimationReplacer.h"
#include "Settings.h"

#include <imgui.h>

bool UIAnimationQueue::ShouldDraw() const
{
	if (!Settings::GetSingleton()->bEnableAnimationQueueProgressBar) return false;

	auto* oar = OpenAnimationReplacer::GetSingleton();
	if (oar->isLoading.load()) return true;
	if (lingerTimer > 0.f) return true;

	return false;
}

ImGuiWindowFlags UIAnimationQueue::GetWindowFlags() const
{
	return ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
	       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
	       ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
}

void UIAnimationQueue::DrawContents()
{
	auto* oar = OpenAnimationReplacer::GetSingleton();
	auto* settings = Settings::GetSingleton();

	int total = oar->loadingTotalAnims.load();
	int parsed = oar->loadingParsedAnims.load();
	int loaded = oar->loadingLoadedAnims.load();
	bool loading = oar->isLoading.load();

	if (loading) {
		lingerTimer = settings->fAnimationQueueLingerTime + kFadeTime;
	} else {
		lingerTimer -= ImGui::GetIO().DeltaTime;
	}

	constexpr float PAD = 10.0f;
	auto* viewport = ImGui::GetMainViewport();
	ImGui::SetWindowPos(ImVec2(
		viewport->WorkPos.x + viewport->WorkSize.x - ImGui::GetWindowWidth() - PAD,
		viewport->WorkPos.y + PAD));

	float alpha = 1.0f;
	if (!loading && lingerTimer < kFadeTime) {
		alpha = std::max(0.f, lingerTimer / kFadeTime);
	}

	ImGui::SetNextWindowBgAlpha(0.25f);
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

	const char* phaseText = oar->loadingPhase.c_str();
	float windowWidth = ImGui::GetWindowSize().x;
	float textWidth = ImGui::CalcTextSize(phaseText).x;
	ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
	ImGui::TextUnformatted(phaseText);

	float fraction = 0.f;
	std::string overlayStr;

	if (total > 0) {
		int progress = loaded > 0 ? loaded : parsed;
		fraction = static_cast<float>(progress) / static_cast<float>(total);
		overlayStr = std::format("{}/{}", progress, total);
	}

	ImGui::ProgressBar(fraction, ImVec2(220, 0), overlayStr.c_str());

	ImGui::PopStyleVar();
}
