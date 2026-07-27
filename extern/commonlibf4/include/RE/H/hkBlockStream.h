#pragma once

#include "RE/H/hkBlockStreamBase.h"

namespace RE
{
	template <class T>
	class hkBlockStream :
		public hkBlockStreamBase::Stream
	{
	public:
		class Consumer;
		class Modifier;
		class Reader;
		class Writer;
	};
	static_assert(sizeof(hkBlockStream<void>) == 0x100);
}
