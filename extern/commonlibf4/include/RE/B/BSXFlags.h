#pragma once

#include "RE/N/NiIntegerExtraData.h"
#include "RE/N/NiObjectNET.h"

namespace RE
{
	class __declspec(novtable) BSXFlags :
		public NiIntegerExtraData
	{
	public:
		static constexpr auto RTTI{ RTTI::BSXFlags };
		static constexpr auto VTABLE{ VTABLE::BSXFlags };
		static constexpr auto Ni_RTTI{ Ni_RTTI::BSXFlags };

		enum class Flag : std::int32_t
		{
			kNone = 0,
			kAnimated = 1 << 0,
			kHavok = 1 << 1,
			kRagdoll = 1 << 2,
			kComplex = 1 << 3,
			kAddon = 1 << 4,
			kEditorMarker = 1 << 5,
			kDynamic = 1 << 6,
			kArticulated = 1 << 7,
			kNeedsTransformUpdate = 1 << 8,
			kExternalEmit = 1 << 9,
			kMagicShaderParticles = 1 << 10,
			kLights = 1 << 11,
			kBreakable = 1 << 12,
			kSearchedBreakable = 1 << 13
		};

		BSXFlags() :
			NiIntegerExtraData("BSX", 0)
		{
			REX::EMPLACE_VTABLE(this);
		}

		BSXFlags(const REX::TEnumSet<Flag> a_value) :
			NiIntegerExtraData("BSX", a_value.underlying())
		{
			REX::EMPLACE_VTABLE(this);
		}

		REX::TEnumSet<Flag> GetFlags() const
		{
			return static_cast<Flag>(value);
		}

		void SetFlags(const REX::TEnumSet<Flag> a_value)
		{
			value = a_value.underlying();
		}

		static BSXFlags* Find(NiObjectNET* a_object)
		{
			return a_object ? a_object->GetExtraData<BSXFlags>("BSX") : nullptr;
		}
	};
	static_assert(sizeof(BSXFlags) == 0x20);
}
