#pragma once

#include "FakeClipGenerator.h"
#include "AnimationLog.h"

class ReplacementAnimation;
class SubMod;

class ActiveClip
{
public:
	struct QueuedReplacement
	{
		enum class Type
		{
			kRestart,
			kContinue,
			kLoop
		};

		QueuedReplacement(ReplacementAnimation* a_repl, float a_blendTime, Type a_type)
			: replacementAnimation(a_repl), blendTime(a_blendTime), type(a_type) {}

		ReplacementAnimation* replacementAnimation;
		float blendTime;
		Type type;
	};

	struct BlendingClip
	{
		BlendingClip(RE::hkbClipGenerator* a_clipGenerator, float a_blendDuration)
			: clipGenerator(a_clipGenerator), blendDuration(a_blendDuration) {}

		bool Update(float a_deltaTime);
		float GetBlendWeight() const;

		FakeClipGenerator clipGenerator;
		float blendDuration{ 0.f };
		float blendElapsedTime{ 0.f };
	};

	ActiveClip(RE::hkbClipGenerator* a_clipGen, int16_t a_originalIndex, RE::TESObjectREFR* a_refr);
	~ActiveClip();

	void OnActivate();
	void OnUpdate();
	void OnDeactivate();
	void OnGenerate();
	void OnStartEcho();
	void PreUpdate(float a_timestep);

	bool HasReplacement() const { return currentReplacement != nullptr; }
	ReplacementAnimation* GetReplacement() const { return currentReplacement; }
	int16_t GetOriginalBindingIndex() const { return originalBindingIndex; }

	RE::hkbClipGenerator* GetClipGenerator() const { return clipGenerator; }
	RE::TESObjectREFR* GetRefr() const { return refr; }

	bool IsBlending() const { return !blendingClips.empty(); }
	RE::hkbClipGenerator* GetLastBlendingClipGenerator() const;

	void StartBlend(float a_blendDuration);
	void ReplaceActiveAnimation(ReplacementAnimation* a_newRepl);

	void QueueReplacement(ReplacementAnimation* a_repl, float a_blendTime, QueuedReplacement::Type a_type);
	ReplacementAnimation* PopQueuedReplacement();
	bool HasQueuedReplacement() const { return queuedReplacement.has_value(); }

	bool IsInterruptible() const;
	bool ShouldReplaceOnLoop() const;
	bool ShouldReplaceOnEcho() const;

	void BackupTriggers();
	void RestoreTriggers();

private:
	void EvaluateAndApplyReplacement();
	void RestoreOriginalIndex();

	RE::hkbClipGenerator* clipGenerator{ nullptr };
	RE::TESObjectREFR* refr{ nullptr };
	int16_t originalBindingIndex{ -1 };
	RE::hkbClipGenerator_PlaybackMode originalMode{ RE::MODE_SINGLE_PLAY };
	int8_t originalFlags{ 0 };
	ReplacementAnimation* currentReplacement{ nullptr };
	bool isActive{ false };

	std::optional<QueuedReplacement> queuedReplacement;
	std::deque<std::unique_ptr<BlendingClip>> blendingClips;
	bool transitioning{ false };

	RE::hkRefPtr<RE::hkbClipTriggerArray> triggersBackup;
	bool triggersBackedUp{ false };
};

class ActiveClipManager
{
public:
	static ActiveClipManager* GetSingleton()
	{
		static ActiveClipManager singleton;
		return &singleton;
	}

	ActiveClip* GetOrCreateActiveClip(RE::hkbClipGenerator* a_clipGen, RE::TESObjectREFR* a_refr);
	ActiveClip* GetActiveClip(RE::hkbClipGenerator* a_clipGen);
	void RemoveActiveClip(RE::hkbClipGenerator* a_clipGen);
	void ClearAll();

private:
	using ReadLocker = std::shared_lock<std::shared_mutex>;
	using WriteLocker = std::unique_lock<std::shared_mutex>;

	std::shared_mutex mutex;
	std::unordered_map<RE::hkbClipGenerator*, std::unique_ptr<ActiveClip>> activeClips;
};
