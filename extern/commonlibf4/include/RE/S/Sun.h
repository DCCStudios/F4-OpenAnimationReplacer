#pragma once

namespace RE
{
	class __declspec(novtable) Sun :
		public SkyObject
	{
	public:
		static constexpr auto RTTI{ RTTI::Sun };
		static constexpr auto VTABLE{ VTABLE::Sun };

		virtual ~Sun();

		// members
		NiPointer<NiBillboardNode>     sunBaseNode;       // 10
		NiPointer<NiBillboardNode>     sunGlareNode;      // 18
		NiPointer<BSTriShape>          sunBase;           // 20
		NiPointer<BSTriShape>          sunQuery;          // 28
		NiPointer<BSTriShape>          sunGlare;          // 30
		NiPointer<NiDirectionalLight>  light;             // 38
		NiPointer<NiDirectionalLight>  cloudLight;        // 40
		float                          glareScale;        // 48
		bool                           doOcclusionTests;  // 4C
		NiPointer<BSShaderAccumulator> sunAccumulator;    // 50
	};
	static_assert(sizeof(Sun) == 0x58);
}
