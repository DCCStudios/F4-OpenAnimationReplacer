#pragma once

#include "RE/H/hkSimdFloat32.h"
#include "RE/H/hkVector4.h"

namespace RE
{
	class hknpSolverInfo
	{
	public:
		// members
		float         m_tau;                                   // 0x000
		float         m_frictionTau;                           // 0x004
		hkSimdFloat32 m_damping;                               // 0x010
		hkSimdFloat32 m_deltaTime;                             // 0x020
		hkSimdFloat32 m_invDeltaTime;                          // 0x030
		hkSimdFloat32 m_subStepDeltaTime;                      // 0x040
		hkSimdFloat32 m_subStepInvDeltaTime;                   // 0x050
		hkSimdFloat32 m_distanceToLinearTim;                   // 0x060
		hkSimdFloat32 m_linearTimToDistance;                   // 0x070
		hkSimdFloat32 m_collisionTolerance;                    // 0x080
		hkSimdFloat32 m_unitScale;                             // 0x090
		hkSimdFloat32 m_collisionAccuracy;                     // 0x0A0
		float         m_maxApproachSpeedForHighQualitySolver;  // 0x0B0
		std::int32_t  m_numSteps;                              // 0x0B4
		float         m_invNumSteps;                           // 0x0B8
		std::int32_t  m_numMicroSteps;                         // 0x0BC
		float         m_invNumMicroSteps;                      // 0x0C0
		float         m_nominalGravityLength;                  // 0x0C4
		float         m_expectedDeltaTime;                     // 0x0C8
		std::uint32_t m_stepSolveCount;                        // 0x0CC
		hkVector4f    m_globalAccelerationPerSubStep;          // 0x0D0
		hkVector4f    m_globalAccelerationPerStep;             // 0x0E0
		hkVector4f    m_integrateVelocityFactor;               // 0x0F0
		hkVector4f    m_invIntegrateVelocityFactor;            // 0x100
		hkSimdFloat32 m_rhsFactor;                             // 0x110
		hkSimdFloat32 m_frictionRhsFactor;                     // 0x120
		float         m_dampDivTau;                            // 0x130
		float         m_tauDivDamp;                            // 0x134
		float         m_dampDivFrictionTau;                    // 0x138
		float         m_frictionTauDivDamp;                    // 0x13C
	};
	static_assert(sizeof(hknpSolverInfo) == 0x140);
}
