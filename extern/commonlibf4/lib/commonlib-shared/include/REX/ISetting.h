#pragma once

namespace REX
{
	struct ISetting
	{
		virtual void Load(void* a_data, bool a_isBase) = 0;
		virtual void Save(void* a_data) = 0;
	};
}
