#include "ActiveClip.h"
#include "OpenAnimationReplacer.h"
#include "ReplacementAnimation.h"
#include "ReplacerMods.h"
#include "AnimationLog.h"
#include "Settings.h"
#include "Variants.h"

// ===== BlendingClip =====

bool ActiveClip::BlendingClip::Update(float a_deltaTime)
{
	blendElapsedTime += a_deltaTime;
	clipGenerator.AdvanceTime(a_deltaTime);
	return blendElapsedTime >= blendDuration;
}

float ActiveClip::BlendingClip::GetBlendWeight() const
{
	if (blendDuration <= 0.f) return 0.f;
	return std::max(1.f - (blendElapsedTime / blendDuration), 0.f);
}

// ===== ActiveClip =====

ActiveClip::ActiveClip(RE::hkbClipGenerator* a_clipGen, int16_t a_originalIndex, RE::TESObjectREFR* a_refr)
	: clipGenerator(a_clipGen)
	, originalBindingIndex(a_originalIndex)
	, refr(a_refr)
{
	if (a_clipGen) {
		originalMode = a_clipGen->mode;
		originalFlags = a_clipGen->flags;
	}
}

ActiveClip::~ActiveClip()
{
	triggersBackup._ptr = nullptr;
	triggersBackedUp = false;
	blendingClips.clear();
}

void ActiveClip::OnActivate()
{
	isActive = true;
	EvaluateAndApplyReplacement();

	if (currentReplacement && Settings::GetSingleton()->bLogReplace) {
		AnimationLog::GetSingleton()->AddEntry(
			AnimationLog::EventType::kReplace,
			refr,
			clipGenerator->animationName.data(),
			currentReplacement->GetReplacementPath(),
			currentReplacement->GetParentSubMod() ? currentReplacement->GetParentSubMod()->GetName() : ""
		);
	}
}

void ActiveClip::OnUpdate()
{
	if (!isActive) return;
	if (!currentReplacement) return;

	auto* subMod = currentReplacement->GetParentSubMod();
	if (!subMod) return;

	if (subMod->IsInterruptible()) {
		EvaluateAndApplyReplacement();
	} else {
		// Even if not interruptible, check if the current replacement's own conditions
		// have become false - if so, restore original
		if (subMod->IsDisabled() || !subMod->EvaluateConditions(refr, clipGenerator)) {
			RestoreOriginalIndex();
			currentReplacement = nullptr;
		}
	}
}

void ActiveClip::PreUpdate(float a_timestep)
{
	auto it = blendingClips.begin();
	while (it != blendingClips.end()) {
		if ((*it)->Update(a_timestep)) {
			it = blendingClips.erase(it);
		} else {
			++it;
		}
	}

	if (queuedReplacement.has_value()) {
		ReplaceActiveAnimation(queuedReplacement->replacementAnimation);
		queuedReplacement.reset();
	}
}

void ActiveClip::OnDeactivate()
{
	RestoreOriginalIndex();
	isActive = false;
	currentReplacement = nullptr;
	blendingClips.clear();
	queuedReplacement.reset();
}

void ActiveClip::OnGenerate()
{
	// Output blending is handled by the Generate hook using
	// the blending clips' captured pose data.
}

void ActiveClip::OnStartEcho()
{
	if (!currentReplacement) return;

	if (ShouldReplaceOnEcho()) {
		EvaluateAndApplyReplacement();

		if (Settings::GetSingleton()->bLogEcho) {
			AnimationLog::GetSingleton()->AddEntry(
				AnimationLog::EventType::kEcho,
				refr,
				clipGenerator->animationName.data(),
				currentReplacement ? currentReplacement->GetReplacementPath() : "",
				currentReplacement && currentReplacement->GetParentSubMod()
					? currentReplacement->GetParentSubMod()->GetName() : ""
			);
		}
	}
}

void ActiveClip::StartBlend(float a_blendDuration)
{
	if (!clipGenerator || a_blendDuration <= 0.f) return;

	auto blendClip = std::make_unique<BlendingClip>(clipGenerator, a_blendDuration);
	blendingClips.push_back(std::move(blendClip));

	constexpr size_t kMaxConcurrentBlends = 4;
	while (blendingClips.size() > kMaxConcurrentBlends) {
		blendingClips.pop_front();
	}
}

void ActiveClip::ReplaceActiveAnimation(ReplacementAnimation* a_newRepl)
{
	if (!clipGenerator || !refr) return;

	float blendTime = 0.2f;
	if (a_newRepl) {
		auto* subMod = a_newRepl->GetParentSubMod();
		if (subMod && subMod->GetCustomBlendTimeOnInterrupt() >= 0.f) {
			blendTime = subMod->GetCustomBlendTimeOnInterrupt();
		}
	}
	StartBlend(blendTime);

	RestoreOriginalIndex();
	currentReplacement = a_newRepl;

	if (currentReplacement) {
		int16_t newIndex = currentReplacement->GetBindingIndex();

		if (currentReplacement->HasVariants()) {
			auto* subMod = currentReplacement->GetParentSubMod();
			bool keepOnLoop = subMod ? subMod->GetKeepRandomResultsOnLoop() : false;
			bool shareResults = subMod ? subMod->GetShareRandomResults() : false;
			newIndex = currentReplacement->GetVariants()->SelectVariant(refr->GetFormID(), keepOnLoop, shareResults);
		}

		if (newIndex >= 0) {
			clipGenerator->animationBindingIndex = newIndex;
		}
	}

	transitioning = true;
}

void ActiveClip::QueueReplacement(ReplacementAnimation* a_repl, float a_blendTime, QueuedReplacement::Type a_type)
{
	queuedReplacement.emplace(a_repl, a_blendTime, a_type);
}

ReplacementAnimation* ActiveClip::PopQueuedReplacement()
{
	if (!queuedReplacement.has_value()) return nullptr;
	auto* repl = queuedReplacement->replacementAnimation;
	queuedReplacement.reset();
	return repl;
}

bool ActiveClip::IsInterruptible() const
{
	if (!currentReplacement) return true;
	auto* subMod = currentReplacement->GetParentSubMod();
	return subMod ? subMod->IsInterruptible() : true;
}

bool ActiveClip::ShouldReplaceOnLoop() const
{
	if (!currentReplacement) return true;
	auto* subMod = currentReplacement->GetParentSubMod();
	return subMod ? subMod->GetReplaceOnLoop() : true;
}

bool ActiveClip::ShouldReplaceOnEcho() const
{
	if (!currentReplacement) return true;
	auto* subMod = currentReplacement->GetParentSubMod();
	return subMod ? subMod->GetReplaceOnEcho() : true;
}

void ActiveClip::BackupTriggers()
{
	if (!clipGenerator || triggersBackedUp) return;
	triggersBackup = clipGenerator->triggers;
	triggersBackedUp = true;
}

void ActiveClip::RestoreTriggers()
{
	if (!clipGenerator || !triggersBackedUp) return;
	clipGenerator->triggers = triggersBackup;
	triggersBackup._ptr = nullptr;
	triggersBackedUp = false;
}

RE::hkbClipGenerator* ActiveClip::GetLastBlendingClipGenerator() const
{
	if (blendingClips.empty()) return nullptr;
	return blendingClips.back()->clipGenerator.AsClipGenerator();
}

void ActiveClip::EvaluateAndApplyReplacement()
{
	if (!clipGenerator || !refr) return;

	auto* oar = OpenAnimationReplacer::GetSingleton();
	auto* replacement = oar->GetReplacementAnimation(clipGenerator, originalBindingIndex, refr);

	if (replacement != currentReplacement) {
		if (currentReplacement && isActive) {
			float blendOut = 0.2f;
			auto* curSubMod = currentReplacement->GetParentSubMod();
			if (curSubMod && curSubMod->GetCustomBlendTimeOnInterrupt() >= 0.f) {
				blendOut = curSubMod->GetCustomBlendTimeOnInterrupt();
			}
			StartBlend(blendOut);
		}

		if (currentReplacement) {
			RestoreOriginalIndex();
		}

		currentReplacement = replacement;

		if (currentReplacement) {
			int16_t newIndex = currentReplacement->GetBindingIndex();

			if (currentReplacement->HasVariants()) {
				auto* subMod = currentReplacement->GetParentSubMod();
				bool keepOnLoop = subMod ? subMod->GetKeepRandomResultsOnLoop() : false;
				bool shareResults = subMod ? subMod->GetShareRandomResults() : false;
				newIndex = currentReplacement->GetVariants()->SelectVariant(refr->GetFormID(), keepOnLoop, shareResults);
			}

			if (newIndex >= 0) {
				clipGenerator->animationBindingIndex = newIndex;
			}
		}
	}
}

void ActiveClip::RestoreOriginalIndex()
{
	if (clipGenerator && originalBindingIndex >= 0) {
		clipGenerator->animationBindingIndex = originalBindingIndex;
	}
}

// ===== ActiveClipManager =====

ActiveClip* ActiveClipManager::GetOrCreateActiveClip(RE::hkbClipGenerator* a_clipGen, RE::TESObjectREFR* a_refr)
{
	{
		ReadLocker lock(mutex);
		auto it = activeClips.find(a_clipGen);
		if (it != activeClips.end()) return it->second.get();
	}

	WriteLocker lock(mutex);
	auto& clip = activeClips[a_clipGen];
	if (!clip) {
		clip = std::make_unique<ActiveClip>(a_clipGen, a_clipGen->animationBindingIndex, a_refr);
	}
	return clip.get();
}

ActiveClip* ActiveClipManager::GetActiveClip(RE::hkbClipGenerator* a_clipGen)
{
	ReadLocker lock(mutex);
	auto it = activeClips.find(a_clipGen);
	return it != activeClips.end() ? it->second.get() : nullptr;
}

void ActiveClipManager::RemoveActiveClip(RE::hkbClipGenerator* a_clipGen)
{
	WriteLocker lock(mutex);
	activeClips.erase(a_clipGen);
}

void ActiveClipManager::ClearAll()
{
	WriteLocker lock(mutex);
	activeClips.clear();
}
