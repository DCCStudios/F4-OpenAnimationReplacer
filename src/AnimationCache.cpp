#include "AnimationCache.h"
#include "OpenAnimationReplacer.h"
#include "Settings.h"
#include "RE/B/BSResourceNiBinaryStream.h"

#include <string_view>

// Per-file load logging is gated behind bVerboseLogging: at tens of thousands
// of animations the ~15 info lines each file emits during preload dominate
// the entire menu-load time (they used to be flushed line-by-line on top).
// Warnings/errors always log.
static bool VerboseCacheLog()
{
	return Settings::GetSingleton()->bVerboseLogging;
}

static uintptr_t ResolveVtable(AnimationCache::CachedAnimation::VtableFixupKind a_kind, uintptr_t a_gameAnimationVtable)
{
	switch (a_kind) {
	case AnimationCache::CachedAnimation::VtableFixupKind::kAnimatedReferenceFrame: {
		static REL::Relocation<uintptr_t> vtable{ RE::VTABLE::hkaAnimatedReferenceFrame[0] };
		return vtable.address();
	}
	case AnimationCache::CachedAnimation::VtableFixupKind::kDefaultAnimatedReferenceFrame: {
		static REL::Relocation<uintptr_t> vtable{ RE::VTABLE::hkaDefaultAnimatedReferenceFrame[0] };
		return vtable.address();
	}
	case AnimationCache::CachedAnimation::VtableFixupKind::kGameAnimation:
	default:
		return a_gameAnimationVtable;
	}
}

namespace
{
	// Havok packfile header - 64 bytes (0x40)
	struct HkxPackfileHeader
	{
		uint32_t magic0;              // 0x00
		uint32_t magic1;              // 0x04
		uint32_t userTag;             // 0x08
		uint32_t fileVersion;         // 0x0C
		uint8_t  pointerSize;         // 0x10
		uint8_t  littleEndian;        // 0x11
		uint8_t  reusePadding;        // 0x12
		uint8_t  emptyBaseOpt;        // 0x13
		uint32_t numSections;         // 0x14
		uint32_t contentsSectionIndex;   // 0x18
		uint32_t contentsSectionOffset;  // 0x1C
		uint32_t classNameSectionIndex;  // 0x20
		uint32_t classNameSectionOffset; // 0x24
		char     contentsVersion[16];    // 0x28
		uint32_t flags;               // 0x38
		uint16_t maxPredicate;        // 0x3C
		uint16_t predicateArraySizePlusPadding; // 0x3E
	};
	static_assert(sizeof(HkxPackfileHeader) == 0x40);

	// v11 (Havok 2014) section header - 0x40 bytes (64 bytes)
	// Layout: name[16] + constant[4] + 7*uint32 offsets + trailing_pad[16]
	struct HkxSectionHeader_v11
	{
		char     sectionTag[16];       // 0x00 - null-padded section name
		uint8_t  constant[4];          // 0x10 - typically {0x00, 0x00, 0x00, 0xFF}
		uint32_t absoluteDataStart;    // 0x14
		uint32_t localFixupsOffset;    // 0x18
		uint32_t globalFixupsOffset;   // 0x1C
		uint32_t virtualFixupsOffset;  // 0x20
		uint32_t exportsOffset;        // 0x24
		uint32_t importsOffset;        // 0x28
		uint32_t endOffset;            // 0x2C
		uint8_t  padding[16];          // 0x30 - trailing 0xFF padding for v11
	};
	static_assert(sizeof(HkxSectionHeader_v11) == 0x40);

	// v8 (older Havok) section header - 0x30 bytes (48 bytes)
	// Layout: name[16] + constant[4] + 7*uint32 offsets
	struct HkxSectionHeader_v8
	{
		char     sectionTag[16];       // 0x00
		uint8_t  constant[4];          // 0x10
		uint32_t absoluteDataStart;    // 0x14
		uint32_t localFixupsOffset;    // 0x18
		uint32_t globalFixupsOffset;   // 0x1C
		uint32_t virtualFixupsOffset;  // 0x20
		uint32_t exportsOffset;        // 0x24
		uint32_t importsOffset;        // 0x28
		uint32_t endOffset;            // 0x2C
	};
	static_assert(sizeof(HkxSectionHeader_v8) == 0x30);

	static constexpr uint32_t kPackfileMagic0 = 0x57E0E057;
	static constexpr uint32_t kPackfileMagic1 = 0x10C0C010;
	static constexpr uint32_t kTagfileMagic   = 0xCAB00D1E;
	static constexpr uint32_t kVersion11      = 0x0B;
}

bool AnimationCache::LoadAnimation(const std::string& a_suffix, const std::filesystem::path& a_absolutePath,
	const void* a_owner, int32_t a_priority, bool a_preserveExtractedMotion)
{
	std::error_code ec;
	const auto diskSize = std::filesystem::file_size(a_absolutePath, ec);
	if (ec) {
		logger::warn("[OAR-Cache] File not found: '{}'", a_absolutePath.string());
		return false;
	}
	const auto diskMTime = std::filesystem::last_write_time(a_absolutePath, ec);
	const bool hasDiskMTime = !ec;

	// Fast path: this exact file is already cached. The cache's real identity
	// is the on-disk file, not the owner tag — a config reload recreates every
	// SubMod, so matching on owner would miss ALL entries and re-read every
	// animation file from disk (seconds of hitching with thousands of anims).
	// Match by path instead and, when size+mtime are unchanged, just re-bind
	// the entry to the new owner/priority: the parsed data, annotations, and
	// runtime clone all stay valid because the bytes they were built from are
	// the same. A changed file (author replaced the .hkx and hit reload) falls
	// through to a full re-read that replaces the entry below.
	const std::string pathStr = a_absolutePath.string();
	{
		std::unique_lock lock(m_mutex);
		auto it = m_cache.find(a_suffix);
		if (it != m_cache.end()) {
			for (auto& existing : it->second) {
				if (!existing || existing->filePath != pathStr) continue;
				if (existing->fileSize == diskSize && existing->fileMTime == diskMTime) {
					if (existing->preserveExtractedMotion != a_preserveExtractedMotion) {
						RetireCloneLocked(*existing, /*a_retireBackingData=*/false, a_suffix);
					}
					existing->owner = a_owner;
					existing->priority = a_priority;
					existing->preserveExtractedMotion = a_preserveExtractedMotion;
					existing->pendingRebind = false;
					// Priority may have changed; keep index 0 = highest.
					std::ranges::stable_sort(it->second, [](const auto& a, const auto& b) {
						return (a ? a->priority : INT32_MIN) > (b ? b->priority : INT32_MIN);
					});
					// Count toward the loading progress bar like a real load.
					OpenAnimationReplacer::GetSingleton()->loadingLoadedAnims.fetch_add(1);
					return true;
				}
				break;
			}
		}
	}

	auto entry = std::make_unique<CachedAnimation>();
	entry->filePath = pathStr;
	entry->owner = a_owner;
	entry->priority = a_priority;
	entry->preserveExtractedMotion = a_preserveExtractedMotion;
	entry->fileSize = diskSize;
	entry->fileMTime = diskMTime;

	std::ifstream file(a_absolutePath, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		logger::warn("[OAR-Cache] Cannot open: '{}'", pathStr);
		return false;
	}

	const auto fileSize = file.tellg();
	if (fileSize < 64 || fileSize > 50 * 1024 * 1024) {
		logger::warn("[OAR-Cache] Invalid file size ({}) for: '{}'", static_cast<int64_t>(fileSize), pathStr);
		return false;
	}

	std::vector<uint8_t> bytes(static_cast<size_t>(fileSize));
	file.seekg(0);
	file.read(reinterpret_cast<char*>(bytes.data()), fileSize);
	if (!file) {
		logger::warn("[OAR-Cache] Failed to read: '{}'", pathStr);
		return false;
	}
	return LoadAnimationBytes(a_suffix, pathStr, std::move(bytes), hasDiskMTime,
		static_cast<uint64_t>(diskSize), diskMTime, a_owner, a_priority, a_preserveExtractedMotion);
}

namespace
{
	// SEH __except handlers must not create objects with destructors in the enclosing
	// frame, so route logging through this helper (keeps the __try wrapper frame free
	// of C++ objects that require unwinding — avoids C2712).
	bool ReportResourceFaultAndFail(std::string_view a_what, const std::string& a_path)
	{
		logger::warn("[OAR-Cache] Skipped {} after an access violation opening it "
			"(BA2 resource system not ready, or malformed/unregistered archive): '{}'",
			a_what, a_path);
		return false;
	}
}

bool AnimationCache::LoadAnimationResource(const std::string& a_suffix, const std::string& a_resourcePath,
	const void* a_owner, int32_t a_priority, bool a_preserveExtractedMotion)
{
	// The game's BSResourceNiBinaryStream ctor can ACCESS-VIOLATE (CreateStandardContext
	// null-deref) while the BA2 resource system is still registering archives during
	// startup, or for a malformed/unregistered archive. An access violation is an SEH
	// exception a C++ try/catch cannot catch, so guard the open/read with __try and skip
	// the faulting resource instead of taking the whole game down on load.
	__try {
		return DoLoadAnimationResourceUnsafe(a_suffix, a_resourcePath, a_owner, a_priority);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return ReportResourceFaultAndFail("archive animation", a_resourcePath);
	}
}

bool AnimationCache::DoLoadAnimationResourceUnsafe(const std::string& a_suffix, const std::string& a_resourcePath,
	const void* a_owner, int32_t a_priority)
{
	RE::BSResourceNiBinaryStream stream(a_resourcePath.c_str(), false, nullptr, true);
	if (!stream) {
		logger::warn("[OAR-Cache] Resource not found: '{}'", a_resourcePath);
		return false;
	}

	RE::NiBinaryStream::BufferInfo bufferInfo{};
	stream.GetBufferInfo(bufferInfo);
	if (bufferInfo.fileSize < 64 || bufferInfo.fileSize > 50 * 1024 * 1024) {
		logger::warn("[OAR-Cache] Invalid resource size ({}) for: '{}'", bufferInfo.fileSize, a_resourcePath);
		return false;
	}
	if (TryRebindCached(a_suffix, a_resourcePath, false,
		static_cast<uint64_t>(bufferInfo.fileSize), {}, a_owner, a_priority, a_preserveExtractedMotion)) {
		return true;
	}

	std::vector<uint8_t> bytes(bufferInfo.fileSize);
	const auto bytesRead = stream.binary_read(bytes.data(), bytes.size());
	if (bytesRead != bytes.size()) {
		logger::warn("[OAR-Cache] Failed to read resource '{}' ({} / {} bytes)",
			a_resourcePath, bytesRead, bytes.size());
		return false;
	}

	return LoadAnimationBytes(a_suffix, a_resourcePath, std::move(bytes), false,
		static_cast<uint64_t>(bufferInfo.fileSize), {}, a_owner, a_priority, a_preserveExtractedMotion);
}

bool AnimationCache::ReadArchiveTextFile(const std::string& a_resourcePath, std::string& a_out)
{
	// SEH-guard the BA2 stream open exactly like LoadAnimationResource: an access
	// violation raised in the game's resource-system ctor during startup archive
	// registration is NOT catchable by a C++ try/catch (which is why the previous
	// try/catch here did not prevent the load crash).
	__try {
		return DoReadArchiveTextFileUnsafe(a_resourcePath, a_out);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return ReportResourceFaultAndFail("archive config", a_resourcePath);
	}
}

bool AnimationCache::DoReadArchiveTextFileUnsafe(const std::string& a_resourcePath, std::string& a_out)
{
	RE::BSResourceNiBinaryStream stream(a_resourcePath.c_str(), false, nullptr, true);
	if (!stream) {
		return false;
	}

	RE::NiBinaryStream::BufferInfo bufferInfo{};
	stream.GetBufferInfo(bufferInfo);
	// OAR config/user JSON files are tiny; cap generously to reject anything
	// that is clearly not a config (or a corrupt size) without allocating huge.
	if (bufferInfo.fileSize == 0 || bufferInfo.fileSize > 16 * 1024 * 1024) {
		return false;
	}

	std::string bytes(bufferInfo.fileSize, '\0');
	const auto bytesRead = stream.binary_read(bytes.data(), bytes.size());
	if (bytesRead != bytes.size()) {
		return false;
	}

	a_out = std::move(bytes);
	return true;
}

bool AnimationCache::LoadAnimationBytes(const std::string& a_suffix, std::string a_sourceIdentity,
	std::vector<uint8_t>&& a_bytes, bool a_hasFileMTime, uint64_t a_sourceSize,
	std::filesystem::file_time_type a_sourceMTime, const void* a_owner, int32_t a_priority,
	bool a_preserveExtractedMotion)
{
	const auto sourceSize = a_sourceSize != 0 ? a_sourceSize : static_cast<uint64_t>(a_bytes.size());
	if (a_bytes.size() < 64 || a_bytes.size() > 50 * 1024 * 1024) {
		logger::warn("[OAR-Cache] Invalid source size ({}) for: '{}'", a_bytes.size(), a_sourceIdentity);
		return false;
	}

	if (TryRebindCached(a_suffix, a_sourceIdentity, a_hasFileMTime, sourceSize,
		a_sourceMTime, a_owner, a_priority, a_preserveExtractedMotion)) {
		return true;
	}

	auto entry = std::make_unique<CachedAnimation>();
	entry->filePath = std::move(a_sourceIdentity);
	entry->owner = a_owner;
	entry->priority = a_priority;
	entry->preserveExtractedMotion = a_preserveExtractedMotion;
	entry->fileSize = sourceSize;
	entry->fileMTime = a_sourceMTime;
	entry->hasFileMTime = a_hasFileMTime;
	entry->fileData = std::move(a_bytes);

	auto magic = *reinterpret_cast<uint32_t*>(entry->fileData.data());

	bool parsed = false;
	if (magic == kPackfileMagic0) {
		parsed = ParsePackfile(*entry);
	} else if (magic == kTagfileMagic) {
		parsed = ParseTagfile(*entry);
	} else {
		logger::warn("[OAR-Cache] Unknown file format (magic=0x{:08X}) for: '{}'", magic, entry->filePath);
		return false;
	}

	if (!parsed || !entry->animation) {
		logger::warn("[OAR-Cache] Failed to parse animation from: '{}'", entry->filePath);
		return false;
	}

	if (VerboseCacheLog()) {
		OAR_VLOG("[OAR-Cache] Loaded '{}': duration={:.3f}s, tracks={}, floats={}, prio={}, path='{}'",
			a_suffix, entry->duration, entry->numTransformTracks, entry->numFloatTracks, a_priority,
			entry->filePath);
	}

	OpenAnimationReplacer::GetSingleton()->loadingLoadedAnims.fetch_add(1);

	std::unique_lock lock(m_mutex);
	auto& files = m_cache[a_suffix];
	// Path identity: a same-path entry here means the file CHANGED on disk
	// (the unchanged case re-bound and returned above). Replace it in place —
	// but retire its clone AND its fileData first: live clips may still hold
	// pointers into both (the clone's spline data targets fileData, and clones
	// retired on earlier invalidations point into it too).
	for (auto& existing : files) {
		if (existing && existing->filePath == entry->filePath) {
			RetireCloneLocked(*existing, /*a_retireBackingData=*/true, a_suffix);
			existing = std::move(entry);
			std::ranges::stable_sort(files, [](const auto& a, const auto& b) {
				return (a ? a->priority : INT32_MIN) > (b ? b->priority : INT32_MIN);
			});
			OAR_VLOG("[OAR-Cache] Replaced changed file for '{}' (old entry retired)", a_suffix);
			return true;
		}
	}
	// Keep the list priority-sorted (highest first) so index 0 is the default
	// choice for callers that don't know the winning SubMod yet (pre-swap).
	auto insertAt = std::ranges::find_if(files, [&](const auto& e) {
		return !e || e->priority < entry->priority;
	});
	files.insert(insertAt, std::move(entry));
	return true;
}

bool AnimationCache::TryRebindCached(const std::string& a_suffix,
	std::string_view a_sourceIdentity, bool a_hasFileMTime, std::uint64_t a_sourceSize,
	std::filesystem::file_time_type a_sourceMTime, const void* a_owner, int32_t a_priority,
	bool a_preserveExtractedMotion)
{
	std::unique_lock lock(m_mutex);
	auto it = m_cache.find(a_suffix);
	if (it == m_cache.end()) return false;

	for (auto& existing : it->second) {
		if (!existing || existing->filePath != a_sourceIdentity) continue;
		const bool timestampMatches = a_hasFileMTime
			? existing->hasFileMTime && existing->fileMTime == a_sourceMTime
			: !existing->hasFileMTime;
		if (existing->fileSize != a_sourceSize || !timestampMatches) continue;

		existing->owner = a_owner;
		existing->priority = a_priority;
		existing->preserveExtractedMotion = a_preserveExtractedMotion;
		existing->pendingRebind = false;
		for (const auto& clone : existing->clones) {
			if (const auto lookup = m_cloneLookup.find(clone.clone); lookup != m_cloneLookup.end()) {
				lookup->second.suffix = a_suffix;
				lookup->second.owner = existing->owner;
				lookup->second.retired = false;
			}
		}
		std::ranges::stable_sort(it->second, [](const auto& a, const auto& b) {
			return (a ? a->priority : INT32_MIN) > (b ? b->priority : INT32_MIN);
		});
		OpenAnimationReplacer::GetSingleton()->loadingLoadedAnims.fetch_add(1);
		return true;
	}
	return false;
}

// Select the cached file for a suffix: the given owner's file when present,
// otherwise the highest-priority file (index 0). Caller must hold m_mutex.
AnimationCache::CachedAnimation* AnimationCache::SelectEntry(const std::string& a_suffix, const void* a_owner) const
{
	auto it = m_cache.find(a_suffix);
	if (it == m_cache.end() || it->second.empty()) return nullptr;

	if (a_owner) {
		for (auto& entry : it->second) {
			if (entry && entry->owner == a_owner) return entry.get();
		}
		// The winner's own file isn't cached under this suffix (failed to
		// load/parse). Fall back to the highest-priority file — matches the
		// pre-per-file-cache behavior of always having SOME file to play.
		static std::atomic<int> s_fallbackLog{ 0 };
		if (s_fallbackLog.fetch_add(1, std::memory_order_relaxed) < 20) {
			logger::warn("[OAR-Cache] No cached file for suffix '{}' owned by {:X} — using highest-priority file '{}'",
				a_suffix, reinterpret_cast<uintptr_t>(a_owner),
				it->second[0] ? it->second[0]->filePath : "(null)");
		}
	}
	return it->second[0].get();
}

void AnimationCache::SetVtableFromGame(uintptr_t a_vtable)
{
	uintptr_t prev = m_gameAnimVtable.exchange(a_vtable);
	if (prev != 0) return;

	std::shared_lock lock(m_mutex);
	int patched = 0;
	int referenceFramePatched = 0;
	for (auto& [key, files] : m_cache) {
		for (auto& entry : files) {
			if (!entry || entry->fileData.empty()) continue;
			uint8_t* sectionData = entry->fileData.data() + entry->sectionFileOffset;
			for (const auto& fixup : entry->vtableFixups) {
				const auto vtable = ResolveVtable(fixup.kind, a_vtable);
				if (vtable == 0) continue;
				*reinterpret_cast<uintptr_t*>(sectionData + fixup.offset) = vtable;
				if (fixup.kind == CachedAnimation::VtableFixupKind::kGameAnimation) {
					patched++;
				} else {
					referenceFramePatched++;
				}
			}
		}
	}
	if (patched > 0 || referenceFramePatched > 0) {
		logger::info("[OAR-Cache] Retroactively patched {} animation and {} reference-frame vtable slots across {} cached suffixes",
			patched, referenceFramePatched, m_cache.size());
	}
}

RE::hkaAnimation* AnimationCache::GetCachedAnimation(const std::string& a_suffix, const void* a_owner) const
{
	std::shared_lock lock(m_mutex);
	if (auto* entry = SelectEntry(a_suffix, a_owner)) {
		return entry->animation;
	}
	return nullptr;
}

RE::hkaAnimation* AnimationCache::GetOrBuildRuntimeAnim(const std::string& a_suffix, RE::hkaAnimation* a_gameAnim,
	const void* a_owner)
{
	std::unique_lock lock(m_mutex);
	auto* selected = SelectEntry(a_suffix, a_owner);
	if (!selected) return nullptr;

	auto& entry = *selected;

	// One clone per distinct game original (see RuntimeClone in the header):
	// a Leaf Matching file serves several originals AT THE SAME TIME (both
	// perspective graphs, multiple weapons), and per-frame callers alternate
	// originals — the old one-clone-per-entry rebuild would thrash every frame.
	if (a_gameAnim) {
		for (size_t i = 0; i < entry.clones.size(); ++i) {
			auto& rc = entry.clones[i];
			if (rc.gameOriginal != a_gameAnim) continue;
			// Address-reuse guard: the engine can free an original and hand the
			// same address to a different animation (weapon switch). Compare
			// the original's duration + track count against build time; on
			// mismatch retire this clone (a clip may still play it) and rebuild.
			const auto* origBytes = reinterpret_cast<const uint8_t*>(a_gameAnim);
			const float dur = *reinterpret_cast<const float*>(origBytes + 0x14);
			const int32_t trk = *reinterpret_cast<const int32_t*>(origBytes + 0x18);
			if (dur == rc.originalDuration && trk == rc.originalNumTracks) {
				return rc.clone;
			}
			OAR_VLOG("[OAR-Cache] Original address {:X} reused by a different animation for '{}' (dur {:.3f}->{:.3f} tracks {}->{}) — retiring stale clone",
				reinterpret_cast<uintptr_t>(a_gameAnim), a_suffix,
				rc.originalDuration, dur, rc.originalNumTracks, trk);
			RetireSingleCloneLocked(entry, i, a_suffix);
			break;
		}
	} else if (!entry.clones.empty()) {
		// Legacy callers pass no original: hand back the freshest clone.
		return entry.clones.back().clone;
	}

	if (!a_gameAnim || !entry.animation) return nullptr;

	// Bound the per-entry clone set. Path-scoped entries only ever hold one
	// live clone (weapon switches retire via the address-reuse guard or leave
	// stale-original clones that simply stop being requested); Leaf Matching
	// entries hold one per original. Retire the oldest on overflow.
	static constexpr size_t kMaxClonesPerEntry = 16;
	while (entry.clones.size() >= kMaxClonesPerEntry) {
		RetireSingleCloneLocked(entry, 0, a_suffix);
	}

	// Clone the game's animation struct (guaranteed correct layout) then patch data pointers.
	// Use 0x100 bytes to cover the full hkaSplineCompressedAnimation struct with margin.
	static constexpr size_t kStructSize = 0x100;
	CachedAnimation::RuntimeClone newClone;
	newClone.structBuffer.resize(kStructSize + 16, 0);

	// Ensure 16-byte alignment for SIMD
	uintptr_t rawAddr = reinterpret_cast<uintptr_t>(newClone.structBuffer.data());
	uintptr_t aligned = (rawAddr + 15) & ~uintptr_t(15);
	uint8_t* cloneBase = reinterpret_cast<uint8_t*>(aligned);

	// Copy the game's animation struct
	std::memcpy(cloneBase, reinterpret_cast<uint8_t*>(a_gameAnim), kStructSize);

	// Now patch the clone with our animation's data pointers
	auto* ourBytes = reinterpret_cast<uint8_t*>(entry.animation);

	// Patch m_data (hkArray<uint8> at +0x98): pointer, size, capacity
	*reinterpret_cast<uintptr_t*>(cloneBase + 0x98) = *reinterpret_cast<uintptr_t*>(ourBytes + 0x98);
	*reinterpret_cast<int32_t*>(cloneBase + 0xA0) = *reinterpret_cast<int32_t*>(ourBytes + 0xA0);
	*reinterpret_cast<uint32_t*>(cloneBase + 0xA4) = *reinterpret_cast<uint32_t*>(ourBytes + 0xA4);

	// Patch m_blockOffsets (hkArray<uint32> at +0x58)
	*reinterpret_cast<uintptr_t*>(cloneBase + 0x58) = *reinterpret_cast<uintptr_t*>(ourBytes + 0x58);
	*reinterpret_cast<int32_t*>(cloneBase + 0x60) = *reinterpret_cast<int32_t*>(ourBytes + 0x60);
	*reinterpret_cast<uint32_t*>(cloneBase + 0x64) = *reinterpret_cast<uint32_t*>(ourBytes + 0x64);

	// Patch m_floatBlockOffsets (hkArray<uint32> at +0x68)
	*reinterpret_cast<uintptr_t*>(cloneBase + 0x68) = *reinterpret_cast<uintptr_t*>(ourBytes + 0x68);
	*reinterpret_cast<int32_t*>(cloneBase + 0x70) = *reinterpret_cast<int32_t*>(ourBytes + 0x70);
	*reinterpret_cast<uint32_t*>(cloneBase + 0x74) = *reinterpret_cast<uint32_t*>(ourBytes + 0x74);

	// Parse annotation tracks from the replacement animation BEFORE clearing them.
	// hkaAnimation layout: +0x28 = hkArray<hkaAnnotationTrack> annotationTracks
	//   hkArray layout: +0x00 = data ptr, +0x08 = size (int32), +0x0C = capacityAndFlags
	// hkaAnnotationTrack layout: +0x00 = trackName (hkStringPtr, 8 bytes)
	//                            +0x08 = hkArray<Annotation> annotations
	// Annotation layout: +0x00 = time (float), +0x04 = pad, +0x08 = text (hkStringPtr)
	if (entry.annotations.empty()) {
		auto* annotTrackPtr = *reinterpret_cast<uint8_t**>(ourBytes + 0x28);
		int32_t annotTrackCount = *reinterpret_cast<int32_t*>(ourBytes + 0x30);

		if (annotTrackPtr && annotTrackCount > 0 &&
			reinterpret_cast<uintptr_t>(annotTrackPtr) > 0x10000) {

			constexpr size_t kAnnotTrackSize = 0x18; // hkStringPtr(8) + hkArray(16)
			constexpr size_t kAnnotationSize = 0x10; // float(4) + pad(4) + hkStringPtr(8)

			for (int32_t t = 0; t < annotTrackCount; ++t) {
				auto* trackBase = annotTrackPtr + (t * kAnnotTrackSize);
				// annotations hkArray at trackBase + 0x08
				auto* annots = *reinterpret_cast<uint8_t**>(trackBase + 0x08);
				int32_t annotCount = *reinterpret_cast<int32_t*>(trackBase + 0x10);
				if (!annots || annotCount <= 0 ||
					reinterpret_cast<uintptr_t>(annots) < 0x10000) continue;

				for (int32_t a = 0; a < annotCount; ++a) {
					auto* annBase = annots + (a * kAnnotationSize);
					float annTime = *reinterpret_cast<float*>(annBase + 0x00);
					auto* txtPtr = *reinterpret_cast<const char**>(annBase + 0x08);
					// hkStringPtr stores the pointer with bit 0 as a flag
					auto rawTxt = reinterpret_cast<uintptr_t>(txtPtr) & ~uintptr_t(1);
					auto* txt = reinterpret_cast<const char*>(rawTxt);
					if (txt && rawTxt > 0x10000 && txt[0] != '\0') {
						entry.annotations.push_back({ annTime, std::string(txt) });
					}
				}
			}
			if (!entry.annotations.empty()) {
				std::ranges::sort(entry.annotations,
					[](const auto& a, const auto& b) { return a.time < b.time; });
				OAR_VLOG("[OAR-Cache] Parsed {} annotations from replacement '{}'",
					entry.annotations.size(), entry.filePath);
				for (auto& pa : entry.annotations) {
					OAR_VLOG("[OAR-Cache]   t={:.4f}s  '{}'", pa.time, pa.text);
				}
			}
		}
	}

	// Patch ALL stale pointers on the clone. The clone was memcpy'd from the game's
	// original animation, so any pointer fields reference game memory that gets freed
	// on weapon switch. We must eliminate all stale references.

	// 1. m_extractedMotion at +0x20: points to hkaAnimatedReferenceFrame.
	//    Replacement packfiles have already had their local/global fixups applied
	//    into entry.fileData. Preserve that pointer only when the SubMod opts in;
	//    the entry and its retired backing buffer keep the pointed-to object alive.
	const auto extractedMotion = *reinterpret_cast<uintptr_t*>(ourBytes + 0x20);
	const auto fileBegin = reinterpret_cast<uintptr_t>(entry.fileData.data());
	const auto fileEnd = fileBegin + entry.fileData.size();
	const bool extractedMotionInBackingFile = extractedMotion >= fileBegin && extractedMotion < fileEnd;
	if (entry.preserveExtractedMotion && extractedMotionInBackingFile) {
		*reinterpret_cast<uintptr_t*>(cloneBase + 0x20) = extractedMotion;
		logger::info("[OAR-Motion] Preserved extractedMotion for '{}' (reference={:X})",
			entry.filePath, extractedMotion);
	} else {
		*reinterpret_cast<uintptr_t*>(cloneBase + 0x20) = 0;
		if (entry.preserveExtractedMotion && extractedMotion != 0) {
			logger::warn("[OAR-Motion] Skipped extractedMotion for '{}' because the reference is outside the backing HKX buffer ({:X})",
				entry.filePath, extractedMotion);
		}
	}

	// 2. annotationTracks at +0x28: points to game's annotation data.
	//    Use a safe dummy pointer with size=0 and DONT_DEALLOCATE.
	{
		static uint8_t* s_dummyAnnotData = nullptr;
		if (!s_dummyAnnotData) {
			s_dummyAnnotData = new uint8_t[64]();
		}
		*reinterpret_cast<uintptr_t*>(cloneBase + 0x28) = reinterpret_cast<uintptr_t>(s_dummyAnnotData);
		*reinterpret_cast<int32_t*>(cloneBase + 0x30) = 0;            // size = 0
		*reinterpret_cast<uint32_t*>(cloneBase + 0x34) = 0x80000000u; // DONT_DEALLOCATE
	}

	OAR_VLOG("[OAR-Annot] Clone '{}': patched extractedMotion+annotationTracks, {} parsed replacement annotations (using trigger NULLing + manual firing)",
		entry.filePath, entry.annotations.size());

	// Clear m_transformOffsets (let game compute at runtime - game knows its own format)
	*reinterpret_cast<uintptr_t*>(cloneBase + 0x78) = 0;
	*reinterpret_cast<int32_t*>(cloneBase + 0x80) = 0;
	*reinterpret_cast<uint32_t*>(cloneBase + 0x84) = 0;

	// Clear m_floatOffsets (same - let game compute)
	*reinterpret_cast<uintptr_t*>(cloneBase + 0x88) = 0;
	*reinterpret_cast<int32_t*>(cloneBase + 0x90) = 0;
	*reinterpret_cast<uint32_t*>(cloneBase + 0x94) = 0;

	// Patch scalar fields that may differ between game's original and our replacement
	*reinterpret_cast<float*>(cloneBase + 0x14) = entry.duration;
	*reinterpret_cast<int32_t*>(cloneBase + 0x18) = entry.numTransformTracks;
	*reinterpret_cast<int32_t*>(cloneBase + 0x1C) = entry.numFloatTracks;
	*reinterpret_cast<int32_t*>(cloneBase + 0x38) = *reinterpret_cast<int32_t*>(ourBytes + 0x38); // numFrames
	*reinterpret_cast<int32_t*>(cloneBase + 0x3C) = *reinterpret_cast<int32_t*>(ourBytes + 0x3C); // numBlocks
	*reinterpret_cast<int32_t*>(cloneBase + 0x40) = *reinterpret_cast<int32_t*>(ourBytes + 0x40); // maxFramesPerBlock
	*reinterpret_cast<int32_t*>(cloneBase + 0x44) = *reinterpret_cast<int32_t*>(ourBytes + 0x44); // maskAndQuantSz
	*reinterpret_cast<float*>(cloneBase + 0x48) = *reinterpret_cast<float*>(ourBytes + 0x48);     // blockDuration
	*reinterpret_cast<float*>(cloneBase + 0x4C) = *reinterpret_cast<float*>(ourBytes + 0x4C);     // blockInverseDuration
	*reinterpret_cast<float*>(cloneBase + 0x50) = *reinterpret_cast<float*>(ourBytes + 0x50);     // frameDuration

	// Zero all unknown pointer fields beyond patched regions (0xA8-0xFF range).
	// The clone was memcpy'd from the game animation, so these hold pointers into game
	// memory that gets freed on weapon switch. Zeroing prevents stale-pointer traversal.
	// m_data ends at 0xA8 (ptr+size+cap = 0x98+0x10). Everything from 0xA8 onward is
	// uncharted territory in the struct — zero it all to be safe.
	std::memset(cloneBase + 0xA8, 0, kStructSize - 0xA8);

	newClone.clone = reinterpret_cast<RE::hkaAnimation*>(cloneBase);
	newClone.gameOriginal = a_gameAnim;
	{
		const auto* origBytes = reinterpret_cast<const uint8_t*>(a_gameAnim);
		newClone.originalDuration = *reinterpret_cast<const float*>(origBytes + 0x14);
		newClone.originalNumTracks = *reinterpret_cast<const int32_t*>(origBytes + 0x18);
	}
	entry.clones.push_back(std::move(newClone));
	const auto& registeredClone = entry.clones.back();
	m_cloneLookup[registeredClone.clone] = CloneLookup{
		.gameOriginal = registeredClone.gameOriginal,
		.originalDuration = registeredClone.originalDuration,
		.originalNumTracks = registeredClone.originalNumTracks,
		.suffix = a_suffix,
		.owner = entry.owner,
		.retired = false
	};

	OAR_VLOG("[OAR-Cache] Built runtime clone for '{}': base={:X} gameStruct={:X} file='{}' ({} clone(s) live)",
		a_suffix, reinterpret_cast<uintptr_t>(cloneBase), reinterpret_cast<uintptr_t>(a_gameAnim),
		entry.filePath, entry.clones.size());

	return entry.clones.back().clone;
}

const std::vector<AnimationCache::ParsedAnnotation>* AnimationCache::GetAnnotations(const std::string& a_suffix,
	const void* a_owner) const
{
	std::shared_lock lock(m_mutex);
	// Annotations must come from the SAME file that is playing — never fall
	// back to another SubMod's annotations, so require the selected entry to
	// have parsed annotations itself (empty = no manual firing for this file).
	if (auto* entry = SelectEntry(a_suffix, a_owner); entry && !entry->annotations.empty()) {
		return &entry->annotations;
	}
	return nullptr;
}

bool AnimationCache::GetDonorTrackMap(const std::string& a_suffix, const void* a_owner,
	std::vector<int16_t>& a_outMap, bool& a_outIdentity) const
{
	std::shared_lock lock(m_mutex);
	auto* entry = SelectEntry(a_suffix, a_owner);
	if (!entry) return false;
	if (!entry->trackToBoneIndices.empty()) {
		a_outMap = entry->trackToBoneIndices;
		a_outIdentity = false;
		return true;
	}
	if (entry->bindingIdentity) {
		a_outMap.clear();
		a_outIdentity = true;
		return true;
	}
	return false;
}

size_t AnimationCache::GetCacheSize() const
{
	std::shared_lock lock(m_mutex);
	size_t total = 0;
	for (auto& [key, files] : m_cache) {
		total += files.size();
	}
	return total;
}

bool AnimationCache::IsOurReplacement(RE::hkaAnimation* a_anim) const
{
	if (!a_anim) return false;
	std::shared_lock lock(m_mutex);
	return m_cloneLookup.contains(a_anim);
}

bool AnimationCache::GetReplacementIdentity(RE::hkaAnimation* a_anim, std::string& a_outSuffix,
	const void*& a_outOwner) const
{
	if (!a_anim) return false;
	std::shared_lock lock(m_mutex);
	const auto it = m_cloneLookup.find(a_anim);
	if (it == m_cloneLookup.end()) return false;
	if (it->second.retired && it->second.suffix.empty()) return false;
	a_outSuffix = it->second.suffix;
	a_outOwner = it->second.owner;
	return true;
}

RE::hkaAnimation* AnimationCache::GetGameOriginalForSuffix(const std::string& a_suffix) const
{
	std::shared_lock lock(m_mutex);
	auto it = m_cache.find(a_suffix);
	if (it == m_cache.end()) return nullptr;
	for (auto& entry : it->second) {
		// Clones append in build order, so the back is the freshest original.
		if (entry && !entry->clones.empty()) return entry->clones.back().gameOriginal;
	}
	return nullptr;
}

RE::hkaAnimation* AnimationCache::GetOriginalFromReplacement(RE::hkaAnimation* a_replacement) const
{
	if (!a_replacement) return nullptr;
	auto originalStillMatches = [](RE::hkaAnimation* a_original, float a_duration, int32_t a_numTracks) {
		if (!a_original || IsBadReadPtr(a_original, 0x1C)) return false;
		const auto* bytes = reinterpret_cast<const uint8_t*>(a_original);
		return *reinterpret_cast<const float*>(bytes + 0x14) == a_duration &&
			*reinterpret_cast<const int32_t*>(bytes + 0x18) == a_numTracks;
	};
	std::shared_lock lock(m_mutex);
	const auto it = m_cloneLookup.find(a_replacement);
	if (it == m_cloneLookup.end()) return nullptr;
	// The recorded original may be stale (freed on weapon switch while the
	// clone stays live for other originals). Callers validate the returned
	// pointer before use, and this preserves the old duration/track guard.
	return originalStillMatches(it->second.gameOriginal, it->second.originalDuration,
		it->second.originalNumTracks) ? it->second.gameOriginal : nullptr;
}

RE::hkaAnimation* AnimationCache::GetOriginalFromRetiredReplacement(RE::hkaAnimation* a_replacement) const
{
	if (!a_replacement) return nullptr;
	auto originalStillMatches = [](RE::hkaAnimation* a_original, float a_duration, int32_t a_numTracks) {
		if (!a_original || IsBadReadPtr(a_original, 0x1C)) return false;
		const auto* bytes = reinterpret_cast<const uint8_t*>(a_original);
		return *reinterpret_cast<const float*>(bytes + 0x14) == a_duration &&
			*reinterpret_cast<const int32_t*>(bytes + 0x18) == a_numTracks;
	};

	std::shared_lock lock(m_mutex);
	const auto it = m_cloneLookup.find(a_replacement);
	if (it == m_cloneLookup.end() || !it->second.retired) return nullptr;
	return originalStillMatches(it->second.gameOriginal, it->second.originalDuration,
		it->second.originalNumTracks) ? it->second.gameOriginal : nullptr;
}

void AnimationCache::RetireSingleCloneLocked(CachedAnimation& a_entry, size_t a_index,
	const std::string& a_suffix)
{
	if (a_index >= a_entry.clones.size()) return;
	auto& rc = a_entry.clones[a_index];
	const auto clonePtr = rc.clone;
	m_cloneLookup.erase(clonePtr);
	if (!rc.structBuffer.empty()) {
		constexpr size_t kMaxRetiredClones = 256;
		if (m_retiredClones.size() >= kMaxRetiredClones) {
			if (const auto retiredPtr = m_retiredClones.front().clonePtr; retiredPtr) {
				m_cloneLookup.erase(retiredPtr);
			}
			m_retiredClones.erase(m_retiredClones.begin());
		}
		RetiredClone rec;
		rec.buffer = std::move(rc.structBuffer);
		rec.clonePtr = rc.clone;
		rec.gameOriginal = rc.gameOriginal;
		rec.originalDuration = rc.originalDuration;
		rec.originalNumTracks = rc.originalNumTracks;
		rec.suffix = a_suffix;
		rec.owner = a_entry.owner;
		m_retiredClones.push_back(std::move(rec));
		m_cloneLookup[clonePtr] = CloneLookup{
			.gameOriginal = rc.gameOriginal,
			.originalDuration = rc.originalDuration,
			.originalNumTracks = rc.originalNumTracks,
			.suffix = a_suffix,
			.owner = a_entry.owner,
			.retired = true
		};
	}
	a_entry.clones.erase(a_entry.clones.begin() + a_index);
}

void AnimationCache::RetireCloneLocked(CachedAnimation& a_entry, bool a_retireBackingData,
	const std::string& a_suffix)
{
	// See header comment: the buffers must survive intact because active clips
	// may still hold pointers into them. Move every clone to the keep-alive
	// list instead of clearing in place. When the entry itself is going away
	// (a_retireBackingData), its fileData must be kept alive too — even if the
	// entry has no CURRENT clone, clones retired on earlier invalidations
	// still point into that buffer.
	while (!a_entry.clones.empty()) {
		RetireSingleCloneLocked(a_entry, a_entry.clones.size() - 1, a_suffix);
	}
	const bool retireData = a_retireBackingData && !a_entry.fileData.empty();
	if (retireData) {
		constexpr size_t kMaxRetiredClones = 256;
		if (m_retiredClones.size() >= kMaxRetiredClones) {
			if (const auto retiredPtr = m_retiredClones.front().clonePtr; retiredPtr) {
				m_cloneLookup.erase(retiredPtr);
			}
			m_retiredClones.erase(m_retiredClones.begin());
		}
		RetiredClone rec;
		rec.suffix = a_suffix;
		rec.owner = a_entry.owner;
		rec.backingFileData = std::move(a_entry.fileData);
		a_entry.fileData = std::vector<uint8_t>{};
		m_retiredClones.push_back(std::move(rec));
	}
}

void AnimationCache::MarkAllForRebind()
{
	std::unique_lock lock(m_mutex);
	for (auto& [key, files] : m_cache) {
		for (auto& entry : files) {
			if (entry) entry->pendingRebind = true;
		}
	}
}

size_t AnimationCache::PruneUnrebound()
{
	std::unique_lock lock(m_mutex);
	size_t pruned = 0;
	for (auto it = m_cache.begin(); it != m_cache.end();) {
		auto& files = it->second;
		std::erase_if(files, [&](std::unique_ptr<CachedAnimation>& entry) {
			if (!entry || !entry->pendingRebind) return false;
			// The submod that owned this file no longer exists after the
			// reload. Retire the clone and the backing file bytes (live clips
			// may still reference them), then drop the entry.
			RetireCloneLocked(*entry, /*a_retireBackingData=*/true, it->first);
			pruned++;
			return true;
		});
		it = files.empty() ? m_cache.erase(it) : std::next(it);
	}
	return pruned;
}

void AnimationCache::InvalidateRuntimeClones()
{
	std::unique_lock lock(m_mutex);
	int count = 0;
	for (auto& [key, files] : m_cache) {
		for (auto& entry : files) {
			if (entry && !entry->clones.empty()) {
				count += static_cast<int>(entry->clones.size());
				RetireCloneLocked(*entry, /*a_retireBackingData=*/false, key);
			}
		}
	}
	if (count > 0) {
		OAR_VLOG("[OAR-Cache] Invalidated {} runtime clones (retired buffers kept alive: {})",
			count, m_retiredClones.size());
	}
}

void AnimationCache::Clear()
{
	std::unique_lock lock(m_mutex);
	// Retired clones remain in their keep-alive list, so only remove live-clone
	// identities before dropping the cached file entries.
	for (auto& [key, files] : m_cache) {
		for (auto& entry : files) {
			if (!entry) continue;
			for (const auto& clone : entry->clones) {
				m_cloneLookup.erase(clone.clone);
			}
		}
	}
	m_cache.clear();
}

bool AnimationCache::ParsePackfile(CachedAnimation& a_entry)
{
	auto* data = a_entry.fileData.data();
	auto dataSize = a_entry.fileData.size();

	if (dataSize < sizeof(HkxPackfileHeader)) return false;

	auto* header = reinterpret_cast<HkxPackfileHeader*>(data);

	if (header->magic0 != kPackfileMagic0 || header->magic1 != kPackfileMagic1) {
		logger::warn("[OAR-Cache] Bad packfile magic");
		return false;
	}

	bool isV11 = (header->fileVersion >= kVersion11);

	const bool verbose = VerboseCacheLog();
	if (verbose) {
		OAR_VLOG("[OAR-Cache] Packfile: ver=0x{:X}, ptrSize={}, numSec={}, contIdx={}, contOff={}, v11={}",
			header->fileVersion, header->pointerSize,
			header->numSections, header->contentsSectionIndex, header->contentsSectionOffset, isV11);
	}

	uint32_t numSections = header->numSections;
	if (numSections < 1 || numSections > 10) {
		logger::warn("[OAR-Cache] Invalid section count: {}", numSections);
		return false;
	}

	// paddingAfter is stored in predicateArraySizePlusPadding (0x10 for anim files, 0 otherwise)
	size_t sectionHeadersStart = sizeof(HkxPackfileHeader) + header->predicateArraySizePlusPadding;
	size_t sectionHeaderSize = isV11 ? sizeof(HkxSectionHeader_v11) : sizeof(HkxSectionHeader_v8);

	if (sectionHeadersStart + numSections * sectionHeaderSize > dataSize) {
		logger::warn("[OAR-Cache] Section headers exceed file size");
		return false;
	}

	// Parse section headers into a uniform format
	struct SectionInfo {
		std::string tag;
		uint32_t absoluteDataStart;
		uint32_t localFixupsOffset;
		uint32_t globalFixupsOffset;
		uint32_t virtualFixupsOffset;
		uint32_t exportsOffset;
		uint32_t importsOffset;
		uint32_t endOffset;
	};
	std::vector<SectionInfo> sections(numSections);

	for (uint32_t i = 0; i < numSections; i++) {
		uint8_t* secPtr = data + sectionHeadersStart + i * sectionHeaderSize;
		if (isV11) {
			auto* s = reinterpret_cast<HkxSectionHeader_v11*>(secPtr);
			sections[i].tag = std::string(s->sectionTag, strnlen(s->sectionTag, 15));
			sections[i].absoluteDataStart = s->absoluteDataStart;
			sections[i].localFixupsOffset = s->localFixupsOffset;
			sections[i].globalFixupsOffset = s->globalFixupsOffset;
			sections[i].virtualFixupsOffset = s->virtualFixupsOffset;
			sections[i].exportsOffset = s->exportsOffset;
			sections[i].importsOffset = s->importsOffset;
			sections[i].endOffset = s->endOffset;
		} else {
			auto* s = reinterpret_cast<HkxSectionHeader_v8*>(secPtr);
			sections[i].tag = std::string(s->sectionTag, strnlen(s->sectionTag, 15));
			sections[i].absoluteDataStart = s->absoluteDataStart;
			sections[i].localFixupsOffset = s->localFixupsOffset;
			sections[i].globalFixupsOffset = s->globalFixupsOffset;
			sections[i].virtualFixupsOffset = s->virtualFixupsOffset;
			sections[i].exportsOffset = s->exportsOffset;
			sections[i].importsOffset = s->importsOffset;
			sections[i].endOffset = s->endOffset;
		}
		if (verbose) {
			OAR_VLOG("[OAR-Cache]   Section[{}] '{}': absStart=0x{:X}, localFix=0x{:X}, globalFix=0x{:X}, virtFix=0x{:X}, end=0x{:X}",
				i, sections[i].tag, sections[i].absoluteDataStart,
				sections[i].localFixupsOffset, sections[i].globalFixupsOffset,
				sections[i].virtualFixupsOffset, sections[i].endOffset);
		}
	}

	// Find the __data__ section (contents section)
	uint32_t dataSectionIdx = header->contentsSectionIndex;
	if (dataSectionIdx >= numSections) dataSectionIdx = numSections - 1;

	auto& ds = sections[dataSectionIdx];

	// absoluteDataStart is the file offset where this section's payload begins
	uint32_t sectionFileOffset = ds.absoluteDataStart;
	if (sectionFileOffset == 0 || sectionFileOffset >= dataSize) {
		// Fallback: calculate from header layout
		sectionFileOffset = static_cast<uint32_t>(sectionHeadersStart + numSections * sectionHeaderSize);
		// Align to 16 bytes
		sectionFileOffset = (sectionFileOffset + 0xF) & ~0xFu;
		logger::warn("[OAR-Cache] absoluteDataStart invalid, using calculated offset 0x{:X}", sectionFileOffset);
	}

	// The payload size is from the start of data to localFixupsOffset
	// All fixup offsets are RELATIVE to the section's payload start
	uint32_t payloadSize = ds.localFixupsOffset;
	if (payloadSize == 0 || payloadSize == 0xFFFFFFFF) {
		payloadSize = ds.endOffset;
	}
	if (payloadSize == 0 || payloadSize == 0xFFFFFFFF || sectionFileOffset + payloadSize > dataSize) {
		payloadSize = static_cast<uint32_t>(dataSize - sectionFileOffset);
	}

	if (sectionFileOffset + payloadSize > dataSize) {
		logger::warn("[OAR-Cache] Section payload exceeds file bounds");
		return false;
	}

	uint8_t* sectionData = data + sectionFileOffset;
	size_t sectionSize = payloadSize;
	a_entry.sectionFileOffset = sectionFileOffset;

	if (verbose) {
		OAR_VLOG("[OAR-Cache] Data section payload: fileOffset=0x{:X}, size={} bytes", sectionFileOffset, sectionSize);
	}

	// === Apply local fixups ===
	// Local fixups are 8-byte records (src_u32, dst_u32) stored at section offset localFixupsOffset
	// Each says: write absolute pointer to (sectionData + dst) at location (sectionData + src)
	int localFixCount = 0;
	if (ds.localFixupsOffset != 0 && ds.localFixupsOffset != 0xFFFFFFFF) {
		uint32_t fixFileStart = sectionFileOffset + ds.localFixupsOffset;
		uint32_t fixFileEnd = sectionFileOffset + ds.globalFixupsOffset;
		if (ds.globalFixupsOffset == 0 || ds.globalFixupsOffset == 0xFFFFFFFF)
			fixFileEnd = sectionFileOffset + ds.virtualFixupsOffset;
		if (ds.virtualFixupsOffset == 0 || ds.virtualFixupsOffset == 0xFFFFFFFF)
			fixFileEnd = sectionFileOffset + ds.endOffset;
		if (fixFileEnd == 0 || fixFileEnd == 0xFFFFFFFF || fixFileEnd > dataSize)
			fixFileEnd = static_cast<uint32_t>(dataSize);

		if (fixFileStart < dataSize && fixFileEnd <= dataSize && fixFileStart < fixFileEnd) {
			uint8_t* fixups = data + fixFileStart;
			size_t fixupBytes = fixFileEnd - fixFileStart;

			for (size_t i = 0; i + 8 <= fixupBytes; i += 8) {
				uint32_t src = *reinterpret_cast<uint32_t*>(fixups + i);
				uint32_t dst = *reinterpret_cast<uint32_t*>(fixups + i + 4);

				if (src == 0xFFFFFFFF || dst == 0xFFFFFFFF) continue;
				if (src + 8 > sectionSize || dst >= sectionSize) continue;

				uintptr_t absoluteDst = reinterpret_cast<uintptr_t>(sectionData) + dst;
				*reinterpret_cast<uintptr_t*>(sectionData + src) = absoluteDst;
				localFixCount++;
			}
		}
	}
	if (verbose) {
		OAR_VLOG("[OAR-Cache] Applied {} local fixups", localFixCount);
	}

	// === Apply global fixups ===
	// Global fixups are 12-byte records (src_u32, section_u32, dst_u32)
	// Each says: write absolute pointer to (targetSection + dst) at (sectionData + src)
	int globalFixCount = 0;
	if (ds.globalFixupsOffset != 0 && ds.globalFixupsOffset != 0xFFFFFFFF) {
		uint32_t fixFileStart = sectionFileOffset + ds.globalFixupsOffset;
		uint32_t fixFileEnd = sectionFileOffset + ds.virtualFixupsOffset;
		if (ds.virtualFixupsOffset == 0 || ds.virtualFixupsOffset == 0xFFFFFFFF)
			fixFileEnd = sectionFileOffset + ds.endOffset;
		if (fixFileEnd == 0 || fixFileEnd == 0xFFFFFFFF || fixFileEnd > dataSize)
			fixFileEnd = static_cast<uint32_t>(dataSize);

		if (fixFileStart < dataSize && fixFileEnd <= dataSize && fixFileStart < fixFileEnd) {
			uint8_t* fixups = data + fixFileStart;
			size_t fixupBytes = fixFileEnd - fixFileStart;

			for (size_t i = 0; i + 12 <= fixupBytes; i += 12) {
				uint32_t src = *reinterpret_cast<uint32_t*>(fixups + i);
				uint32_t targetSec = *reinterpret_cast<uint32_t*>(fixups + i + 4);
				uint32_t dst = *reinterpret_cast<uint32_t*>(fixups + i + 8);

				if (src == 0xFFFFFFFF || dst == 0xFFFFFFFF) continue;
				if (src + 8 > sectionSize) continue;
				if (targetSec >= numSections) continue;

				uint32_t targetFileOffset = sections[targetSec].absoluteDataStart;
				if (targetFileOffset == 0 || targetFileOffset >= dataSize) continue;
				if (targetFileOffset + dst >= dataSize) continue;

				uintptr_t absoluteDst = reinterpret_cast<uintptr_t>(data + targetFileOffset + dst);
				*reinterpret_cast<uintptr_t*>(sectionData + src) = absoluteDst;
				globalFixCount++;
			}
		}
	}
	if (verbose) {
		OAR_VLOG("[OAR-Cache] Applied {} global fixups", globalFixCount);
	}

	// === Apply virtual fixups (vtable patching) ===
	// Virtual fixups are 12-byte records (src_u32, section_u32, nameOffset_u32)
	// Each says: object at sectionData+src needs its vtable set. The class name
	// selects the correct Havok vtable; animation vtables are deferred until a
	// live game animation supplies the runtime-specific address.
	uintptr_t gameVtable = m_gameAnimVtable.load();
	int vtableFixCount = 0;
	int referenceFrameFixCount = 0;

	auto getVirtualClassName = [&](uint32_t a_nameOffset) -> std::string_view {
		if (header->classNameSectionIndex >= sections.size()) return {};
		const auto& classNameSection = sections[header->classNameSectionIndex];
		if (classNameSection.absoluteDataStart >= dataSize) return {};

		const uint64_t namePos = static_cast<uint64_t>(classNameSection.absoluteDataStart) + a_nameOffset;
		if (namePos >= dataSize) return {};

		uint64_t nameEnd = dataSize;
		if (classNameSection.endOffset != 0 && classNameSection.endOffset != 0xFFFFFFFF) {
			nameEnd = std::min<uint64_t>(nameEnd,
				static_cast<uint64_t>(classNameSection.absoluteDataStart) + classNameSection.endOffset);
		}
		if (nameEnd <= namePos) return {};

		const auto maxLength = static_cast<size_t>(nameEnd - namePos);
		const auto* name = reinterpret_cast<const char*>(data + namePos);
		return std::string_view(name, strnlen(name, maxLength));
	};

	if (ds.virtualFixupsOffset != 0 && ds.virtualFixupsOffset != 0xFFFFFFFF) {
		uint32_t fixFileStart = sectionFileOffset + ds.virtualFixupsOffset;
		uint32_t fixFileEnd = sectionFileOffset + ds.exportsOffset;
		if (ds.exportsOffset == 0 || ds.exportsOffset == 0xFFFFFFFF)
			fixFileEnd = sectionFileOffset + ds.importsOffset;
		if (ds.importsOffset == 0 || ds.importsOffset == 0xFFFFFFFF)
			fixFileEnd = sectionFileOffset + ds.endOffset;
		if (fixFileEnd == 0 || fixFileEnd == 0xFFFFFFFF || fixFileEnd > dataSize)
			fixFileEnd = static_cast<uint32_t>(dataSize);

		if (fixFileStart < dataSize && fixFileEnd <= dataSize && fixFileStart < fixFileEnd) {
			uint8_t* fixups = data + fixFileStart;
			size_t fixupBytes = fixFileEnd - fixFileStart;

			for (size_t i = 0; i + 12 <= fixupBytes; i += 12) {
				uint32_t src = *reinterpret_cast<uint32_t*>(fixups + i);
				uint32_t nameOffset = *reinterpret_cast<uint32_t*>(fixups + i + 8);
				if (src == 0xFFFFFFFF) continue;
				if (src + 8 > sectionSize) continue;

				const auto className = getVirtualClassName(nameOffset);
				a_entry.vtableFixups.push_back({
					src,
					className == "hkaDefaultAnimatedReferenceFrame" ? CachedAnimation::VtableFixupKind::kDefaultAnimatedReferenceFrame :
					className == "hkaAnimatedReferenceFrame" ? CachedAnimation::VtableFixupKind::kAnimatedReferenceFrame :
					CachedAnimation::VtableFixupKind::kGameAnimation
				});

				const auto vtableKind = a_entry.vtableFixups.back().kind;
				const auto vtable = ResolveVtable(vtableKind, gameVtable);
				if (vtable != 0) {
					*reinterpret_cast<uintptr_t*>(sectionData + src) = vtable;
				}
				if (vtableKind == CachedAnimation::VtableFixupKind::kGameAnimation) {
					vtableFixCount++;
				} else {
					referenceFrameFixCount++;
				}
			}
		}
	}
	if (verbose) {
		logger::info("[OAR-Cache] Recorded {} animation and {} reference-frame virtual fixups (animation vtable {}, reference-frame vtables resolved)",
			vtableFixCount, referenceFrameFixCount, gameVtable != 0 ? "applied" : "deferred");
	}

	// Locate the file's hkaAnimationBinding to capture the DONOR'S OWN
	// track->bone map. The binding holds a fixed-up pointer to the animation
	// at +0x18 (hkReferencedObject 0x10 + originalSkeletonName 0x08), with
	// transformTrackToBoneIndices as an hkArray<int16> at +0x20/+0x28. Scan
	// the payload for the animation pointer and validate the surrounding
	// struct. Required for Leaf Matching: the donor gets sampled under OTHER
	// weapons' clips whose bindings describe a different track layout.
	auto captureBinding = [&](uintptr_t a_animAddr) {
		for (size_t off = 0x18; off + 8 <= sectionSize; off += 8) {
			if (*reinterpret_cast<uintptr_t*>(sectionData + off) != a_animAddr) continue;
			uint8_t* base = sectionData + (off - 0x18);
			const auto arrPtr = *reinterpret_cast<uintptr_t*>(base + 0x20);
			const auto arrSize = *reinterpret_cast<int32_t*>(base + 0x28);
			if (arrSize == 0) {
				// Empty index array = Havok identity mapping (track i -> bone i).
				a_entry.bindingIdentity = true;
				return true;
			}
			if (arrSize < 0 || arrSize > 4096) continue;
			// The map covers exactly the animation's transform tracks.
			if (arrSize != a_entry.numTransformTracks) continue;
			const auto secBase = reinterpret_cast<uintptr_t>(sectionData);
			if (arrPtr < secBase || arrPtr + arrSize * sizeof(int16_t) > secBase + sectionSize) continue;
			const auto* vals = reinterpret_cast<const int16_t*>(arrPtr);
			bool sane = true;
			for (int32_t i = 0; i < arrSize; ++i) {
				if (vals[i] < -1 || vals[i] >= 4096) { sane = false; break; }
			}
			if (!sane) continue;
			a_entry.trackToBoneIndices.assign(vals, vals + arrSize);
			if (verbose) {
				OAR_VLOG("[OAR-Cache]   Binding found at 0x{:X}: {} track->bone indices",
					off - 0x18, arrSize);
			}
			return true;
		}
		return false;
	};

	// Now find the animation object - should be at contentsSectionOffset within the payload
	uint32_t rootOffset = header->contentsSectionOffset;
	if (rootOffset < sectionSize) {
		auto* candidate = reinterpret_cast<RE::hkaAnimation*>(sectionData + rootOffset);
		if (candidate->type >= 1 && candidate->type <= 10 &&
			candidate->duration > 0.001f && candidate->duration < 600.f &&
			candidate->numberOfTransformTracks >= 1 && candidate->numberOfTransformTracks <= 500) {
			a_entry.animation = candidate;
			a_entry.duration = candidate->duration;
			a_entry.numTransformTracks = candidate->numberOfTransformTracks;
			a_entry.numFloatTracks = candidate->numberOfFloatTracks;
			captureBinding(reinterpret_cast<uintptr_t>(candidate));

			auto* bytes = reinterpret_cast<uint8_t*>(candidate);
			if (verbose) {
				OAR_VLOG("[OAR-Cache]   Animation at root offset 0x{:X}: type={}, dur={:.3f}, tracks={}",
					rootOffset, candidate->type, candidate->duration, candidate->numberOfTransformTracks);
			}

			// Compute missing m_transformOffsets if the HKX didn't serialize them
			ComputeSplineOffsets(bytes, a_entry);

			if (verbose) {
				OAR_VLOG("[OAR-Cache]   Post-fixup ptrs: +0x58={:X} +0x68={:X} +0x78={:X} +0x88={:X} +0x98={:X}",
					*reinterpret_cast<uintptr_t*>(bytes + 0x58),
					*reinterpret_cast<uintptr_t*>(bytes + 0x68),
					*reinterpret_cast<uintptr_t*>(bytes + 0x78),
					*reinterpret_cast<uintptr_t*>(bytes + 0x88),
					*reinterpret_cast<uintptr_t*>(bytes + 0x98));
			}
			return true;
		}
	}

	// Fallback: scan payload for animation-like objects (after fixups applied, vtable should match)
	if (gameVtable != 0) {
		auto* anim = FindAnimationInBuffer(sectionData, sectionSize, gameVtable);
		if (anim) {
			a_entry.animation = anim;
			a_entry.duration = anim->duration;
			a_entry.numTransformTracks = anim->numberOfTransformTracks;
			a_entry.numFloatTracks = anim->numberOfFloatTracks;
			captureBinding(reinterpret_cast<uintptr_t>(anim));
			ComputeSplineOffsets(reinterpret_cast<uint8_t*>(anim), a_entry);
			return true;
		}
	}

	// Last resort: heuristic scan without vtable match
	for (size_t off = 0; off + 0x20 <= sectionSize; off += 8) {
		auto type = *reinterpret_cast<int32_t*>(sectionData + off + 0x10);
		auto dur = *reinterpret_cast<float*>(sectionData + off + 0x14);
		auto tracks = *reinterpret_cast<int32_t*>(sectionData + off + 0x18);
		auto floats = *reinterpret_cast<int32_t*>(sectionData + off + 0x1C);

		if (type >= 1 && type <= 10 && dur > 0.001f && dur < 600.f &&
			tracks >= 1 && tracks <= 500 && floats >= 0 && floats <= 200) {
			if (gameVtable != 0) {
				*reinterpret_cast<uintptr_t*>(sectionData + off) = gameVtable;
			}
			auto* candidate = reinterpret_cast<RE::hkaAnimation*>(sectionData + off);
			a_entry.animation = candidate;
			a_entry.duration = dur;
			a_entry.numTransformTracks = tracks;
			a_entry.numFloatTracks = floats;

			auto* bytes = sectionData + off;
			if (verbose) {
				OAR_VLOG("[OAR-Cache]   Found animation (heuristic) at 0x{:X}: type={}, dur={:.3f}, tracks={}",
					off, type, dur, tracks);
			}
			captureBinding(reinterpret_cast<uintptr_t>(bytes));

			// Compute missing m_transformOffsets if the HKX didn't serialize them
			ComputeSplineOffsets(bytes, a_entry);

			if (verbose) {
				OAR_VLOG("[OAR-Cache]   Post-fixup ptrs: +0x58={:X} +0x68={:X} +0x78={:X} +0x88={:X} +0x98={:X}",
					*reinterpret_cast<uintptr_t*>(bytes + 0x58),
					*reinterpret_cast<uintptr_t*>(bytes + 0x68),
					*reinterpret_cast<uintptr_t*>(bytes + 0x78),
					*reinterpret_cast<uintptr_t*>(bytes + 0x88),
					*reinterpret_cast<uintptr_t*>(bytes + 0x98));
			}
			return true;
		}
	}

	logger::warn("[OAR-Cache] Could not find hkaAnimation in packfile");
	return false;
}

bool AnimationCache::ParseTagfile(CachedAnimation& a_entry)
{
	logger::warn("[OAR-Cache] Tagfile format not yet supported - try converting to packfile (.hkx)");
	return false;
}

// Compute m_transformOffsets (and m_floatOffsets) by walking the Havok spline compressed data format.
// HKX files from many tools don't serialize these arrays, but the game's runtime requires them.
// Format reference: PyNifly wiki, Havok 2013 SDK headers.
void AnimationCache::ComputeSplineOffsets(uint8_t* a_animBytes, CachedAnimation& a_entry)
{
	// hkaSplineCompressedAnimation layout (FO4 64-bit):
	// +0x18: numTransformTracks (int32)
	// +0x1C: numFloatTracks (int32)
	// +0x38: numFrames (int32)
	// +0x3C: numBlocks (int32)
	// +0x40: maxFramesPerBlock (int32)
	// +0x44: maskAndQuantizationSize (int32)
	// +0x58: m_blockOffsets (hkArray<uint32>: ptr, size, cap)
	// +0x68: m_floatBlockOffsets (hkArray<uint32>)
	// +0x78: m_transformOffsets (hkArray<uint32>) ← what we compute
	// +0x88: m_floatOffsets (hkArray<uint32>)
	// +0x98: m_data (hkArray<uint8>)

	auto existingXform = *reinterpret_cast<uintptr_t*>(a_animBytes + 0x78);
	if (existingXform != 0) return;

	int32_t numTracks      = *reinterpret_cast<int32_t*>(a_animBytes + 0x18);
	int32_t numFloatTracks  = *reinterpret_cast<int32_t*>(a_animBytes + 0x1C);
	int32_t numFrames      = *reinterpret_cast<int32_t*>(a_animBytes + 0x38);
	int32_t numBlocks      = *reinterpret_cast<int32_t*>(a_animBytes + 0x3C);
	int32_t maskAndQuantSz = *reinterpret_cast<int32_t*>(a_animBytes + 0x44);
	uint8_t* dataPtr       = *reinterpret_cast<uint8_t**>(a_animBytes + 0x98);
	uint32_t* blockOffsets = *reinterpret_cast<uint32_t**>(a_animBytes + 0x58);

	if (!dataPtr || !blockOffsets || numBlocks <= 0 || numTracks <= 0 || numFrames <= 0) {
		logger::warn("[OAR-Cache] ComputeSplineOffsets: invalid params (data={:X} blkOff={:X} blk={} trk={} fr={})",
			(uintptr_t)dataPtr, (uintptr_t)blockOffsets, numBlocks, numTracks, numFrames);
		return;
	}

	// Havok rotation quantization: bytes per packed quaternion and alignment
	// SDK types 0-5: POLAR32, THREEAXISROT40, THREECOMP48, THREECOMP24, STRAIGHT16, UNCOMPRESSED
	static constexpr int kBytesPerQuat[6] = { 4, 5, 6, 3, 2, 16 };
	static constexpr int kQuatAlign[6]    = { 4, 1, 2, 1, 2, 4 };

	auto alignUp = [](size_t v, size_t a) -> size_t {
		if (a <= 1) return v;
		size_t r = v % a;
		return r ? v + (a - r) : v;
	};

	size_t numEntries = static_cast<size_t>(numBlocks) * numTracks;
	auto offsets = std::make_unique<uint32_t[]>(numEntries);

	int32_t dataSizeField = *reinterpret_cast<int32_t*>(a_animBytes + 0xA0);
	size_t dataSize = (dataSizeField > 0) ? static_cast<size_t>(dataSizeField) : 0x100000;

	const bool verbose = VerboseCacheLog();
	if (verbose) {
		OAR_VLOG("[OAR-Cache] ComputeSplineOffsets: maskAndQuantSz={}, dataSize={}, numBlocks={}, numTracks={}",
			maskAndQuantSz, dataSize, numBlocks, numTracks);
	}

	// Per-component sub-track type helper using HavokLib's TransformMask layout
	// For position/scale mask byte: bit i = static for axis i, bit (i+4) = spline for axis i (i=0..2)
	// For rotation mask byte: upper nibble non-zero = spline, lower nibble non-zero = static, both zero = identity
	enum SubTrackType { STT_DYNAMIC, STT_STATIC, STT_IDENTITY };

	auto getVecSubType = [](uint8_t mask, int axis) -> SubTrackType {
		bool isStatic = (mask >> axis) & 1;
		bool isSpline = (mask >> (axis + 4)) & 1;
		if (isStatic) return STT_STATIC;
		if (isSpline) return STT_DYNAMIC;
		return STT_IDENTITY;
	};

	auto getRotSubType = [](uint8_t rotMask) -> SubTrackType {
		if (rotMask & 0xF0) return STT_DYNAMIC;
		if (rotMask & 0x0F) return STT_STATIC;
		return STT_IDENTITY;
	};

	// Walk scalar channel (position or scale): shared header for all splined axes,
	// per-component BBOX/static, interleaved control points
	auto walkVecChannel = [&](size_t& cursor, uint8_t* trackBase, size_t maxCursor,
		uint8_t mask, int bpc, int32_t block, int32_t track, const char* tag, bool& ok) {

		SubTrackType xType = getVecSubType(mask, 0);
		SubTrackType yType = getVecSubType(mask, 1);
		SubTrackType zType = getVecSubType(mask, 2);
		bool useSpline = (xType == STT_DYNAMIC || yType == STT_DYNAMIC || zType == STT_DYNAMIC);

		if (useSpline) {
			if (cursor + 3 > maxCursor) { ok = false; return; }
			uint16_t numItems = *reinterpret_cast<uint16_t*>(trackBase + cursor);
			cursor += 2;
			uint8_t degree = *(trackBase + cursor);
			cursor += 1;

			if (numItems > 10000 || degree > 10) {
				logger::error("[OAR-Cache] Bad {} spline at block {} track {}: nI={} deg={}",
					tag, block, track, numItems, degree);
				ok = false;
				return;
			}

			int numKnots = numItems + degree + 2;
			cursor += numKnots;
			cursor = alignUp(cursor, 4);

			// Per-component BBOX (dynamic) or static float or nothing (identity)
			auto addComponent = [&](SubTrackType t) {
				if (t == STT_DYNAMIC) cursor += 8;       // min(f32) + max(f32)
				else if (t == STT_STATIC) cursor += 4;   // float32
			};
			addComponent(xType);
			addComponent(yType);
			addComponent(zType);

			// Interleaved control points: per CP, one quantized value per DYNAMIC axis
			int nDynamic = (xType == STT_DYNAMIC ? 1 : 0) + (yType == STT_DYNAMIC ? 1 : 0) + (zType == STT_DYNAMIC ? 1 : 0);
			int numCP = numItems + 1;
			cursor += numCP * nDynamic * bpc;
			cursor = alignUp(cursor, 4);
		} else {
			// All static/identity: just per-component float32 values
			if (xType == STT_STATIC) cursor += 4;
			if (yType == STT_STATIC) cursor += 4;
			if (zType == STT_STATIC) cursor += 4;
		}
	};

	bool success = true;
	for (int32_t block = 0; block < numBlocks && success; block++) {
		uint32_t blockStart = blockOffsets[block];
		uint8_t* blockData = dataPtr + blockStart;
		uint8_t* maskHdr = blockData;
		uint8_t* trackBase = blockData + maskAndQuantSz;
		size_t maxCursor = (dataSize > blockStart + maskAndQuantSz)
			? dataSize - blockStart - maskAndQuantSz : 0;
		size_t cursor = 0;

		for (int32_t track = 0; track < numTracks && success; track++) {
			cursor = alignUp(cursor, 4);

			offsets[block * numTracks + track] = static_cast<uint32_t>(cursor / 4);

			if (cursor > maxCursor) {
				logger::error("[OAR-Cache] Cursor overflow at block {} track {}: cursor={} max={}",
					block, track, cursor, maxCursor);
				success = false;
				break;
			}

			uint8_t quantByte = maskHdr[track * 4 + 0];
			uint8_t posMask   = maskHdr[track * 4 + 1];
			uint8_t rotMask   = maskHdr[track * 4 + 2];
			uint8_t scaleMask = maskHdr[track * 4 + 3];

			int posQuant   = (quantByte >> 0) & 0x03;
			int rotQuant   = (quantByte >> 2) & 0x0F;
			int scaleQuant = (quantByte >> 6) & 0x03;

			if (rotQuant >= 6) {
				logger::error("[OAR-Cache] Invalid rotation quantization {} at block {} track {}", rotQuant, block, track);
				success = false;
				break;
			}

			int posBPC   = (posQuant == 0) ? 1 : 2;
			int scaleBPC = (scaleQuant == 0) ? 1 : 2;
			int rotBPQ   = kBytesPerQuat[rotQuant];
			int rotAL    = kQuatAlign[rotQuant];

			// === POSITION ===
			walkVecChannel(cursor, trackBase, maxCursor, posMask, posBPC, block, track, "pos", success);
			if (!success) break;

			// === ROTATION (whole quaternion, not per-component) ===
			SubTrackType rotType = getRotSubType(rotMask);
			if (rotType == STT_DYNAMIC) {
				if (cursor + 3 > maxCursor) { success = false; break; }
				uint16_t numItems = *reinterpret_cast<uint16_t*>(trackBase + cursor);
				cursor += 2;
				uint8_t degree = *(trackBase + cursor);
				cursor += 1;

				if (numItems > 10000 || degree > 10) {
					logger::error("[OAR-Cache] Bad rot spline at block {} track {}: nI={} deg={}",
						block, track, numItems, degree);
					success = false;
					break;
				}

				int numKnots = numItems + degree + 2;
				cursor += numKnots;
				cursor = alignUp(cursor, rotAL);

				int numCP = numItems + 1;
				cursor += numCP * rotBPQ;
			} else if (rotType == STT_STATIC) {
				cursor = alignUp(cursor, rotAL);
				cursor += rotBPQ;
			}
			cursor = alignUp(cursor, 4);

			// === SCALE ===
			walkVecChannel(cursor, trackBase, maxCursor, scaleMask, scaleBPC, block, track, "scale", success);
			if (!success) break;
		}

		if (success && verbose) {
			OAR_VLOG("[OAR-Cache] Block {} walk complete: final cursor={} (max={})", block, cursor, maxCursor);
		}
	}

	if (!success) {
		logger::error("[OAR-Cache] ComputeSplineOffsets: failed while walking spline data");
		return;
	}

	// Write the computed array into the animation object's hkArray<uint32> at +0x78
	a_entry.computedTransformOffsets = std::move(offsets);
	*reinterpret_cast<uint32_t**>(a_animBytes + 0x78) = a_entry.computedTransformOffsets.get();
	*reinterpret_cast<int32_t*>(a_animBytes + 0x80) = static_cast<int32_t>(numEntries);
	*reinterpret_cast<uint32_t*>(a_animBytes + 0x84) = static_cast<uint32_t>(numEntries) | 0x80000000u;

	if (verbose) {
		OAR_VLOG("[OAR-Cache] Computed m_transformOffsets: {} entries ({} blocks x {} tracks)",
			numEntries, numBlocks, numTracks);
	}
}

RE::hkaAnimation* AnimationCache::FindAnimationInBuffer(uint8_t* a_data, size_t a_size, uintptr_t a_vtable)
{
	if (a_vtable == 0) return nullptr;

	for (size_t off = 0; off + 0x20 <= a_size; off += 8) {
		auto vtbl = *reinterpret_cast<uintptr_t*>(a_data + off);
		if (vtbl == a_vtable) {
			auto* candidate = reinterpret_cast<RE::hkaAnimation*>(a_data + off);
			if (candidate->type >= 1 && candidate->type <= 10 &&
				candidate->duration > 0.001f && candidate->duration < 600.f &&
				candidate->numberOfTransformTracks >= 1 && candidate->numberOfTransformTracks <= 500) {
				return candidate;
			}
		}
	}

	return nullptr;
}
