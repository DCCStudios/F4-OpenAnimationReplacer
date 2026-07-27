#pragma once

#include "RE/H/hkArray.h"
#include "RE/H/hkBool.h"
#include "RE/H/hkReferencedObject.h"
#include "RE/H/hkStopwatch.h"

namespace RE
{
	class hkClass;
	class hkProcessContext;
	class hkSocket;
	class hkStringPtr;
	class hkVisualDebuggerClient;
	class hkVisualDebuggerTrackedObject;
	class hkVtableClassRegistry;

	class hkVisualDebugger :
		public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hkVisualDebugger };
		inline static constexpr auto VTABLE{ VTABLE::hkVisualDebugger };

		using TrackCallback = void (*)(void*, const hkClass*, hkBool, std::uint64_t, void*);

		hkVisualDebugger(const hkArray<hkProcessContext*>& a_contexts, const hkVtableClassRegistry* a_classReg = nullptr)
		{
			using func_t = void (*)(hkVisualDebugger*, const hkArray<hkProcessContext*>&, const hkVtableClassRegistry*);
			static REL::Relocation<func_t> func{ ID::hkVisualDebugger::ctor };
			func(this, a_contexts, a_classReg);
		}

		// add
		virtual void Step([[maybe_unused]] float a_frameTime) {}  // 0x20 [04]
		virtual void PollForNewClients() {}                       // 0x28 [05]

		void AddContext(hkProcessContext* a_context)
		{
			using func_t = decltype(&hkVisualDebugger::AddContext);
			static REL::Relocation<func_t> func{ ID::hkVisualDebugger::AddContext };
			func(this, a_context);
		}

		void AddDefaultProcess(const char* a_name)
		{
			using func_t = decltype(&hkVisualDebugger::AddDefaultProcess);
			static REL::Relocation<func_t> func{ ID::hkVisualDebugger::AddDefaultProcess };
			func(this, a_name);
		}

		void Serve(const std::uint32_t a_port = 25001)
		{
			using func_t = decltype(&hkVisualDebugger::Serve);
			static REL::Relocation<func_t> func{ ID::hkVisualDebugger::Serve };
			func(this, a_port);
		}

		void Shutdown()
		{
			using func_t = decltype(&hkVisualDebugger::Shutdown);
			static REL::Relocation<func_t> func{ ID::hkVisualDebugger::Shutdown };
			func(this);
		}

		// members
		hkSocket*                              m_server;                     // 0x10
		bool                                   m_suppressPollForNewClients;  // 0x18
		hkArray<hkVisualDebuggerClient>        m_clients;                    // 0x20
		hkArray<hkProcessContext*>             m_contexts;                   // 0x30
		hkArray<hkVisualDebuggerTrackedObject> m_trackedObjects;             // 0x40
		hkArray<TrackCallback>                 m_trackCallbacks;             // 0x50
		hkArray<void*>                         m_trackCallbackHandles;       // 0x60
		const hkVtableClassRegistry*           m_classReg;                   // 0x70
		hkArray<hkStringPtr>                   m_defaultProcesses;           // 0x78
		hkArray<hkStringPtr>                   m_requiredProcesses;          // 0x88
		hkBool                                 m_timingFrame;                // 0x98
		hkStopwatch                            m_frameTimer;                 // 0xA0
		hkBool                                 m_overrideFrameTimeIfZero;    // 0xD0
		std::byte                              m_padD1[0x7];                 // 0xD1
	};
	static_assert(sizeof(hkVisualDebugger) == 0xD8);
}
