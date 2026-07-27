#pragma once

namespace RE
{
	class BGSSoundDescriptorForm;
	class BGSSoundOutput;

	class BGSAudio
	{
	public:
		static BGSSoundDescriptorForm* GetSoundDescriptor(const char* a_formName)
		{
			using func_t = decltype(&BGSAudio::GetSoundDescriptor);
			static REL::Relocation<func_t> func{ ID::BGSAudio::GetSoundDescriptor };
			return func(a_formName);
		}

		static const BGSSoundOutput* GetUIOutputModel()
		{
			using func_t = decltype(&BGSAudio::GetUIOutputModel);
			static REL::Relocation<func_t> func{ ID::BGSAudio::GetUIOutputModel };
			return func();
		}
	};
}
