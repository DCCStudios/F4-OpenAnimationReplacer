#include "AnimationLog.h"

void AnimationLog::AddEntry(EventType a_type, RE::TESObjectREFR* a_refr,
	const std::string& a_origAnim, const std::string& a_replAnim,
	const std::string& a_subModName,
	const std::string& a_fullPath,
	Perspective a_perspective)
{
	if (!enabled) return;

	Entry entry;
	entry.type = a_type;
	entry.refrName = GetRefrName(a_refr);
	entry.refrFormID = a_refr ? a_refr->GetFormID() : 0;
	entry.originalAnim = a_origAnim;
	entry.replacementAnim = a_replAnim;
	entry.subModName = a_subModName;
	entry.fullPath = a_fullPath;
	entry.perspective = a_perspective;
	entry.timestamp = std::chrono::steady_clock::now();

	std::lock_guard lock(mutex);
	entries.push_back(std::move(entry));
	while (static_cast<int>(entries.size()) > maxEntries) {
		entries.pop_front();
	}
}

void AnimationLog::AddAnimEvent(RE::TESObjectREFR* a_refr, const std::string& a_eventName)
{
	if (!enabled) return;

	Entry entry;
	entry.type = EventType::kAnimEvent;
	entry.refrName = GetRefrName(a_refr);
	entry.refrFormID = a_refr ? a_refr->GetFormID() : 0;
	entry.originalAnim = a_eventName;
	entry.timestamp = std::chrono::steady_clock::now();

	std::lock_guard lock(animEventMutex);
	animEventEntries.push_back(std::move(entry));
	while (static_cast<int>(animEventEntries.size()) > maxEntries) {
		animEventEntries.pop_front();
	}
}

void AnimationLog::Clear()
{
	std::lock_guard lock(mutex);
	entries.clear();
}

void AnimationLog::ClearAnimEvents()
{
	std::lock_guard lock(animEventMutex);
	animEventEntries.clear();
}

std::string AnimationLog::GetRefrName(RE::TESObjectREFR* a_refr) const
{
	if (!a_refr) return "Unknown";
	auto name = RE::TESFullName::GetFullName(*a_refr);
	if (!name.empty()) return std::string(name);
	return std::format("0x{:08X}", a_refr->GetFormID());
}
