#pragma once

#include <vector>
#include <string_view>

namespace REL
{
	using FHookHandle = std::uint32_t;

	enum class EHookType : std::uint32_t
	{
		None = 0,
		CALL5 = 2,
		CALL6 = 3,
		JMP5 = 4,
		JMP6 = 5,
		VFT = 6,
	};

	enum class EHookStep : std::uint32_t
	{
		None = 0,
		PreLoad = 1,
		Load = 2,
	};

	struct IHook
	{
		virtual bool        Init() = 0;
		virtual FHookHandle GetHandle() const = 0;
		virtual const char* GetName() const = 0;
		virtual EHookType   GetType() const = 0;
		virtual EHookStep   GetStep() const = 0;
		virtual std::size_t GetSize() const = 0;
		virtual std::size_t GetSizeTrampoline() const = 0;
		virtual bool        GetEnabled() const = 0;
		virtual bool        Enable() = 0;
		virtual bool        Disable() = 0;
	};
}

template <class T>
	requires(std::is_base_of_v<REL::IHook, T>)
struct std::formatter<T>
{
	template <class ParseContext>
	constexpr auto parse(ParseContext& a_ctx)
	{
		return a_ctx.begin();
	}

	template <class FormatContext>
	constexpr auto format(const T& a_hook, FormatContext& a_ctx) const
	{
		return format_to(a_ctx.out(), "Hook [{}]", a_hook.GetName());
	}
};

template <>
struct std::formatter<REL::EHookType>
{
	template <class ParseContext>
	constexpr auto parse(ParseContext& a_ctx)
	{
		return a_ctx.begin();
	}

	template <class FormatContext>
	constexpr auto format(const REL::EHookType& a_type, FormatContext& a_ctx) const
	{
		switch (a_type) {
			case REL::EHookType::None:
				return format_to(a_ctx.out(), "None");
			case REL::EHookType::CALL5:
				return format_to(a_ctx.out(), "CALL5");
			case REL::EHookType::CALL6:
				return format_to(a_ctx.out(), "CALL6");
			case REL::EHookType::JMP5:
				return format_to(a_ctx.out(), "JMP5");
			case REL::EHookType::JMP6:
				return format_to(a_ctx.out(), "JMP6");
			case REL::EHookType::VFT:
				return format_to(a_ctx.out(), "VFT");
		}

		return format_to(a_ctx.out(), "<Unknown>");
	}
};

template <>
struct std::formatter<REL::EHookStep>
{
	template <class ParseContext>
	constexpr auto parse(ParseContext& a_ctx)
	{
		return a_ctx.begin();
	}

	template <class FormatContext>
	constexpr auto format(const REL::EHookStep& a_step, FormatContext& a_ctx) const
	{
		switch (a_step) {
			case REL::EHookStep::None:
				return format_to(a_ctx.out(), "None");
			case REL::EHookStep::PreLoad:
				return format_to(a_ctx.out(), "PreLoad");
			case REL::EHookStep::Load:
				return format_to(a_ctx.out(), "Load");
		}

		return format_to(a_ctx.out(), "<Unknown>");
	}
};
