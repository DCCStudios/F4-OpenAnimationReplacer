#include "AnimationCache.h"
#include "OpenAnimationReplacer.h"

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

bool AnimationCache::LoadAnimation(const std::string& a_suffix, const std::filesystem::path& a_absolutePath)
{
	if (!std::filesystem::exists(a_absolutePath)) {
		logger::warn("[OAR-Cache] File not found: '{}'", a_absolutePath.string());
		return false;
	}

	auto entry = std::make_unique<CachedAnimation>();
	entry->filePath = a_absolutePath.string();

	std::ifstream file(a_absolutePath, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		logger::warn("[OAR-Cache] Cannot open: '{}'", a_absolutePath.string());
		return false;
	}

	auto fileSize = file.tellg();
	if (fileSize < 64 || fileSize > 50 * 1024 * 1024) {
		logger::warn("[OAR-Cache] Invalid file size ({}) for: '{}'", (int64_t)fileSize, a_absolutePath.string());
		return false;
	}

	entry->fileData.resize(static_cast<size_t>(fileSize));
	file.seekg(0);
	file.read(reinterpret_cast<char*>(entry->fileData.data()), fileSize);
	file.close();

	auto magic = *reinterpret_cast<uint32_t*>(entry->fileData.data());

	bool parsed = false;
	if (magic == kPackfileMagic0) {
		parsed = ParsePackfile(*entry);
	} else if (magic == kTagfileMagic) {
		parsed = ParseTagfile(*entry);
	} else {
		logger::warn("[OAR-Cache] Unknown file format (magic=0x{:08X}) for: '{}'", magic, a_absolutePath.string());
		return false;
	}

	if (!parsed || !entry->animation) {
		logger::warn("[OAR-Cache] Failed to parse animation from: '{}'", a_absolutePath.string());
		return false;
	}

	logger::info("[OAR-Cache] Loaded '{}': duration={:.3f}s, tracks={}, floats={}", 
		a_suffix, entry->duration, entry->numTransformTracks, entry->numFloatTracks);

	OpenAnimationReplacer::GetSingleton()->loadingLoadedAnims.fetch_add(1);

	std::unique_lock lock(m_mutex);
	m_cache[a_suffix] = std::move(entry);
	return true;
}

void AnimationCache::SetVtableFromGame(uintptr_t a_vtable)
{
	uintptr_t prev = m_gameAnimVtable.exchange(a_vtable);
	if (prev != 0) return;

	std::shared_lock lock(m_mutex);
	int patched = 0;
	for (auto& [key, entry] : m_cache) {
		if (!entry || entry->fileData.empty()) continue;
		uint8_t* sectionData = entry->fileData.data() + entry->sectionFileOffset;
		for (uint32_t off : entry->vtableFixupOffsets) {
			*reinterpret_cast<uintptr_t*>(sectionData + off) = a_vtable;
			patched++;
		}
	}
	if (patched > 0) {
		logger::info("[OAR-Cache] Retroactively patched {} vtable slots across {} cached animations",
			patched, m_cache.size());
	}
}

RE::hkaAnimation* AnimationCache::GetCachedAnimation(const std::string& a_suffix) const
{
	std::shared_lock lock(m_mutex);
	auto it = m_cache.find(a_suffix);
	if (it != m_cache.end() && it->second) {
		return it->second->animation;
	}
	return nullptr;
}

RE::hkaAnimation* AnimationCache::GetOrBuildRuntimeAnim(const std::string& a_suffix, RE::hkaAnimation* a_gameAnim)
{
	std::unique_lock lock(m_mutex);
	auto it = m_cache.find(a_suffix);
	if (it == m_cache.end() || !it->second) return nullptr;

	auto& entry = *it->second;

	// If clone exists but was built from a DIFFERENT game animation, the game reloaded
	// animations (weapon switch). Rebuild from fresh data to avoid stale struct fields.
	if (entry.runtimeAnimation && entry.gameOriginal != a_gameAnim && a_gameAnim != nullptr) {
		logger::info("[OAR-Cache] Game animation changed for '{}': old={:X} new={:X} — rebuilding clone",
			a_suffix, reinterpret_cast<uintptr_t>(entry.gameOriginal),
			reinterpret_cast<uintptr_t>(a_gameAnim));
		entry.runtimeAnimation = nullptr;
		entry.runtimeStruct.clear();
		entry.gameOriginal = nullptr;
	}

	if (entry.runtimeAnimation) return entry.runtimeAnimation;

	if (!a_gameAnim || !entry.animation) return nullptr;

	// Clone the game's animation struct (guaranteed correct layout) then patch data pointers.
	// Use 0x100 bytes to cover the full hkaSplineCompressedAnimation struct with margin.
	static constexpr size_t kStructSize = 0x100;
	entry.runtimeStruct.resize(kStructSize + 16, 0);

	// Ensure 16-byte alignment for SIMD
	uintptr_t rawAddr = reinterpret_cast<uintptr_t>(entry.runtimeStruct.data());
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
				logger::info("[OAR-Cache] Parsed {} annotations from replacement '{}'",
					entry.annotations.size(), entry.filePath);
				for (auto& pa : entry.annotations) {
					logger::info("[OAR-Cache]   t={:.4f}s  '{}'", pa.time, pa.text);
				}
			}
		}
	}

	// Patch ALL stale pointers on the clone. The clone was memcpy'd from the game's
	// original animation, so any pointer fields reference game memory that gets freed
	// on weapon switch. We must eliminate all stale references.

	// 1. m_extractedMotion at +0x20: points to game's hkaAnimatedReferenceFrame.
	//    NULL it out — weapon animations don't use root motion extraction, and our packfile
	//    data has unfixed local pointers that can't be used directly.
	*reinterpret_cast<uintptr_t*>(cloneBase + 0x20) = 0;

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

	logger::info("[OAR-Annot] Clone '{}': patched extractedMotion+annotationTracks, {} parsed replacement annotations (using trigger NULLing + manual firing)",
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

	entry.runtimeAnimation = reinterpret_cast<RE::hkaAnimation*>(cloneBase);
	entry.gameOriginal = a_gameAnim;

	logger::info("[OAR-Cache] Built runtime clone for '{}': base={:X} gameStruct={:X}",
		a_suffix, reinterpret_cast<uintptr_t>(cloneBase), reinterpret_cast<uintptr_t>(a_gameAnim));

	return entry.runtimeAnimation;
}

const std::vector<AnimationCache::ParsedAnnotation>* AnimationCache::GetAnnotations(const std::string& a_suffix) const
{
	std::shared_lock lock(m_mutex);
	auto it = m_cache.find(a_suffix);
	if (it != m_cache.end() && it->second && !it->second->annotations.empty()) {
		return &it->second->annotations;
	}
	return nullptr;
}

size_t AnimationCache::GetCacheSize() const
{
	std::shared_lock lock(m_mutex);
	return m_cache.size();
}

bool AnimationCache::IsOurReplacement(RE::hkaAnimation* a_anim) const
{
	if (!a_anim) return false;
	std::shared_lock lock(m_mutex);
	for (auto& [key, entry] : m_cache) {
		if (entry && entry->runtimeAnimation == a_anim) return true;
	}
	return false;
}

RE::hkaAnimation* AnimationCache::GetOriginalFromReplacement(RE::hkaAnimation* a_replacement) const
{
	if (!a_replacement) return nullptr;
	std::shared_lock lock(m_mutex);
	for (auto& [key, entry] : m_cache) {
		if (entry && entry->runtimeAnimation == a_replacement && entry->gameOriginal) {
			return entry->gameOriginal;
		}
	}
	return nullptr;
}

void AnimationCache::InvalidateRuntimeClones()
{
	std::unique_lock lock(m_mutex);
	int count = 0;
	for (auto& [key, entry] : m_cache) {
		if (entry && entry->runtimeAnimation) {
			entry->runtimeAnimation = nullptr;
			entry->runtimeStruct.clear();
			entry->gameOriginal = nullptr;
			count++;
		}
	}
	if (count > 0) {
		logger::info("[OAR-Cache] Invalidated {} runtime clones (gameOriginal reset)", count);
	}
}

void AnimationCache::Clear()
{
	std::unique_lock lock(m_mutex);
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

	logger::info("[OAR-Cache] Packfile: ver=0x{:X}, ptrSize={}, numSec={}, contIdx={}, contOff={}, v11={}",
		header->fileVersion, header->pointerSize,
		header->numSections, header->contentsSectionIndex, header->contentsSectionOffset, isV11);

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
		logger::info("[OAR-Cache]   Section[{}] '{}': absStart=0x{:X}, localFix=0x{:X}, globalFix=0x{:X}, virtFix=0x{:X}, end=0x{:X}",
			i, sections[i].tag, sections[i].absoluteDataStart,
			sections[i].localFixupsOffset, sections[i].globalFixupsOffset,
			sections[i].virtualFixupsOffset, sections[i].endOffset);
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

	logger::info("[OAR-Cache] Data section payload: fileOffset=0x{:X}, size={} bytes", sectionFileOffset, sectionSize);

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
	logger::info("[OAR-Cache] Applied {} local fixups", localFixCount);

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
	logger::info("[OAR-Cache] Applied {} global fixups", globalFixCount);

	// === Apply virtual fixups (vtable patching) ===
	// Virtual fixups are 12-byte records (src_u32, section_u32, nameOffset_u32)
	// Each says: object at sectionData+src needs its vtable set
	// Store all vtable offsets so we can retroactively patch when vtable is captured
	uintptr_t gameVtable = m_gameAnimVtable.load();
	int vtableFixCount = 0;

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
				if (src == 0xFFFFFFFF) continue;
				if (src + 8 > sectionSize) continue;

				a_entry.vtableFixupOffsets.push_back(src);
				if (gameVtable != 0) {
					*reinterpret_cast<uintptr_t*>(sectionData + src) = gameVtable;
				}
				vtableFixCount++;
			}
		}
	}
	logger::info("[OAR-Cache] Recorded {} virtual fixup offsets (vtable {})",
		vtableFixCount, gameVtable != 0 ? "applied" : "deferred");

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

			auto* bytes = reinterpret_cast<uint8_t*>(candidate);
			logger::info("[OAR-Cache]   Animation at root offset 0x{:X}: type={}, dur={:.3f}, tracks={}",
				rootOffset, candidate->type, candidate->duration, candidate->numberOfTransformTracks);

			// Compute missing m_transformOffsets if the HKX didn't serialize them
			ComputeSplineOffsets(bytes, a_entry);

			logger::info("[OAR-Cache]   Post-fixup ptrs: +0x58={:X} +0x68={:X} +0x78={:X} +0x88={:X} +0x98={:X}",
				*reinterpret_cast<uintptr_t*>(bytes + 0x58),
				*reinterpret_cast<uintptr_t*>(bytes + 0x68),
				*reinterpret_cast<uintptr_t*>(bytes + 0x78),
				*reinterpret_cast<uintptr_t*>(bytes + 0x88),
				*reinterpret_cast<uintptr_t*>(bytes + 0x98));
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
			logger::info("[OAR-Cache]   Found animation (heuristic) at 0x{:X}: type={}, dur={:.3f}, tracks={}",
				off, type, dur, tracks);

			// Compute missing m_transformOffsets if the HKX didn't serialize them
			ComputeSplineOffsets(bytes, a_entry);

			logger::info("[OAR-Cache]   Post-fixup ptrs: +0x58={:X} +0x68={:X} +0x78={:X} +0x88={:X} +0x98={:X}",
				*reinterpret_cast<uintptr_t*>(bytes + 0x58),
				*reinterpret_cast<uintptr_t*>(bytes + 0x68),
				*reinterpret_cast<uintptr_t*>(bytes + 0x78),
				*reinterpret_cast<uintptr_t*>(bytes + 0x88),
				*reinterpret_cast<uintptr_t*>(bytes + 0x98));
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

	logger::info("[OAR-Cache] ComputeSplineOffsets: maskAndQuantSz={}, dataSize={}, numBlocks={}, numTracks={}",
		maskAndQuantSz, dataSize, numBlocks, numTracks);

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

		if (success) {
			logger::info("[OAR-Cache] Block {} walk complete: final cursor={} (max={})", block, cursor, maxCursor);
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

	logger::info("[OAR-Cache] Computed m_transformOffsets: {} entries ({} blocks x {} tracks)",
		numEntries, numBlocks, numTracks);
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
