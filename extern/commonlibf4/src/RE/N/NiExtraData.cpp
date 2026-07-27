#include "RE/N/NiExtraData.h"

namespace RE
{
	NiExtraData::NiExtraData()
	{
		REX::EMPLACE_VTABLE(this);
	}

	NiExtraData::NiExtraData(const BSFixedString& a_name) :
		name(a_name)
	{
		REX::EMPLACE_VTABLE(this);
	}

	NiExtraData::~NiExtraData() = default;

	const BSFixedString& NiExtraData::GetName() const
	{
		return name;
	}

	void NiExtraData::SetName(const BSFixedString& a_name)
	{
		name = a_name;
	}
}
