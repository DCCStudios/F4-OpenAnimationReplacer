#pragma once

#include "REX/TEnumSet.h"

namespace REX::W32
{
	using RTTITypeDescriptor = std::type_info;

	struct RTTIClassHierarchyDescriptor;

	struct RTTIBaseClassDescriptor
	{
		enum class Attribute : std::uint32_t
		{
			None = 0,
			NotVisible = 1 << 0,
			Ambiguous = 1 << 1,
			Private = 1 << 2,
			PrivateOrProtectedBase = 1 << 3,
			Virtual = 1 << 4,
			NonPolymorphic = 1 << 5,
			HasHierarchyDescriptor = 1 << 6
		};

		struct PMD
		{
			// members
			std::int32_t mDisp;  // 0x00
			std::int32_t pDisp;  // 0x04
			std::int32_t vDisp;  // 0x08
		};
		static_assert(sizeof(PMD) == 0x0C);

		// members
		std::uint32_t       typeDescriptor;     // 0x00
		std::uint32_t       numContainedBases;  // 0x04
		PMD                 where;              // 0x08
		TEnumSet<Attribute> attributes;         // 0x14
		std::uint32_t       classDescriptor;    // 0x18
	};
	static_assert(sizeof(RTTIBaseClassDescriptor) == 0x1C);

	struct RTTIClassHierarchyDescriptor
	{
		enum class Attribute : std::uint32_t
		{
			NoInheritance = 0,
			MultipleInheritance = 1 << 0,
			VirtualInheritance = 1 << 1,
			AmbiguousInheritance = 1 << 2
		};

		// members
		std::uint32_t       signature;       // 0x00
		TEnumSet<Attribute> attributes;      // 0x04
		std::uint32_t       numBaseClasses;  // 0x08
		std::uint32_t       baseClassArray;  // 0x0C
	};
	static_assert(sizeof(RTTIClassHierarchyDescriptor) == 0x10);

	struct RTTICompleteObjectLocator
	{
		// members
		std::uint32_t signature;        // 0x00
		std::uint32_t offset;           // 0x04
		std::uint32_t cdOffset;         // 0x08
		std::uint32_t typeDescriptor;   // 0x0C
		std::uint32_t classDescriptor;  // 0x10
		std::uint32_t self;             // 0x14
	};
	static_assert(sizeof(RTTICompleteObjectLocator) == 0x18);
}
