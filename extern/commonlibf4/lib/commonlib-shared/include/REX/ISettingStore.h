#pragma once

namespace REX
{
	struct ISetting;

	struct ISettingStore
	{
		virtual void Init(const char* a_file, const char* a_fileCustom) = 0;
		virtual void Add(ISetting* a_setting) = 0;
		virtual void Load() = 0;
		virtual void Save() = 0;
	};
}
