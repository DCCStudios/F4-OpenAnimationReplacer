#include "REL/Offset2ID.h"

#include "REX/FModule.h"
#include "REX/LOG.h"

namespace REL
{
	void Offset2ID::load_v2()
	{
		const auto iddb = IDDB::GetSingleton();
		const auto id2offset = iddb->get_id2offset<IDDB::MAPPING>();
		_offset2id.reserve(id2offset.size());
		_offset2id.insert(_offset2id.begin(), id2offset.begin(), id2offset.end());
		std::sort(std::execution::sequenced_policy{}, _offset2id.begin(), _offset2id.end(), [](auto&& a_lhs, auto&& a_rhs) {
			return a_lhs.offset < a_rhs.offset;
		});
	}

	void Offset2ID::load_v5()
	{
		const auto iddb = IDDB::GetSingleton();
		const auto id2offset = iddb->get_id2offset<std::uint32_t>();
		_offset2id.reserve(id2offset.size());

		std::uint64_t id{ 0 };
		for (auto offset : id2offset) {
			_offset2id.emplace(_offset2id.end(), value_type{ id++, offset });
		}

		std::sort(std::execution::sequenced_policy{}, _offset2id.begin(), _offset2id.end(), [](auto&& a_lhs, auto&& a_rhs) {
			return a_lhs.offset < a_rhs.offset;
		});
	}

	std::uint64_t Offset2ID::get_id(std::size_t a_offset) const
	{
		if (_offset2id.empty()) {
			REX::FAIL("No Address Library has been loaded!");
		}

		const value_type elem{ 0, a_offset };
		const auto       it = std::lower_bound(
            _offset2id.begin(),
            _offset2id.end(),
            elem,
            [](auto&& a_lhs, auto&& a_rhs) {
                return a_lhs.offset < a_rhs.offset;
            });
		if (it == _offset2id.end()) {
			const auto mod = REX::FModule::GetExecutingModule();
			const auto version = mod.GetFileVersion();
			REX::FAIL(
				"Failed to find Address Library ID for offset!\n"
				"Invalid offset: 0x{:08X}\n"
				"Game Version: {}",
				a_offset, version.string());
		}

		return it->id;
	}
}
