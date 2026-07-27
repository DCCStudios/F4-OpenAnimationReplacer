#pragma once

#include "REX/BASE.h"

namespace REX
{
	template <class T>
	class TSingleton
	{
	public:
		static T* GetSingleton()
		{
			static T singleton;
			return std::addressof(singleton);
		}

	protected:
		TSingleton() = default;
		~TSingleton() = default;

		TSingleton(const TSingleton&) = delete;
		TSingleton(TSingleton&&) = delete;

		TSingleton& operator=(const TSingleton&) = delete;
		TSingleton& operator=(TSingleton&&) = delete;
	};

	template <class T>
	using Singleton [[deprecated("Renamed to 'REX::TSingleton'")]] = TSingleton<T>;
}
