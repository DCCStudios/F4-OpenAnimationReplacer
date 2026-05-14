#pragma once

#include "HavokTypes.h"

class FakeClipGenerator
{
public:
	static constexpr size_t kBufferSize = 0x180;

	FakeClipGenerator(RE::hkbClipGenerator* a_clipGenerator);
	FakeClipGenerator(RE::hkbClipGenerator* a_clipGenerator, bool a_bCopyTriggers);
	~FakeClipGenerator() = default;

	RE::hkbClipGenerator* AsClipGenerator() { return reinterpret_cast<RE::hkbClipGenerator*>(buffer); }
	const RE::hkbClipGenerator* AsClipGenerator() const { return reinterpret_cast<const RE::hkbClipGenerator*>(buffer); }

	int16_t GetOriginalBindingIndex() const { return originalBindingIndex; }
	float GetLocalTime() const { return localTime; }
	float GetPlaybackSpeed() const { return playbackSpeed; }

	void SetLocalTime(float a_time) { localTime = a_time; }
	void AdvanceTime(float a_deltaTime);

	bool IsAtEnd() const { return atEnd; }

private:
	void CaptureState(RE::hkbClipGenerator* a_src);

	alignas(16) uint8_t buffer[kBufferSize]{};

	int16_t originalBindingIndex{ -1 };
	float localTime{ 0.f };
	float playbackSpeed{ 1.f };
	float cropStartAmountLocalTime{ 0.f };
	float cropEndAmountLocalTime{ 0.f };
	float startTime{ 0.f };
	float enforcedDuration{ 0.f };
	float userControlledTimeFraction{ 0.f };
	RE::hkbClipGenerator_PlaybackMode mode{ RE::MODE_SINGLE_PLAY };
	int8_t flags{ 0 };
	bool atEnd{ false };
	bool pingPongBackward{ false };
};
