#pragma once

namespace RE
{
	class __declspec(novtable) alignas(0x10) NiBillboardNode :
		public NiNode  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::NiBillboardNode };
		static constexpr auto VTABLE{ VTABLE::NiBillboardNode };
		static constexpr auto Ni_RTTI{ Ni_RTTI::NiBillboardNode };

		virtual ~NiBillboardNode();

		enum class FaceMode
		{
			kAlwaysFaceCamera = 0x0,
			kRotateAboutUp = 0x1,
			kRigidFaceCamera = 0x3,
			kAlwaysFaceCenter = 0x4,
			kBSRotateAboutUp = 0x5
		};

		// members
		NiTFlags<std::uint32_t, NiBillboardNode> flags;  // 140
	};
	static_assert(sizeof(NiBillboardNode) == 0x150);
}
