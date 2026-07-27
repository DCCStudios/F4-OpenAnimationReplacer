#pragma once

namespace RE
{
	enum class DETECTION_FORMULA_TYPE : std::uint32_t
	{
		kFormula = 0x0,
		kRange = 0x1,
		kVisual_Range = 0x2,
		kSound_Range = 0x3,
	};
}
