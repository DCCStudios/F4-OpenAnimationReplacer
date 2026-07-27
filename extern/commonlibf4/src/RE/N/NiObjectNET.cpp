#include "RE/N/NiObjectNET.h"

#include "RE/N/NiExtraData.h"
#include "RE/N/NiRTTI.h"
#include "RE/N/NiTimeController.h"

namespace RE
{
	NiObjectNET::NiObjectNET()
	{
		REX::EMPLACE_VTABLE(this);
	}

	NiObjectNET::~NiObjectNET() = default;

	bool NiObjectNET::AddExtraData(const BSFixedString& a_key, NiExtraData* a_extra)
	{
		if (a_key.empty())
			return false;

		if (!a_extra)
			return false;

		if (!a_extra->GetName().empty())
			a_extra->SetName(a_key);
		else if (a_key != a_extra->GetName())
			return false;

		return InsertExtraData(a_extra);
	}

	bool NiObjectNET::AddExtraData(NiExtraData* a_extra)
	{
		if (!a_extra)
			return false;

		const auto& extraName = a_extra->GetName();
		if (extraName.empty()) {
			// TODO: Game handles nameless ExtraData by using the rtti name and a suffix?
			assert(false);
		}

		return InsertExtraData(a_extra);
	}

	NiExtraData* NiObjectNET::GetExtraData(const BSFixedString& a_key) const
	{
		return extra ? extra->FindExtra(a_key) : nullptr;
	}

	std::uint16_t NiObjectNET::GetExtraDataSize() const
	{
		return extra ? static_cast<std::uint16_t>(extra->GetSize()) : 0;
	}

	bool NiObjectNET::HasExtraData(const BSFixedString& a_key) const
	{
		return extra ? extra->FindExtra(a_key) : false;
	}

	bool NiObjectNET::InsertExtraData(NiExtraData* a_extra)
	{
		if (extra) {
			extra->Add(a_extra);
			return true;
		}

		return false;
	}

	bool NiObjectNET::RemoveExtraData(const BSFixedString& a_key)
	{
		if (a_key.empty())
			return false;

		return extra && extra->RemoveExtra(a_key);
	}
}
