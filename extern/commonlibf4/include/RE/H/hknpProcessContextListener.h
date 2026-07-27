#pragma once

namespace RE
{
	class hknpWorld;
	class hkProcess;

	class hknpProcessContextListener
	{
	public:
		hknpProcessContextListener() = default;

		virtual ~hknpProcessContextListener() = default;  // 0x00 [00]

		// add
		virtual void       WorldAddedCallback(hknpWorld* a_world) = 0;    // 0x08 [01]
		virtual void       WorldRemovedCallback(hknpWorld* a_world) = 0;  // 0x10 [02]
		virtual hkProcess* GetProcess() { return nullptr; }               // 0x18 [03]
	};
	static_assert(sizeof(hknpProcessContextListener) == 0x08);
}
