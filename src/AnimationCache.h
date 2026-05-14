#pragma once

#include "HavokTypes.h"
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <string>
#include <filesystem>

class AnimationCache
{
public:
	static AnimationCache* GetSingleton()
	{
		static AnimationCache singleton;
		return &singleton;
	}

	struct ParsedAnnotation
	{
		float time{ 0.f };
		std::string text;
	};

	struct CachedAnimation
	{
		RE::hkaAnimation* animation{ nullptr };
		std::vector<uint8_t> fileData;
		std::string filePath;
		float duration{ 0.f };
		int32_t numTransformTracks{ 0 };
		int32_t numFloatTracks{ 0 };
		std::vector<uint32_t> vtableFixupOffsets;
		uint32_t sectionFileOffset{ 0 };
		std::unique_ptr<uint32_t[]> computedTransformOffsets;
		std::unique_ptr<uint32_t[]> computedFloatOffsets;

		std::vector<ParsedAnnotation> annotations;

		std::vector<uint8_t> runtimeStruct;
		RE::hkaAnimation* runtimeAnimation{ nullptr };
		RE::hkaAnimation* gameOriginal{ nullptr };
	};

	bool LoadAnimation(const std::string& a_suffix, const std::filesystem::path& a_absolutePath);
	RE::hkaAnimation* GetCachedAnimation(const std::string& a_suffix) const;
	RE::hkaAnimation* GetOrBuildRuntimeAnim(const std::string& a_suffix, RE::hkaAnimation* a_gameAnim);
	const std::vector<ParsedAnnotation>* GetAnnotations(const std::string& a_suffix) const;
	void SetVtableFromGame(uintptr_t a_vtable);
	uintptr_t GetGameAnimVtable() const { return m_gameAnimVtable.load(); }
	size_t GetCacheSize() const;
	bool IsOurReplacement(RE::hkaAnimation* a_anim) const;
	RE::hkaAnimation* GetOriginalFromReplacement(RE::hkaAnimation* a_replacement) const;
	void InvalidateRuntimeClones();
	void Clear();

private:
	AnimationCache() = default;

	bool ParsePackfile(CachedAnimation& a_entry);
	bool ParseTagfile(CachedAnimation& a_entry);
	RE::hkaAnimation* FindAnimationInBuffer(uint8_t* a_data, size_t a_size, uintptr_t a_vtable);
	static void ComputeSplineOffsets(uint8_t* a_animBytes, CachedAnimation& a_entry);

	mutable std::shared_mutex m_mutex;
	std::unordered_map<std::string, std::unique_ptr<CachedAnimation>> m_cache;
	std::atomic<uintptr_t> m_gameAnimVtable{ 0 };
};
