#pragma once

class ReplacementAnimation;

enum class VariantMode : int32_t
{
	kRandom = 0,
	kSequential,
};

enum class VariantRerollPolicy : int32_t
{
	kOnEachPlay = 0,
	kWhileActive,
};

struct VariantEntry
{
	std::string filename;
	std::string cacheSuffix;
	int16_t bindingIndex{ -1 };
	float weight{ 1.0f };
	bool playOnce{ false };
};

class Variants
{
public:
	void AddVariant(VariantEntry a_entry);
	int16_t SelectVariant(uint32_t a_refrFormID, bool a_keepOnLoop = false, bool a_shareResults = false);
	std::string SelectVariantSuffix(uint32_t a_refrFormID, bool a_keepOnLoop = false, bool a_shareResults = false);
	void ResetState(uint32_t a_refrFormID);

	VariantMode GetMode() const { return mode; }
	void SetMode(VariantMode a_mode) { mode = a_mode; }
	const std::vector<VariantEntry>& GetEntries() const { return entries; }
	std::vector<VariantEntry>& GetEntriesMutable() { return entries; }
	size_t GetCount() const { return entries.size(); }

	int32_t SelectIndex(uint32_t a_refrFormID, bool a_keepOnLoop = false, bool a_shareResults = false);
	int32_t SelectRandomIndex_Fresh() const;

private:
	int32_t SelectRandomIndex() const;
	int32_t SelectSequentialIndex(uint32_t a_refrFormID);

	VariantMode mode{ VariantMode::kRandom };
	std::vector<VariantEntry> entries;

	mutable std::shared_mutex stateMutex;
	std::unordered_map<uint32_t, uint32_t> sequentialIndices;
	std::unordered_map<uint32_t, int32_t> lastRandomResult;
	int32_t sharedResult{ -1 };
};
