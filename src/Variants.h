#pragma once

class ReplacementAnimation;

enum class VariantMode : int32_t
{
	kRandom = 0,
	kSequential,
};

struct VariantEntry
{
	std::string filename;
	int16_t bindingIndex{ -1 };
	float weight{ 1.0f };
	bool playOnce{ false };
};

class Variants
{
public:
	void AddVariant(VariantEntry a_entry);
	int16_t SelectVariant(uint32_t a_refrFormID);
	void ResetState(uint32_t a_refrFormID);

	VariantMode GetMode() const { return mode; }
	void SetMode(VariantMode a_mode) { mode = a_mode; }
	const std::vector<VariantEntry>& GetEntries() const { return entries; }

private:
	int16_t SelectRandom() const;
	int16_t SelectSequential(uint32_t a_refrFormID);

	VariantMode mode{ VariantMode::kRandom };
	std::vector<VariantEntry> entries;

	mutable std::shared_mutex stateMutex;
	std::unordered_map<uint32_t, uint32_t> sequentialIndices;
};
