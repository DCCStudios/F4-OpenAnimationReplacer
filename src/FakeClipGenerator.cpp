#include "FakeClipGenerator.h"

FakeClipGenerator::FakeClipGenerator(RE::hkbClipGenerator* a_clipGenerator)
{
	CaptureState(a_clipGenerator);
}

FakeClipGenerator::FakeClipGenerator(RE::hkbClipGenerator* a_clipGenerator, bool a_bCopyTriggers)
{
	CaptureState(a_clipGenerator);
	if (!a_bCopyTriggers) {
		auto* fake = AsClipGenerator();
		fake->triggers._ptr = nullptr;
	}
}

void FakeClipGenerator::CaptureState(RE::hkbClipGenerator* a_src)
{
	if (!a_src) return;

	memcpy(buffer, a_src, std::min(sizeof(buffer), static_cast<size_t>(kBufferSize)));

	// Null the vtable to prevent accidental virtual calls on our copy
	*reinterpret_cast<uintptr_t*>(buffer) = 0;

	auto* src = a_src;
	originalBindingIndex = src->animationBindingIndex;
	localTime = 0.f;
	playbackSpeed = src->playbackSpeed;
	cropStartAmountLocalTime = src->cropStartAmountLocalTime;
	cropEndAmountLocalTime = src->cropEndAmountLocalTime;
	startTime = src->startTime;
	enforcedDuration = src->enforcedDuration;
	userControlledTimeFraction = src->userControlledTimeFraction;
	mode = src->mode;
	flags = src->flags;
	atEnd = false;
	pingPongBackward = false;
}

void FakeClipGenerator::AdvanceTime(float a_deltaTime)
{
	float effectiveDelta = a_deltaTime * playbackSpeed;

	if (mode == RE::MODE_SINGLE_PLAY) {
		localTime += effectiveDelta;
		if (enforcedDuration > 0.f && localTime >= enforcedDuration) {
			localTime = enforcedDuration;
			atEnd = true;
		}
	} else if (mode == RE::MODE_LOOPING) {
		localTime += effectiveDelta;
		if (enforcedDuration > 0.f) {
			while (localTime >= enforcedDuration) {
				localTime -= enforcedDuration;
			}
		}
	} else if (mode == RE::MODE_PING_PONG) {
		if (pingPongBackward) {
			localTime -= effectiveDelta;
			if (localTime <= 0.f) {
				localTime = -localTime;
				pingPongBackward = false;
			}
		} else {
			localTime += effectiveDelta;
			if (enforcedDuration > 0.f && localTime >= enforcedDuration) {
				localTime = 2.f * enforcedDuration - localTime;
				pingPongBackward = true;
			}
		}
	}
}
