#pragma once

#include "ISetting.h"

namespace REX
{
	template <class T>
	class TSetting :
		public ISetting
	{
	public:
		TSetting() = delete;

		TSetting(T a_default) :
			m_value(a_default),
			m_valueDefault(a_default)
		{}

	public:
		T GetValue() const
		{
			return m_value;
		}

		T GetValueDefault() const
		{
			return m_valueDefault;
		}

		void SetValue(T a_value)
		{
			m_value = a_value;
		}

	public:
		operator T&() { return m_value; }
		operator const T&() const { return m_value; }

	protected:
		T m_value;
		T m_valueDefault;
	};
}
