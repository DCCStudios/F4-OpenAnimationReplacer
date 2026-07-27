#pragma once

namespace RE
{
	class BSISoundOutputModel;
	class NiAVObject;
	class NiPoint3;

	class BSSoundHandle
	{
	public:
		~BSSoundHandle() noexcept = default;  // NOLINT(modernize-use-equals-default)

		enum class ASSUMED_STATE : std::uint32_t
		{
			kInitialized = 0x0,
			kPlaying = 0x1,
			kStopped = 0x2,
			kPaused = 0x3
		};

		enum class LOOP_TYPE : std::uint32_t
		{
			kNone = 0x0,
			kWholeFile = 0x1,
			kEnvFast = 0x2,
			kEnvSlow = 0x3
		};

		void ClearFollowedObject()
		{
			using func_t = decltype(&BSSoundHandle::ClearFollowedObject);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::ClearFollowedObject };
			func(this);
		}

		bool FadeInPlay(std::uint16_t a_milliseconds)
		{
			using func_t = decltype(&BSSoundHandle::FadeInPlay);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::FadeInPlay };
			return func(this, a_milliseconds);
		}

		bool FadeOutAndRelease(std::uint16_t a_milliseconds)
		{
			using func_t = decltype(&BSSoundHandle::FadeOutAndRelease);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::FadeOutAndRelease };
			return func(this, a_milliseconds);
		}

		bool FadeTo(const std::uint16_t a_targetAttenuation, const std::uint16_t a_milliseconds)
		{
			using func_t = decltype(&BSSoundHandle::FadeTo);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::FadeTo };
			return func(this, a_targetAttenuation, a_milliseconds);
		}

		std::uint64_t GetDuration()
		{
			using func_t = decltype(&BSSoundHandle::GetDuration);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::GetDuration };
			return func(this);
		}

		bool IsEnvelopeLoop()
		{
			using func_t = decltype(&BSSoundHandle::IsEnvelopeLoop);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::IsEnvelopeLoop };
			return func(this);
		}

		bool IsPaused()
		{
			using func_t = decltype(&BSSoundHandle::IsPaused);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::IsPaused };
			return func(this);
		}

		bool IsPlaying() const
		{
			using func_t = decltype(&BSSoundHandle::IsPlaying);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::IsPlaying };
			return func(this);
		}

		bool IsValid()
		{
			using func_t = decltype(&BSSoundHandle::IsValid);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::IsValid };
			return func(this);
		}

		bool Pause()
		{
			using func_t = decltype(&BSSoundHandle::Pause);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::Pause };
			return func(this);
		}

		bool Play()
		{
			using func_t = decltype(&BSSoundHandle::Play);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::Play };
			return func(this);
		}

		bool PlayAfter(const std::uint64_t a_delay)
		{
			using func_t = decltype(&BSSoundHandle::PlayAfter);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::PlayAfter };
			return func(this, a_delay);
		}

		bool SetFrequency(const float a_frequency)
		{
			using func_t = decltype(&BSSoundHandle::SetFrequency);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::SetFrequency };
			return func(this, a_frequency);
		}

		bool SetFrequencyVariance(const std::uint8_t a_variance)
		{
			using func_t = decltype(&BSSoundHandle::SetFrequencyVariance);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::SetFrequencyVariance };
			return func(this, a_variance);
		}

		void SetObjectToFollow(const NiAVObject* a_object)
		{
			using func_t = decltype(&BSSoundHandle::SetObjectToFollow);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::SetObjectToFollow };
			func(this, a_object);
		}

		bool SetOutputModel(const BSISoundOutputModel* a_outputModel)
		{
			using func_t = decltype(&BSSoundHandle::SetOutputModel);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::SetOutputModel };
			return func(this, a_outputModel);
		}

		bool SetPosition(const NiPoint3& a_position)
		{
			using func_t = decltype(&BSSoundHandle::SetPosition);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::SetPosition };
			return func(this, a_position);
		}

		bool SetStaticAttenuation(const std::uint16_t a_attenuation)
		{
			using func_t = decltype(&BSSoundHandle::SetStaticAttenuation);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::SetStaticAttenuation };
			return func(this, a_attenuation);
		}

		bool Stop()
		{
			using func_t = decltype(&BSSoundHandle::Stop);
			static REL::Relocation<func_t> func{ ID::BSSoundHandle::Stop };
			return func(this);
		}

		// members
		std::uint32_t soundID{ static_cast<std::uint32_t>(-1) };  // 0x0
		bool          assumeSuccess{ false };                     // 0x4
		std::uint8_t  state{ 0 };                                 // 0x5
	};
	static_assert(sizeof(BSSoundHandle) == 0x8);
}
