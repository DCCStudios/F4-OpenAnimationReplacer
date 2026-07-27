#pragma once

#include "RE/I/IRendererResourceManager.h"

namespace RE
{
	class __declspec(novtable) BSShaderResourceManager :
		public IRendererResourceManager
	{
	public:
		static constexpr auto RTTI{ RTTI::BSShaderResourceManager };
		static constexpr auto VTABLE{ VTABLE::BSShaderResourceManager };

		virtual ~BSShaderResourceManager();

		virtual BSTriShape*         CreateTriShape(std::uint32_t a_numTriangles, std::uint16_t* a_indices, std::uint32_t a_numVertices, NiPoint3* a_positions, NiPoint2* a_texCoords0, NiColorA* a_texCoords1, NiPoint3* a_normals, NiPoint3* a_binormals, NiPoint3* a_tangents, NiColorA* a_colors, NiColorA* a_skinBoneWeights, std::uint8_t* a_skinBoneIndices, NiColorA* a_landscapeData1, NiColorA* a_landscapeData2, float* a_eyeData, bool a_generatePositionData) override;
		virtual BSTriShape*         CreateTriShape(std::uint32_t a_numTriangles, std::uint32_t a_numVertices, NiPoint3* a_positions, NiPoint2* a_texCoords, NiColorA* a_colors, std::uint16_t* a_indices) override;
		virtual void*               CreateTriShape(NiStream* a_stream, std::uint64_t a_vertexDesc, std::uint32_t a_vertexCount, std::uint32_t a_indexCount, char** a_dynamicData) override;
		virtual void*               CreateTriShapeRendererData(void* a_vertexBuffer, std::uint64_t a_vertexDesc, std::uint16_t* a_indices, std::uint32_t a_indexCount) override;
		virtual BSSubIndexTriShape* CreateSubIndexTriShape(std::uint32_t a_numTriangles, std::uint32_t a_numSegments, std::uint16_t* a_indices, std::uint32_t a_numVertices, NiPoint3* a_positions, NiPoint2* a_texCoords0, NiColorA* a_texCoords1, NiPoint3* a_normals, NiPoint3* a_binormals, NiPoint3* a_tangents, NiColorA* a_colors, NiColorA* a_skinBoneWeights, std::uint8_t* a_skinBoneIndices, NiColorA* a_landscapeData1, NiColorA* a_landscapeData2, float* a_eyeData) override;
		virtual void                IncRefTriShape(void* a_rendererData) override;
		virtual void                DecRefTriShape(void* a_rendererData) override;
		virtual BSDynamicTriShape*  CreateDynamicTriShape(std::uint32_t a_dynamicFlags, std::uint32_t a_numTriangles, std::uint16_t* a_indices, std::uint32_t a_numVertices, NiPoint3* a_positions, NiPoint2* a_texCoords0, NiColorA* a_texCoords1, NiPoint3* a_normals, NiPoint3* a_binormals, NiPoint3* a_tangents, NiColorA* a_colors, NiColorA* a_skinBoneWeights, std::uint8_t* a_skinBoneIndices, NiColorA* a_landscapeData1, NiColorA* a_landscapeData2, float* a_eyeData) override;
		virtual BSDynamicTriShape*  CreateDynamicTriShape(std::uint32_t, std::uint32_t, std::uint32_t, NiPoint3*, NiPoint2*, NiColorA*, std::uint16_t*) override;
		virtual void                IncRefDynamicTriShape(void* a_rendererData) override;
		virtual void                DecRefDynamicTriShape(void* a_rendererData) override;
		virtual void                ConvertBSTriShapeToBSDynamicTriShape(NiNode* a_root, NiDefaultAVObjectPalette* a_objectPalette) override;
		virtual void                ApplyMaterials(NiAVObject* a_root) override;
		virtual void                SetTriShapeStreamDynamicFlags(std::uint32_t) override;
		virtual void*               CreateParticleShape() override;
		virtual void                IncRefParticleShape(void* a_rendererData) override;
		virtual void                DecRefParticleShape(void* a_rendererData) override;
		virtual BSLines*            CreateLineShape(std::uint32_t a_numLines, std::uint16_t* a_indices, std::uint32_t a_numVertices, NiPoint3* a_positions, NiPoint2* a_texCoords0, NiColorA* a_texCoords1, NiPoint3* a_normals, NiPoint3* a_binormals, NiPoint3* a_tangents, NiColorA* a_colors, NiColorA* a_skinBoneWeights, std::uint8_t* a_skinBoneIndices, NiColorA* a_landscapeData1, NiColorA* a_landscapeData2, float* a_eyeData) override;
		virtual BSDynamicLines*     CreateDynamicLineShape(std::uint32_t, std::uint32_t, std::uint32_t, NiPoint3*, NiPoint2*, NiColorA*, std::uint16_t*) override;
		virtual void*               CreateDynamicLineShape(NiStream*, std::uint64_t, std::uint32_t, std::uint32_t) override;
		virtual BSDynamicLines*     CreateDynamicLineShape(std::uint32_t, std::uint32_t, std::uint16_t*, std::uint32_t, NiPoint3*, NiPoint2*, NiColorA*, NiPoint3*, NiPoint3*, NiPoint3*, NiColorA*, NiColorA*, std::uint8_t*, NiColorA*, NiColorA*, float*) override;
		virtual void                IncRefLines(void* a_rendererData) override;
		virtual void                DecRefLines(void* a_rendererData) override;
		virtual void                IncRefDynamicLines(void* a_rendererData) override;
		virtual void                DecRefDynamicLines(void* a_rendererData) override;
		virtual void                LoadTexture(NiTexture* apTexture) override;
		virtual void                CreateStreamingTexture(NiTexture* a_texture, const void* a_userDataIn, void* a_userDataOut) override;
		virtual bool                ReadStreamingTextureData(NiTexture* a_texture, void* a_userDataIn) override;
		virtual bool                CreateStreamingTextureArraySlice(NiTexture* a_texture, const void* a_userDataIn, void* a_userDataOut, std::uint32_t a_slice) override;
		virtual bool                ReadStreamingTextureDataToArraySlice(NiTexture* a_texture, void* a_userDataIn, std::uint32_t a_slice) override;
		virtual void                FinishStreamingTexture(NiTexture* a_texture) override;
		virtual void                IncRefTexture(BSGraphics::Texture* a_rendererData) override;
		virtual void                DecRefTexture(BSGraphics::Texture* a_rendererData) override;
		virtual void                GetTextureWidthHeight(NiTexture* a_texture, std::uint32_t* a_width, std::uint32_t* a_height) override;
		virtual std::uint32_t       GetTextureFormat(NiTexture* a_texture) override;
		virtual bool                UpdateTextureToDesiredMipLevel(NiTexture* a_texture, std::uint32_t*) override;
		virtual void                LoadUpgradeTextureData(NiTexture* a_texture) override;
		virtual bool                UpdateStreamingTextureToDesiredMipLevel(NiTexture* a_texture, std::uint32_t*, void*, void*) override;
		virtual void                FinishStreamingTextureUpgade(NiTexture* a_texture) override;
		virtual void*               CreateVertexBuffer(std::uint32_t* a_dataSize, void* a_data, std::uint32_t a_stride, std::uint64_t) override;
		virtual void                DecRefVertexBuffer(void* a_rendererData) override;
		virtual NiShadeProperty*    CreateDefaultEffectShaderProperty(bool a_vertexColors, bool a_skinned) override;
		virtual BSMeshLODTriShape*  CreateMeshLODTriShape(std::uint32_t*, std::uint32_t, std::uint64_t, std::uint32_t, std::uint32_t, void*) override;
		virtual BSMeshLODTriShape*  CreateMeshLODTriShape(BSTriShape*, std::uint32_t*, std::uint32_t) override;
		virtual std::uint32_t       UpdateIndexBufferForInstancing(BSMultiStreamInstanceTriShape*, std::uint32_t) override;
		virtual bool                FindIntersectionsTriShapeFastPath(const NiPoint3*, const NiPoint3*, NiPick*, BSTriShape*) override;
		virtual void                CreateTangentSpace(std::uint32_t, std::uint16_t*, std::uint32_t, NiPoint3*, NiPoint2*, NiPoint3*, NiPoint3*, NiPoint3*) override;
		virtual float               GetShaderFrameCount() override;
		virtual float               GetShaderTimerDelta() override;
		virtual void                GetFadeNodeSettings(FadeNodeSettings* a_skinned) override;
		virtual void                GetCameraVectors(NiPoint3* a_up, NiPoint3* a_right, NiPoint3* a_direction) override;

		static inline BSShaderResourceManager* GetSingleton()
		{
			static REL::Relocation<BSShaderResourceManager**> ptr{ ID::BSShaderResourceManager::Singleton };
			return *ptr;
		}
	};
}
