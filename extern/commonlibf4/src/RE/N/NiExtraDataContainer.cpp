#include "RE/N/NiExtraDataContainer.h"

#include "RE/N/NiExtraData.h"

namespace RE
{
	NiExtraDataContainer::NiExtraDataContainer(const std::uint32_t a_count) :
		extra(a_count)
	{}

	void NiExtraDataContainer::Add(NiExtraData* a_extra)
	{
		const BSAutoWriteLock l(lock);
		extra.push_back(a_extra);
	}

	NiExtraData* NiExtraDataContainer::FindExtra(const BSFixedString& a_key) const
	{
		const BSAutoReadLock l(lock);
		for (auto& entry : extra)
			if (entry->GetName() == a_key)
				return entry.get();

		return nullptr;
	}

	std::int32_t NiExtraDataContainer::FindIndex(const BSFixedString& a_key) const
	{
		const BSAutoReadLock l(lock);
		for (auto i = 0u; i < extra.size(); i++)
			if (extra[i]->GetName() == a_key)
				return i;

		return -1;
	}

	std::uint32_t NiExtraDataContainer::GetSize() const
	{
		const BSAutoReadLock l(lock);
		return extra.size();
	}

	bool NiExtraDataContainer::RemoveExtra(const BSFixedString& a_key)
	{
		const BSAutoWriteLock l(lock);

		const auto it = std::find_if(extra.begin(), extra.end(), [&](auto& entry) {
			return entry->GetName() == a_key;
		});

		if (it != extra.end()) {
			extra.erase(it);
			return true;
		}

		return false;
	}
}
