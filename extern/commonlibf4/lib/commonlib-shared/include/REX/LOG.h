#pragma once

#include "REX/BASE.h"

namespace REX
{
	enum class ELogLevel
	{
		Trace = 0,
		Debug = 1,
		Info = 2,
		Warning = 3,
		Error = 4,
		Critical = 5,
	};
}

namespace REX::Impl
{
	void Log(const std::source_location a_loc, const ELogLevel a_level, const std::string_view a_fmt);

	void Log(const std::source_location a_loc, const ELogLevel a_level, const std::wstring_view a_fmt);

	template <class... T>
	void Log(const std::source_location a_loc, const ELogLevel a_level, const std::format_string<T...> a_fmt, T&&... a_args)
	{
		Log(a_loc, a_level, std::vformat(a_fmt.get(), std::make_format_args(a_args...)));
	}

	template <class... T>
	void Log(const std::source_location a_loc, const ELogLevel a_level, const std::wformat_string<T...> a_fmt, T&&... a_args)
	{
		Log(a_loc, a_level, std::vformat(a_fmt.get(), std::make_wformat_args(a_args...)));
	}
}

namespace REX
{
	template <class... T>
	struct TRACE
	{
		TRACE() = delete;

		explicit TRACE(const std::format_string<T...> a_fmt, T&&... a_args, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Trace, a_fmt, std::forward<T>(a_args)...);
		}

		explicit TRACE(const std::wformat_string<T...> a_fmt, T&&... a_args, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Trace, a_fmt, std::forward<T>(a_args)...);
		}
	};

	template <>
	struct TRACE<void>
	{
		TRACE() = delete;

		explicit TRACE(const std::string_view a_fmt, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Trace, a_fmt);
		}

		explicit TRACE(const std::wstring_view a_fmt, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Trace, a_fmt);
		}
	};

	template <class... T>
	struct DEBUG
	{
		DEBUG() = delete;

		explicit DEBUG(const std::format_string<T...> a_fmt, T&&... a_args, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Debug, a_fmt, std::forward<T>(a_args)...);
		}

		explicit DEBUG(const std::wformat_string<T...> a_fmt, T&&... a_args, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Debug, a_fmt, std::forward<T>(a_args)...);
		}
	};

	template <>
	struct DEBUG<void>
	{
		DEBUG() = delete;

		explicit DEBUG(const std::string_view a_fmt, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Debug, a_fmt);
		}

		explicit DEBUG(const std::wstring_view a_fmt, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Debug, a_fmt);
		}
	};

	template <class... T>
	struct INFO
	{
		INFO() = delete;

		explicit INFO(const std::format_string<T...> a_fmt, T&&... a_args, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Info, a_fmt, std::forward<T>(a_args)...);
		}

		explicit INFO(const std::wformat_string<T...> a_fmt, T&&... a_args, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Info, a_fmt, std::forward<T>(a_args)...);
		}
	};

	template <>
	struct INFO<void>
	{
		INFO() = delete;

		explicit INFO(const std::string_view a_fmt, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Info, a_fmt);
		}

		explicit INFO(const std::wstring_view a_fmt, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Info, a_fmt);
		}
	};

	template <class... T>
	struct WARN
	{
		WARN() = delete;

		explicit WARN(const std::format_string<T...> a_fmt, T&&... a_args, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Warning, a_fmt, std::forward<T>(a_args)...);
		}

		explicit WARN(const std::wformat_string<T...> a_fmt, T&&... a_args, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Warning, a_fmt, std::forward<T>(a_args)...);
		}
	};

	template <>
	struct WARN<void>
	{
		WARN() = delete;

		explicit WARN(const std::string_view a_fmt, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Warning, a_fmt);
		}

		explicit WARN(const std::wstring_view a_fmt, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Warning, a_fmt);
		}
	};

	template <class... T>
	struct ERROR
	{
		ERROR() = delete;

		explicit ERROR(const std::format_string<T...> a_fmt, T&&... a_args, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Error, a_fmt, std::forward<T>(a_args)...);
		}

		explicit ERROR(const std::wformat_string<T...> a_fmt, T&&... a_args, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Error, a_fmt, std::forward<T>(a_args)...);
		}
	};

	template <>
	struct ERROR<void>
	{
		ERROR() = delete;

		explicit ERROR(const std::string_view a_fmt, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Error, a_fmt);
		}

		explicit ERROR(const std::wstring_view a_fmt, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Error, a_fmt);
		}
	};

	template <class... T>
	struct CRITICAL
	{
		CRITICAL() = delete;

		explicit CRITICAL(const std::format_string<T...> a_fmt, T&&... a_args, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Critical, a_fmt, std::forward<T>(a_args)...);
		}

		explicit CRITICAL(const std::wformat_string<T...> a_fmt, T&&... a_args, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Critical, a_fmt, std::forward<T>(a_args)...);
		}
	};

	template <>
	struct CRITICAL<void>
	{
		CRITICAL() = delete;

		explicit CRITICAL(const std::string_view a_fmt, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Critical, a_fmt);
		}

		explicit CRITICAL(const std::wstring_view a_fmt, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Log(a_loc, ELogLevel::Critical, a_fmt);
		}
	};

	template <class... T>
	TRACE(const std::format_string<T...>, T&&...) -> TRACE<T...>;
	template <class... T>
	TRACE(const std::wformat_string<T...>, T&&...) -> TRACE<T...>;
	TRACE(const std::string_view) -> TRACE<void>;
	TRACE(const std::wstring_view) -> TRACE<void>;

	template <class... T>
	DEBUG(const std::format_string<T...>, T&&...) -> DEBUG<T...>;
	template <class... T>
	DEBUG(const std::wformat_string<T...>, T&&...) -> DEBUG<T...>;
	DEBUG(const std::string_view) -> DEBUG<void>;
	DEBUG(const std::wstring_view) -> DEBUG<void>;

	template <class... T>
	INFO(const std::format_string<T...>, T&&...) -> INFO<T...>;
	template <class... T>
	INFO(const std::wformat_string<T...>, T&&...) -> INFO<T...>;
	INFO(const std::string_view) -> INFO<void>;
	INFO(const std::wstring_view) -> INFO<void>;

	template <class... T>
	WARN(const std::format_string<T...>, T&&...) -> WARN<T...>;
	template <class... T>
	WARN(const std::wformat_string<T...>, T&&...) -> WARN<T...>;
	WARN(const std::string_view) -> WARN<void>;
	WARN(const std::wstring_view) -> WARN<void>;

	template <class... T>
	ERROR(const std::format_string<T...>, T&&...) -> ERROR<T...>;
	template <class... T>
	ERROR(const std::wformat_string<T...>, T&&...) -> ERROR<T...>;
	ERROR(const std::string_view) -> ERROR<void>;
	ERROR(const std::wstring_view) -> ERROR<void>;

	template <class... T>
	CRITICAL(const std::format_string<T...>, T&&...) -> CRITICAL<T...>;
	template <class... T>
	CRITICAL(const std::wformat_string<T...>, T&&...) -> CRITICAL<T...>;
	CRITICAL(const std::string_view) -> CRITICAL<void>;
	CRITICAL(const std::wstring_view) -> CRITICAL<void>;
}

namespace REX
{
	namespace Impl
	{
		void Fail(const std::source_location a_loc, const std::string_view a_fmt);
		void Fail(const std::source_location a_loc, const std::wstring_view a_fmt);

		template <class... T>
		void Fail(const std::source_location a_loc, const std::format_string<T...> a_fmt, T&&... a_args)
		{
			Fail(a_loc, std::vformat(a_fmt.get(), std::make_format_args(a_args...)));
		}

		template <class... T>
		void Fail(const std::source_location a_loc, const std::wformat_string<T...> a_fmt, T&&... a_args)
		{
			Fail(a_loc, std::vformat(a_fmt.get(), std::make_wformat_args(a_args...)));
		}
	}

	template <class... T>
	struct FAIL
	{
		FAIL() = delete;

		explicit FAIL(const std::format_string<T...> a_fmt, T&&... a_args, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Fail(a_loc, a_fmt, std::forward<T>(a_args)...);
		}

		explicit FAIL(const std::wformat_string<T...> a_fmt, T&&... a_args, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Fail(a_loc, a_fmt, std::forward<T>(a_args)...);
		}

		explicit FAIL(const std::source_location a_loc, const std::format_string<T...> a_fmt, T&&... a_args)
		{
			Impl::Fail(a_loc, a_fmt, std::forward<T>(a_args)...);
		}

		explicit FAIL(const std::source_location a_loc, const std::wformat_string<T...> a_fmt, T&&... a_args)
		{
			Impl::Fail(a_loc, a_fmt, std::forward<T>(a_args)...);
		}
	};

	template <>
	struct FAIL<void>
	{
		FAIL() = delete;

		explicit FAIL(const std::string_view a_fmt, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Fail(a_loc, a_fmt);
		}

		explicit FAIL(const std::wstring_view a_fmt, const std::source_location a_loc = std::source_location::current())
		{
			Impl::Fail(a_loc, a_fmt);
		}

		explicit FAIL(const std::source_location a_loc, const std::string_view a_fmt)
		{
			Impl::Fail(a_loc, a_fmt);
		}

		explicit FAIL(const std::source_location a_loc, const std::wstring_view a_fmt)
		{
			Impl::Fail(a_loc, a_fmt);
		}
	};

	template <class... T>
	FAIL(const std::format_string<T...>, T&&...) -> FAIL<T...>;
	template <class... T>
	FAIL(const std::wformat_string<T...>, T&&...) -> FAIL<T...>;
	FAIL(const std::string_view) -> FAIL<void>;
	FAIL(const std::wstring_view) -> FAIL<void>;
}
