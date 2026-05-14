#include "Variants.h"
#include "Utils.h"

void Variants::AddVariant(VariantEntry a_entry)
{
	entries.push_back(std::move(a_entry));
}

int16_t Variants::SelectVariant(uint32_t a_refrFormID)
{
	if (entries.empty()) return -1;

	switch (mode) {
	case VariantMode::kRandom:
		return SelectRandom();
	case VariantMode::kSequential:
		return SelectSequential(a_refrFormID);
	default:
		return entries[0].bindingIndex;
	}
}

void Variants::ResetState(uint32_t a_refrFormID)
{
	WriteLocker lock(stateMutex);
	sequentialIndices.erase(a_refrFormID);
}

int16_t Variants::SelectRandom() const
{
	if (entries.size() == 1) return entries[0].bindingIndex;

	float totalWeight = 0.0f;
	for (const auto& e : entries) totalWeight += e.weight;
	if (totalWeight <= 0.0f) return entries[0].bindingIndex;

	static thread_local std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> dist(0.0f, totalWeight);
	float roll = dist(rng);

	float cumulative = 0.0f;
	for (const auto& e : entries) {
		cumulative += e.weight;
		if (roll < cumulative) return e.bindingIndex;
	}
	return entries.back().bindingIndex;
}

int16_t Variants::SelectSequential(uint32_t a_refrFormID)
{
	WriteLocker lock(stateMutex);
	auto& idx = sequentialIndices[a_refrFormID];
	if (idx >= entries.size()) idx = 0;
	auto result = entries[idx].bindingIndex;
	idx++;
	return result;
}
