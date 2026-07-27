#pragma once

#include "RE/H/hkBool.h"
#include "RE/H/hkReferencedObject.h"
#include "RE/H/hkVector4.h"
#include "RE/H/hknpCharacterState.h"

namespace RE
{
	class hknpCharacterStateManager;

	class hknpCharacterContext :
		public hkReferencedObject
	{
	public:
		static constexpr auto RTTI{ RTTI::hknpCharacterContext };
		static constexpr auto VTABLE{ VTABLE::hknpCharacterContext };

		enum class CharacterType : std::int32_t
		{
			kProxy = 0x0,
			kRigidBody = 0x1
		};

		CharacterType                              m_characterType;             // 0x10
		const hknpCharacterStateManager*           m_stateManager;              // 0x18
		hknpCharacterState::hknpCharacterStateType m_currentState;              // 0x20
		hknpCharacterState::hknpCharacterStateType m_previousState;             // 0x24
		hkBool                                     m_filterEnable;              // 0x28
		float                                      m_maxLinearAcceleration;     // 0x2C
		float                                      m_maxLinearSpeed;            // 0x30
		float                                      m_gain;                      // 0x34
		std::byte                                  m_pad38[0x04];               // 0x38
		hkVector4f                                 m_previousSurfaceVelocity;   // 0x40
		std::int32_t                               m_numUpdatesInCurrentState;  // 0x50
		std::byte                                  m_pad54[0x0C];               // 0x54
	};
	static_assert(sizeof(hknpCharacterContext) == 0x60);
}
