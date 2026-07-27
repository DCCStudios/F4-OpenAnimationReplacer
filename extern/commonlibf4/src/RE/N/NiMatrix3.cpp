#include "RE/N/NiMatrix3.h"

namespace RE
{
	bool NiMatrix3::ToEulerAnglesXYZ(NiPoint3& a_point) const
	{
		return ToEulerAnglesXYZ(a_point.x, a_point.y, a_point.z);
	}

	bool NiMatrix3::ToEulerAnglesXYZ(float& a_x, float& a_y, float& a_z) const
	{
		using func_t = bool (*)(const NiMatrix3*, float&, float&, float&);
		static REL::Relocation<func_t> func{ ID::NiMatrix3::ToEulerAnglesXYZ };
		return func(this, a_x, a_y, a_z);
	}

	bool NiMatrix3::ToEulerAnglesXZY(float& a_x, float& a_z, float& a_y) const
	{
		using func_t = bool (*)(const NiMatrix3*, float&, float&, float&);
		static REL::Relocation<func_t> func{ ID::NiMatrix3::ToEulerAnglesXZY };
		return func(this, a_x, a_z, a_y);
	}

	bool NiMatrix3::ToEulerAnglesYXZ(float& a_y, float& a_x, float& a_z) const
	{
		using func_t = bool (*)(const NiMatrix3*, float&, float&, float&);
		static REL::Relocation<func_t> func{ ID::NiMatrix3::ToEulerAnglesYXZ };
		return func(this, a_y, a_x, a_z);
	}

	bool NiMatrix3::ToEulerAnglesYZX(float& a_y, float& a_z, float& a_x) const
	{
		using func_t = bool (*)(const NiMatrix3*, float&, float&, float&);
		static REL::Relocation<func_t> func{ ID::NiMatrix3::ToEulerAnglesYZX };
		return func(this, a_y, a_z, a_x);
	}

	bool NiMatrix3::ToEulerAnglesZYX(float& a_z, float& a_y, float& a_x) const
	{
		using func_t = bool (*)(const NiMatrix3*, float&, float&, float&);
		static REL::Relocation<func_t> func{ ID::NiMatrix3::ToEulerAnglesZYX };
		return func(this, a_z, a_y, a_x);
	}

	bool NiMatrix3::ToEulerAnglesZXY(float& a_z, float& a_x, float& a_y) const
	{
		using func_t = bool (*)(const NiMatrix3*, float&, float&, float&);
		static REL::Relocation<func_t> func{ ID::NiMatrix3::ToEulerAnglesZXY };
		return func(this, a_z, a_x, a_y);
	}

	void NiMatrix3::FromEulerAnglesXYZ(const NiPoint3& a_point)
	{
		FromEulerAnglesXYZ(a_point.x, a_point.y, a_point.z);
	}

	void NiMatrix3::FromEulerAnglesXYZ(float a_x, float a_y, float a_z)
	{
		using func_t = void (*)(NiMatrix3*, float, float, float);
		static REL::Relocation<func_t> func{ ID::NiMatrix3::FromEulerAnglesXYZ };
		return func(this, a_x, a_y, a_z);
	}

	void NiMatrix3::FromEulerAnglesXZY(float a_x, float a_z, float a_y)
	{
		using func_t = void (*)(NiMatrix3*, float, float, float);
		static REL::Relocation<func_t> func{ ID::NiMatrix3::FromEulerAnglesXZY };
		return func(this, a_x, a_z, a_y);
	}

	void NiMatrix3::FromEulerAnglesYXZ(float a_y, float a_x, float a_z)
	{
		using func_t = void (*)(NiMatrix3*, float, float, float);
		static REL::Relocation<func_t> func{ ID::NiMatrix3::FromEulerAnglesYXZ };
		return func(this, a_y, a_x, a_z);
	}

	void NiMatrix3::FromEulerAnglesYZX(float a_y, float a_z, float a_x)
	{
		using func_t = void (*)(NiMatrix3*, float, float, float);
		static REL::Relocation<func_t> func{ ID::NiMatrix3::FromEulerAnglesYZX };
		return func(this, a_y, a_z, a_x);
	}

	void NiMatrix3::FromEulerAnglesZYX(float a_z, float a_y, float a_x)
	{
		using func_t = void (*)(NiMatrix3*, float, float, float);
		static REL::Relocation<func_t> func{ ID::NiMatrix3::FromEulerAnglesZYX };
		return func(this, a_z, a_y, a_x);
	}

	void NiMatrix3::FromEulerAnglesZXY(float a_z, float a_x, float a_y)
	{
		using func_t = void (*)(NiMatrix3*, float, float, float);
		static REL::Relocation<func_t> func{ ID::NiMatrix3::FromEulerAnglesZXY };
		return func(this, a_z, a_x, a_y);
	}
}
