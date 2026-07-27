#pragma once

namespace RE
{
	class PipboyCommandResult;

	class __declspec(novtable) PipboyCommand
	{
	public:
		static constexpr auto RTTI{ RTTI::PipboyCommand };
		static constexpr auto VTABLE{ VTABLE::PipboyCommand };

		PipboyCommand(std::uint32_t a_id) :
			id(a_id)
		{
			REX::EMPLACE_VTABLE(this);
		}

		// add
		virtual ~PipboyCommand();                                          // 00
		virtual void                       Init(Json::Value* a_json) = 0;  // 01
		virtual const PipboyCommandResult* DoValidate();                   // 02
		virtual const PipboyCommandResult* DoExecute();                    // 03

		// members
		const std::uint32_t        id;                           // 0x08
		const PipboyCommandResult* validationResult{ nullptr };  // 0x10
		const PipboyCommandResult* executionResult{ nullptr };   // 0x18
		bool                       isReadonOnly{ false };        // 0x20
	};
	static_assert(sizeof(PipboyCommand) == 0x28);
}
