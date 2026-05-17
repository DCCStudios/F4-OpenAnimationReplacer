#include "Variants.h"
#include "Utils.h"

void Variants::AddVariant(VariantEntry a_entry)
{
	entries.push_back(std::move(a_entry));
}

int32_t Variants::SelectIndex(uint32_t a_refrFormID, bool a_keepOnLoop, bool a_shareResults)
{
	if (entries.empty()) return -1;

	switch (mode) {
	case VariantMode::kRandom:
	{
		if (a_shareResults && sharedResult >= 0) {
			return sharedResult;
		}
		// Always return cached result if one exists (stable per-activation)
		{
			ReadLocker rlock(stateMutex);
			auto it = lastRandomResult.find(a_refrFormID);
			if (it != lastRandomResult.end()) {
				return it->second;
			}
		}
		// No cached result — roll a new one
		int32_t result = SelectRandomIndex();
		{
			WriteLocker wlock(stateMutex);
			lastRandomResult[a_refrFormID] = result;
			if (a_shareResults) sharedResult = result;
		}
		static std::atomic<int> s_rollLog{ 0 };
		int rc = s_rollLog.fetch_add(1);
		if (rc < 30) {
			logger::info("[OAR-Variant] Fresh roll for refr={:X}: index={} / {} entries",
				a_refrFormID, result, entries.size());
		}
		return result;
	}
	case VariantMode::kSequential:
	{
		// Return current sequential result without advancing
		{
			ReadLocker rlock(stateMutex);
			auto it = sequentialIndices.find(a_refrFormID);
			if (it != sequentialIndices.end() && it->second > 0) {
				return static_cast<int32_t>(it->second - 1);
			}
		}
		return SelectSequentialIndex(a_refrFormID);
	}
	default:
		return 0;
	}
}

int16_t Variants::SelectVariant(uint32_t a_refrFormID, bool a_keepOnLoop, bool a_shareResults)
{
	int32_t idx = SelectIndex(a_refrFormID, a_keepOnLoop, a_shareResults);
	if (idx < 0 || idx >= static_cast<int32_t>(entries.size())) return -1;
	return entries[idx].bindingIndex;
}

std::string Variants::SelectVariantSuffix(uint32_t a_refrFormID, bool a_keepOnLoop, bool a_shareResults)
{
	int32_t idx = SelectIndex(a_refrFormID, a_keepOnLoop, a_shareResults);
	if (idx < 0 || idx >= static_cast<int32_t>(entries.size())) return {};
	return entries[idx].cacheSuffix;
}

void Variants::ResetState(uint32_t a_refrFormID)
{
	WriteLocker lock(stateMutex);
	bool hadEntry = lastRandomResult.contains(a_refrFormID);
	sequentialIndices.erase(a_refrFormID);
	lastRandomResult.erase(a_refrFormID);
	static std::atomic<int> s_resetLog{ 0 };
	int rc = s_resetLog.fetch_add(1);
	if (rc < 30) {
		logger::info("[OAR-Variant] ResetState refr={:X} hadCachedResult={}", a_refrFormID, hadEntry);
	}
}

int32_t Variants::SelectRandomIndex_Fresh() const
{
	return SelectRandomIndex();
}

int32_t Variants::SelectRandomIndex() const
{
	if (entries.size() == 1) return 0;

	float totalWeight = 0.0f;
	for (const auto& e : entries) totalWeight += e.weight;
	if (totalWeight <= 0.0f) return 0;

	static thread_local std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> dist(0.0f, totalWeight);
	float roll = dist(rng);

	float cumulative = 0.0f;
	for (size_t i = 0; i < entries.size(); ++i) {
		cumulative += entries[i].weight;
		if (roll < cumulative) return static_cast<int32_t>(i);
	}
	return static_cast<int32_t>(entries.size() - 1);
}

int32_t Variants::SelectSequentialIndex(uint32_t a_refrFormID)
{
	WriteLocker lock(stateMutex);
	auto& idx = sequentialIndices[a_refrFormID];
	if (idx >= entries.size()) idx = 0;
	int32_t result = static_cast<int32_t>(idx);
	idx++;
	return result;
}
