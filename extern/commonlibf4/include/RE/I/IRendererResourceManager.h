#pragma once

namespace RE
{
	class BSDynamicLines;
	class BSDynamicTriShape;
	class BSLines;
	class BSMeshLODTriShape;
	class BSMultiStreamInstanceTriShape;
	class BSSubIndexTriShape;
	class BSTriShape;
	class FadeNodeSettings;
	class NiAVObject;
	class NiColorA;
	class NiDefaultAVObjectPalette;
	class NiNode;
	class NiPick;
	class NiPoint2;
	class NiPoint3;
	class NiShadeProperty;
	class NiStream;
	class NiTexture;

	namespace BSGraphics
	{
		class Texture;
	}

	class __declspec(novtable) IRendererResourceManager
	{
	public:
		static constexpr auto RTTI{ RTTI::IRendererResourceManager };
		static constexpr auto VTABLE{ VTABLE::IRendererResourceManager };

		virtual ~IRendererResourceManager();

		virtual BSTriShape*         CreateTriShape(std::uint32_t a_numTriangles, std::uint16_t* a_indices, std::uint32_t a_numVertices, NiPoint3* a_positions, NiPoint2* a_texCoords0, NiColorA* a_texCoords1, NiPoint3* a_normals, NiPoint3* a_binormals, NiPoint3* a_tangents, NiColorA* a_colors, NiColorA* a_skinBoneWeights, std::uint8_t* a_skinBoneIndices, NiColorA* a_landscapeData1, NiColorA* a_landscapeData2, float* a_eyeData, bool a_generatePositionData) = 0;
		virtual BSTriShape*         CreateTriShape(std::uint32_t a_numTriangles, std::uint32_t a_numVertices, NiPoint3* a_positions, NiPoint2* a_texCoords, NiColorA* a_colors, std::uint16_t* a_indices) = 0;
		virtual void*               CreateTriShape(NiStream* a_stream, std::uint64_t a_vertexDesc, std::uint32_t a_vertexCount, std::uint32_t a_indexCount, char** a_dynamicData) = 0;
		virtual void*               CreateTriShapeRendererData(void* a_vertexBuffer, std::uint64_t a_vertexDesc, std::uint16_t* a_indices, std::uint32_t a_indexCount) = 0;
		virtual BSSubIndexTriShape* CreateSubIndexTriShape(std::uint32_t a_numTriangles, std::uint32_t a_numSegments, std::uint16_t* a_indices, std::uint32_t a_numVertices, NiPoint3* a_positions, NiPoint2* a_texCoords0, NiColorA* a_texCoords1, NiPoint3* a_normals, NiPoint3* a_binormals, NiPoint3* a_tangents, NiColorA* a_colors, NiColorA* a_skinBoneWeights, std::uint8_t* a_skinBoneIndices, NiColorA* a_landscapeData1, NiColorA* a_landscapeData2, float* a_eyeData) = 0;
		virtual void                IncRefTriShape(void* a_rendererData) = 0;
		virtual void                DecRefTriShape(void* a_rendererData) = 0;
		virtual BSDynamicTriShape*  CreateDynamicTriShape(std::uint32_t a_dynamicFlags, std::uint32_t a_numTriangles, std::uint16_t* a_indices, std::uint32_t a_numVertices, NiPoint3* a_positions, NiPoint2* a_texCoords0, NiColorA* a_texCoords1, NiPoint3* a_normals, NiPoint3* a_binormals, NiPoint3* a_tangents, NiColorA* a_colors, NiColorA* a_skinBoneWeights, std::uint8_t* a_skinBoneIndices, NiColorA* a_landscapeData1, NiColorA* a_landscapeData2, float* a_eyeData) = 0;
		virtual BSDynamicTriShape*  CreateDynamicTriShape(std::uint32_t, std::uint32_t, std::uint32_t, NiPoint3*, NiPoint2*, NiColorA*, std::uint16_t*) = 0;
		virtual void                IncRefDynamicTriShape(void* a_rendererData) = 0;
		virtual void                DecRefDynamicTriShape(void* a_rendererData) = 0;
		virtual void                ConvertBSTriShapeToBSDynamicTriShape(NiNode* a_root, NiDefaultAVObjectPalette* a_objectPalette) = 0;
		virtual void                ApplyMaterials(NiAVObject* a_root) = 0;
		virtual void                SetTriShapeStreamDynamicFlags(std::uint32_t) = 0;
		virtual void*               CreateParticleShape() = 0;
		virtual void                IncRefParticleShape(void* a_rendererData) = 0;
		virtual void                DecRefParticleShape(void* a_rendererData) = 0;
		virtual BSLines*            CreateLineShape(std::uint32_t, std::uint16_t*, std::uint32_t, NiPoint3*, NiPoint2*, NiColorA*, NiPoint3*, NiPoint3*, NiPoint3*, NiColorA*, NiColorA*, std::uint8_t*, NiColorA*, NiColorA*, float*) = 0;
		virtual BSDynamicLines*     CreateDynamicLineShape(std::uint32_t, std::uint32_t, std::uint32_t, NiPoint3*, NiPoint2*, NiColorA*, std::uint16_t*) = 0;
		virtual void*               CreateDynamicLineShape(NiStream*, std::uint64_t, std::uint32_t, std::uint32_t) = 0;
		virtual BSDynamicLines*     CreateDynamicLineShape(std::uint32_t, std::uint32_t, std::uint16_t*, std::uint32_t, NiPoint3*, NiPoint2*, NiColorA*, NiPoint3*, NiPoint3*, NiPoint3*, NiColorA*, NiColorA*, std::uint8_t*, NiColorA*, NiColorA*, float*) = 0;
		virtual void                IncRefLines(void* a_rendererData) = 0;
		virtual void                DecRefLines(void* a_rendererData) = 0;
		virtual void                IncRefDynamicLines(void* a_rendererData) = 0;
		virtual void                DecRefDynamicLines(void* a_rendererData) = 0;
		virtual void                LoadTexture(NiTexture* apTexture) = 0;
		virtual void                CreateStreamingTexture(NiTexture* a_texture, const void* a_userDataIn, void* a_userDataOut) = 0;
		virtual bool                ReadStreamingTextureData(NiTexture* a_texture, void* a_userDataIn) = 0;
		virtual bool                CreateStreamingTextureArraySlice(NiTexture* a_texture, const void* a_userDataIn, void* a_userDataOut, std::uint32_t a_slice) = 0;
		virtual bool                ReadStreamingTextureDataToArraySlice(NiTexture* a_texture, void* a_userDataIn, std::uint32_t a_slice) = 0;
		virtual void                FinishStreamingTexture(NiTexture* a_texture) = 0;
		virtual void                IncRefTexture(BSGraphics::Texture* a_rendererData) = 0;
		virtual void                DecRefTexture(BSGraphics::Texture* a_rendererData) = 0;
		virtual void                GetTextureWidthHeight(NiTexture* a_texture, std::uint32_t* a_width, std::uint32_t* a_height) = 0;
		virtual std::uint32_t       GetTextureFormat(NiTexture* a_texture) = 0;
		virtual bool                UpdateTextureToDesiredMipLevel(NiTexture* a_texture, std::uint32_t*) = 0;
		virtual void                LoadUpgradeTextureData(NiTexture* a_texture) = 0;
		virtual bool                UpdateStreamingTextureToDesiredMipLevel(NiTexture* a_texture, std::uint32_t*, void*, void*) = 0;
		virtual void                FinishStreamingTextureUpgade(NiTexture* a_texture) = 0;
		virtual void*               CreateVertexBuffer(std::uint32_t* a_dataSize, void* a_data, std::uint32_t a_stride, std::uint64_t) = 0;
		virtual void                DecRefVertexBuffer(void* a_rendererData) = 0;
		virtual NiShadeProperty*    CreateDefaultEffectShaderProperty(bool a_vertexColors, bool a_skinned) = 0;
		virtual BSMeshLODTriShape*  CreateMeshLODTriShape(std::uint32_t*, std::uint32_t, std::uint64_t, std::uint32_t, std::uint32_t, void*) = 0;
		virtual BSMeshLODTriShape*  CreateMeshLODTriShape(BSTriShape*, std::uint32_t*, std::uint32_t) = 0;
		virtual std::uint32_t       UpdateIndexBufferForInstancing(BSMultiStreamInstanceTriShape*, std::uint32_t) = 0;
		virtual bool                FindIntersectionsTriShapeFastPath(const NiPoint3*, const NiPoint3*, NiPick*, BSTriShape*) = 0;
		virtual void                CreateTangentSpace(std::uint32_t, std::uint16_t*, std::uint32_t, NiPoint3*, NiPoint2*, NiPoint3*, NiPoint3*, NiPoint3*) = 0;
		virtual float               GetShaderFrameCount() = 0;
		virtual float               GetShaderTimerDelta() = 0;
		virtual void                GetFadeNodeSettings(FadeNodeSettings* a_skinned) = 0;
		virtual void                GetCameraVectors(NiPoint3* a_up, NiPoint3* a_right, NiPoint3* a_direction) = 0;
	};
}
