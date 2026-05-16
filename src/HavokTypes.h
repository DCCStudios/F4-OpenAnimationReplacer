#pragma once

namespace RE
{
	struct hkbGenerateNodesJob;
	struct hkbNodePartitionInfo;
	struct hkbBehaviorGraph;
	struct hkbSpatialQueryInterface;
	struct hkbCharacter;
	struct hkbCharacterSetup;
	struct hkbEventQueue;
	struct hkbGeneratorOutput;
	struct hkbEventPayload;
	struct hkbRagdollInterface;
	struct hkbCharacterData;
	struct hkbSymbolIdMap;
	struct hkbStateMachine;
	struct hkbRagdollDriver;
	struct hkbAssetLoader;
	struct hkbNodeInfo;
	struct hkbStateListener;
	struct hkaFootPlacementIkSolver;
	struct hkbContext;
	struct hkbWorld;
	struct hkbPhysicsInterface;
	struct hkbFootIkDriver;
	struct hkbTransitionEffect;
	struct hkbNodeInternalStateInfo;
	struct hkRefVariant;
	struct hkbVariableValueSet;
	struct hkbAnimationBindingWithTriggers;
	struct hkbAnimationBindingSet;
	struct hkbHandIkDriver;
	struct hkbDockingDriver;
	struct hkPseudoRandomGenerator;
	struct hkbBindable;
	struct hkaAnimationBinding;
	struct hkbClipTrigger;
	struct hkbEventInfo;
	struct hkbClipTriggerArray;
	struct hkaDefaultAnimationControl;
	struct hkbBehaviorGraphStringData;
	struct hkbVariableBindingSet;
	struct hkbGeneratorSyncInfo;
	struct hkbReferencedGeneratorSyncInfo;
	struct hkbSharedEventQueue;
	struct hkbSceneModifier;
	struct hkbAttachmentManager;
	struct hkbAttachmentInstance;
	struct hkaDefaultAnimationControlMapperData;
	struct hkbProjectData;
	struct hkaMirroredSkeleton;
	struct hkaSkeleton;
	struct hkLocalFrame;
	struct hkbCharacterControllerDriver;
	struct hkbProjectAssetManager;
	struct hkaAnimation;
	struct hkbBehaviorGraphData;
	struct hkbCondition;
	struct hkbFootIkDriverInfo;
	struct hkbMirroredSkeletonInfo;
	struct hkbRagdollController;
	struct hkaBone;
	struct hkbCharacterController;
	struct hkbDockingTarget;
	struct hkbAssetBundleStringData;
	struct hkaAnnotationTrack;
	struct hkbCustomIdSelector;
	struct hkbAttachmentSetup;
	struct hkbGeneratorOutputListener;
	struct hkaAnimatedReferenceFrame;
	struct hkaSkeletonMapper;
	struct hkClassEnum;
	struct hkaBoneAttachment;
	struct hkbProjectStringData;
	struct hkbHandIkDriverInfo;
	struct hkbAssetBundle;
	struct hkbCharacterStringData;

	struct hkClassMember
	{
		enum Type : uint8_t
		{
			TYPE_VOID = 0x0,
			TYPE_BOOL = 0x1,
			TYPE_CHAR = 0x2,
			TYPE_INT8 = 0x3,
			TYPE_UINT8 = 0x4,
			TYPE_INT16 = 0x5,
			TYPE_UINT16 = 0x6,
			TYPE_INT32 = 0x7,
			TYPE_UINT32 = 0x8,
			TYPE_INT64 = 0x9,
			TYPE_UINT64 = 0xA,
			TYPE_REAL = 0xB,
			TYPE_VECTOR4 = 0xC,
			TYPE_QUATERNION = 0xD,
			TYPE_MATRIX3 = 0xE,
			TYPE_ROTATION = 0xF,
			TYPE_QSTRANSFORM = 0x10,
			TYPE_MATRIX4 = 0x11,
			TYPE_TRANSFORM = 0x12,
			TYPE_ZERO = 0x13,
			TYPE_POINTER = 0x14,
			TYPE_FUNCTIONPOINTER = 0x15,
			TYPE_ARRAY = 0x16,
			TYPE_INPLACEARRAY = 0x17,
			TYPE_ENUM = 0x18,
			TYPE_STRUCT = 0x19,
			TYPE_SIMPLEARRAY = 0x1A,
			TYPE_HOMOGENEOUSARRAY = 0x1B,
			TYPE_VARIANT = 0x1C,
			TYPE_CSTRING = 0x1D,
			TYPE_ULONG = 0x1E,
			TYPE_FLAGS = 0x1F,
			TYPE_HALF = 0x20,
			TYPE_STRINGPTR = 0x21,
			TYPE_RELARRAY = 0x22,
			TYPE_MAX = 0x23
		};
	};

	struct hkStringPtr
	{
		const char* stringAndFlag;

		const char* data() const
		{
			return reinterpret_cast<const char*>(
				reinterpret_cast<uintptr_t>(stringAndFlag) & ~static_cast<uintptr_t>(1));
		}
		void set(const char* a_str)
		{
			stringAndFlag = a_str;
		}
		operator const char*() const { return data(); }
	};

	struct hkbVariableBindingSet : public hkReferencedObject
	{
		struct Binding
		{
			enum BindingType : int8_t
			{
				BINDING_TYPE_VARIABLE = 0x0,
				BINDING_TYPE_CHARACTER_PROPERTY = 0x1
			};

			enum InternalBindingFlags : int8_t {};

			hkStringPtr memberPath;
			const hkClass* memberClass;
			int32_t offsetInObjectPlusOne;
			int32_t offsetInArrayPlusOne;
			int32_t rootVariableIndex;
			int32_t variableIndex;
			int8_t bitIndex;
			hkEnum<BindingType, int8_t> bindingType;
			hkEnum<hkClassMember::Type, uint8_t> memberType;
			int8_t variableType;
			hkFlags<InternalBindingFlags, int8_t> flags;
		};

		hkArray<Binding, hkContainerHeapAllocator> bindings;
		int32_t indexOfBindingToEnable;
		hkBool hasOutputBinding;
		hkBool initializedOffsets;
	};

	struct hkbBindable : public hkReferencedObject
	{
		hkRefPtr<hkbVariableBindingSet> variableBindingSet;
		hkArray<hkbBindable*, hkContainerHeapAllocator> cachedBindables;
		hkBool areBindablesCached;
		hkBool hasEnableChanged;
	};

	enum hkbNodeType : uint8_t
	{
		HKB_NODE_TYPE_INVALID = 0x0,
		HKB_NODE_TYPE_FIRST_GENERATOR = 0x1,
		HKB_NODE_TYPE_BEHAVIOR_GRAPH = 0x1,
		HKB_NODE_TYPE_BEHAVIOR_REFERENCE_GENERATOR = 0x2,
		HKB_NODE_TYPE_BLENDER_GENERATOR = 0x3,
		HKB_NODE_TYPE_CLIP_GENERATOR = 0x4,
		HKB_NODE_TYPE_MANUAL_SELECTOR_GENERATOR = 0x5,
		HKB_NODE_TYPE_MODIFIER_GENERATOR = 0x6,
		HKB_NODE_TYPE_REFERENCE_POSE_GENERATOR = 0x7,
		HKB_NODE_TYPE_STATE_MACHINE = 0x8,
		HKB_NODE_TYPE_SCRIPT_GENERATOR = 0x9,
		HKB_NODE_TYPE_LAYER_GENERATOR = 0xA,
		HKB_NODE_TYPE_DOCKING_GENERATOR = 0xB,
		HKB_NODE_TYPE_PARAMETRIC_MOTION_GENERATOR = 0xC,
		HKB_NODE_TYPE_PIN_BONE_GENERATOR = 0xD,
		HKB_NODE_TYPE_OTHER_GENERATOR = 0xE,
	};

	struct hkbNode : public hkbBindable
	{
		enum CloneState : int8_t
		{
			CLONE_STATE_DEFAULT = 0x0,
			CLONE_STATE_TEMPLATE = 0x1,
			CLONE_STATE_CLONE = 0x2
		};

		uint32_t userData;
		hkStringPtr name;
		uint16_t id;
		hkEnum<CloneState, int8_t> cloneState;
		hkEnum<hkbNodeType, uint8_t> type;
		hkbNodeInfo* nodeInfo;
	};

	struct hkbGeneratorPartitionInfo
	{
		uint32_t boneMask[8];
		uint32_t partitionMask[1];
		int16_t numBones;
		int16_t numMaxPartitions;
	};

	struct hkbGeneratorSyncInfo
	{
		uint8_t syncPoints[0x80];
		float duration;
		float localTime;
		float playbackSpeed;
		int8_t numSyncPoints;
		bool isCyclic;
		bool isMirrored;
		bool isAdditive;
		uint8_t activeInterval[0x14];
	};
	static_assert(sizeof(hkbGeneratorSyncInfo) == 0xA4);

	struct hkbGenerator : public hkbNode
	{
		hkbGeneratorPartitionInfo partitionInfo;
		hkbGeneratorSyncInfo* syncInfo;
		int8_t pad[4];
	};

	struct hkbEventBase
	{
		int32_t id;
		hkbEventPayload* payload;
	};

	struct hkbEventProperty : public hkbEventBase {};

	struct hkbClipTrigger
	{
		float localTime;
		hkbEventProperty event;
		hkBool relativeToEndOfClip;
		hkBool acyclic;
		hkBool isAnnotation;
	};

	struct hkbClipTriggerArray : public hkReferencedObject
	{
		hkArray<hkbClipTrigger, hkContainerHeapAllocator> triggers;
	};

	enum hkbClipGenerator_PlaybackMode : int8_t
	{
		MODE_SINGLE_PLAY = 0,
		MODE_LOOPING = 1,
		MODE_USER_CONTROLLED = 2,
		MODE_PING_PONG = 3,
		MODE_COUNT = 4,
	};

	struct hkaAnnotationTrack
	{
		struct Annotation
		{
			float time;          // +0x00
			uint32_t pad04;      // +0x04
			hkStringPtr text;    // +0x08
		};
		static_assert(sizeof(Annotation) == 0x10);

		hkStringPtr trackName;
		hkArray<Annotation, hkContainerHeapAllocator> annotations;
	};

	// Forward decl for SamplePartialTracks parameter (hkQsTransform layout = hkQsTransformRaw)
	struct hkQsTransformRaw;

	struct hkaAnimation : public hkReferencedObject
	{
		int32_t type;                    // +0x10 AnimationType (1..5)
		float duration;                  // +0x14
		int32_t numberOfTransformTracks; // +0x18
		int32_t numberOfFloatTracks;     // +0x1C
		// +0x20: extractedMotion (hkRefPtr)
		// +0x28: annotationTracks (hkArray<hkaAnnotationTrack>)

		// Vtable [4] in F4 (Havok 2014, confirmed via SKSE/CommonLibSSE NG): SamplePartialTracks
		// Samples this animation at a_time and fills a_outTracks (track-indexed) with the
		// per-track hkQsTransform. a_outFloats is filled with float track values.
		void SamplePartialTracks(float a_time,
			uint32_t a_maxNumTransformTracks,
			hkQsTransformRaw* a_outTracks,
			uint32_t a_maxNumFloatTracks,
			float* a_outFloats,
			void* a_chunkCache /* hkaChunkCache* */) const
		{
			using SampleFn = void(*)(const hkaAnimation*, float, uint32_t,
				hkQsTransformRaw*, uint32_t, float*, void*);
			auto* vtbl = *reinterpret_cast<void* const* const*>(this);
			auto fn = reinterpret_cast<SampleFn>(vtbl[4]);
			fn(this, a_time, a_maxNumTransformTracks, a_outTracks,
				a_maxNumFloatTracks, a_outFloats, a_chunkCache);
		}
	};
	static_assert(offsetof(hkaAnimation, type) == 0x10);
	static_assert(offsetof(hkaAnimation, duration) == 0x14);

	struct hkbClipGenerator : public hkbGenerator
	{
		hkStringPtr animationBundleName;                   // from NAF
		hkStringPtr animationName;                         // from NAF
		hkRefPtr<hkbClipTriggerArray> triggers;            // from NAF
		uint32_t userPartitionMask;                        // from NAF
		float cropStartAmountLocalTime;                    // from NAF
		float cropEndAmountLocalTime;                      // from NAF
		float startTime;                                   // from NAF
		float playbackSpeed;                               // from NAF
		float enforcedDuration;                            // from NAF
		float userControlledTimeFraction;                   // from NAF
		int16_t animationBindingIndex;                     // +0xBC runtime: index into binding set
		hkbClipGenerator_PlaybackMode mode;
		int8_t flags;

		// Post-activation fields accessed via raw offsets (verified by HaBCR static_asserts)
		// animationControl at +0xD0, binding at +0xE8, localTime at +0x140

		void* GetAnimationControlRaw() const
		{
			auto* bytes = reinterpret_cast<const uint8_t*>(this);
			return *reinterpret_cast<void* const*>(bytes + 0xD0);
		}

		void* GetBindingRaw() const
		{
			auto* bytes = reinterpret_cast<const uint8_t*>(this);
			return *reinterpret_cast<void* const*>(bytes + 0xE8);
		}

		hkaAnimation* GetAnimation() const
		{
			auto ctrl = reinterpret_cast<uintptr_t>(GetAnimationControlRaw());
			if (!ctrl) return nullptr;
			auto bind = *reinterpret_cast<uintptr_t*>(ctrl + 0x38);
			if (!bind) return nullptr;
			return *reinterpret_cast<hkaAnimation**>(bind + 0x18);
		}

		hkaAnimation** GetAnimationSlot() const
		{
			auto ctrl = reinterpret_cast<uintptr_t>(GetAnimationControlRaw());
			if (!ctrl) return nullptr;
			auto bind = *reinterpret_cast<uintptr_t*>(ctrl + 0x38);
			if (!bind) return nullptr;
			return reinterpret_cast<hkaAnimation**>(bind + 0x18);
		}

		float GetLocalTime() const
		{
			return *reinterpret_cast<const float*>(
				reinterpret_cast<const uint8_t*>(this) + 0x140);
		}
	};

	struct hkbProjectStringData : public hkReferencedObject
	{
		hkArray<hkStringPtr, hkContainerHeapAllocator> animationFilenames;   // +0x10
		hkArray<hkStringPtr, hkContainerHeapAllocator> behaviorFilenames;    // +0x20
		hkArray<hkStringPtr, hkContainerHeapAllocator> characterFilenames;   // +0x30
		hkArray<hkStringPtr, hkContainerHeapAllocator> eventNames;           // +0x40
		hkStringPtr animationPath;    // +0x50
		hkStringPtr behaviorPath;     // +0x58
		hkStringPtr characterPath;    // +0x60
		hkStringPtr scriptsPath;      // +0x68
		hkStringPtr fullPathToSource; // +0x70
		hkStringPtr rootPath;         // +0x78
	};

	struct hkbProjectData : public hkReferencedObject
	{
		uint8_t worldUpWS[16];                         // +0x10 (hkVector4f)
		hkRefPtr<hkbProjectStringData> stringData;     // +0x20
	};

	struct __declspec(novtable) hkbCharacter : public hkReferencedObject
	{
		virtual void getNearbyCharacters(float, hkArray<hkbCharacter*, hkContainerHeapAllocator>*) {}

		void* unk10;
		hkArray<hkbCharacter*, hkContainerHeapAllocator> nearbyCharacters;
		uint32_t userData;
		int16_t currentLod;
		int16_t numTracksInLod;
		hkbGeneratorOutput* generatorOutput;
		hkStringPtr name;
		hkRefPtr<hkbRagdollDriver> ragdollDriver;
		hkRefPtr<hkbRagdollInterface> ragdollInterface;
		hkRefPtr<hkbCharacterControllerDriver> characterControllerDriver;
		hkRefPtr<hkbFootIkDriver> footIkDriver;
		hkRefPtr<hkbHandIkDriver> handIkDriver;
		hkRefPtr<hkbDockingDriver> dockingDriver;
		hkRefPtr<hkReferencedObject> aiControlDriver;
		hkRefPtr<hkbCharacterSetup> setup;
		hkRefPtr<hkbBehaviorGraph> behaviorGraph;
		hkRefPtr<hkbProjectData> projectData;
		hkRefPtr<hkbAnimationBindingSet> animationBindingSet;
		hkbSpatialQueryInterface* spatialQueryInterface;
		hkbWorld* world;
		hkArray<hkaBoneAttachment*, hkContainerHeapAllocator> boneAttachments;
		hkbEventQueue* m_eventQueue;
		void* characterLuaState;
		hkbProjectAssetManager* assetManager;
		int32_t capabilities;
		int32_t effectiveCapabilities;
	};
	static_assert(offsetof(hkbCharacter, behaviorGraph) == 0x80);

	struct hkPointerMap
	{
		void* elements;
		int32_t numElements;
		int32_t hashMod;
	};

	struct hkbBehaviorGraph : public hkbGenerator
	{
		enum VariableMode : int8_t {};

		struct GlobalTransitionData;

		hkEnum<VariableMode, int8_t> variableMode;
		hkArray<uint16_t, hkContainerHeapAllocator> uniqueIdPool;
		hkPointerMap* idToStateMachineTemplateMap;
		hkArray<int32_t, hkContainerHeapAllocator> mirroredExternalIdMap;
		hkPseudoRandomGenerator* pseudoRandomGenerator;
		hkRefPtr<hkbGenerator> rootGenerator;
		hkRefPtr<hkbBehaviorGraphData> data;
		hkRefPtr<hkbBehaviorGraph> _template;
		hkbGenerator* rootGeneratorClone;
		hkArray<hkbNodeInfo*, hkContainerHeapAllocator>* activeNodes;
		hkRefPtr<hkbBehaviorGraph::GlobalTransitionData> globalTransitionData;
		hkRefPtr<hkbSymbolIdMap> eventIdMap;
		hkRefPtr<hkbSymbolIdMap> attributeIdMap;
		hkRefPtr<hkbSymbolIdMap> variableIdMap;
		hkRefPtr<hkbSymbolIdMap> characterPropertyIdMap;
		hkbVariableValueSet* variableValueSet;
		hkPointerMap* nodeTemplateToCloneMap;
		hkPointerMap* stateListenerTemplateToCloneMap;
		hkArray<hkbNode*, hkContainerHeapAllocator> recentlyCreatedClones;
		hkbNodePartitionInfo* nodePartitionInfo;
		int32_t numIntermediateOutputs;
		hkArray<int16_t, hkContainerHeapAllocator> intermediateOutputSizes;
		hkArray<hkbGenerateNodesJob*, hkContainerHeapAllocator> jobs;
		hkArray<void*, hkContainerHeapAllocator> allPartitionMemory;
		hkArray<int32_t, hkContainerHeapAllocator> internalToRootVariableIdMap;
		hkArray<int32_t, hkContainerHeapAllocator> internalToCharacterCharacterPropertyIdMap;
		hkArray<int32_t, hkContainerHeapAllocator> internalToRootAttributeIdMap;
		uint16_t nextUniqueId;
		hkBool isActive;
		hkBool isLinked;
		hkBool updateActiveNodes;
		hkBool updateActiveNodesForEnable;
		hkBool checkNodeValidity;
		hkBool stateOrTransitionChanged;

		void setActiveGeneratorLocalTime(const hkbContext* a_context, hkbGenerator* a_gen, float a_time)
		{
			using func_t = decltype(&hkbBehaviorGraph::setActiveGeneratorLocalTime);
			REL::Relocation<func_t> func{ REL::ID(992878) };
			return func(this, a_context, a_gen, a_time);
		}

		hkbNode* getNodeClone(hkbNode* a_node)
		{
			using func_t = decltype(&hkbBehaviorGraph::getNodeClone);
			REL::Relocation<func_t> func{ REL::ID(326555) };
			return func(this, a_node);
		}
	};
	static_assert(offsetof(hkbBehaviorGraph, rootGenerator) == 0xC0);

	struct hkbCharacterStringData : public hkReferencedObject
	{
		struct FileNameMeshNamePair
		{
			hkStringPtr fileName;
			hkStringPtr meshName;
		};

		// Havok 2013 SDK (verified from projectanarchy source):
		//   skinNames, boneAttachmentNames, animationBundleNameData, ...
		// Our field names follow CommonLibF4/NAF convention. Bethesda may have
		// split SDK "skinNames" into deformable+rigid and added "animationNames".
		// The runtime probe in main.cpp verifies the actual layout.
		hkArray<FileNameMeshNamePair, hkContainerHeapAllocator> deformableSkinNames;
		hkArray<FileNameMeshNamePair, hkContainerHeapAllocator> rigidSkinNames;
		hkArray<FileNameMeshNamePair, hkContainerHeapAllocator> animationNames;
		hkArray<FileNameMeshNamePair, hkContainerHeapAllocator> animationBundleNameData;
		hkArray<FileNameMeshNamePair, hkContainerHeapAllocator> animationBundleFilenameData;
		hkArray<hkStringPtr, hkContainerHeapAllocator> characterPropertyNames;
		hkArray<FileNameMeshNamePair, hkContainerHeapAllocator> retargetingSkeletonMapperFilenames;
		hkArray<hkStringPtr, hkContainerHeapAllocator> lodNames;
		hkArray<hkStringPtr, hkContainerHeapAllocator> mirroredSyncPointSubstringsA;
		hkArray<hkStringPtr, hkContainerHeapAllocator> mirroredSyncPointSubstringsB;
		hkStringPtr name;
		hkStringPtr rigName;
		hkStringPtr ragdollName;
		hkStringPtr behaviorFilename;
	};

	class AnimVariableCacheInfo;
	class BSAnimationGraphChannel;
	class BShkbAnimationGraph;
	class hkbVariableValue;

	struct BSAnimationGraphVariableCache
	{
		BSTArray<AnimVariableCacheInfo> variableCache;
		BSTArray<hkbVariableValue*> variableQuickLookup;
		BSSpinLock* lock;
		BSTSmartPointer<BShkbAnimationGraph> graphToCacheFor;
	};
	static_assert(sizeof(BSAnimationGraphVariableCache) == 0x40);

	struct BSAnimationGraphEvent
	{
		TESObjectREFR* refr;
		BSFixedString animEvent;
		BSFixedString argument;
	};

	namespace BGSAnimationSystemUtils
	{
		inline bool GetEventSourcePointersFromGraph(
			const TESObjectREFR* a_refr,
			BSScrapArray<BSTEventSource<BSAnimationGraphEvent>*>& a_sourcesOut)
		{
			using func_t = decltype(&GetEventSourcePointersFromGraph);
			REL::Relocation<func_t> func{ REL::ID(897074) };
			return func(a_refr, a_sourcesOut);
		}
	}

	class BSAnimationGraphManager :
		public BSTEventSink<BSAnimationGraphEvent>,
		public BSIntrusiveRefCounted
	{
	public:
		struct DependentManagerSmartPtr
		{
			std::uint64_t ptrAndFlagsStorage;
		};
		static_assert(sizeof(DependentManagerSmartPtr) == 0x08);

		BSTArray<BSTSmartPointer<BSAnimationGraphChannel>> boundChannel;
		BSTArray<BSTSmartPointer<BSAnimationGraphChannel>> bumpedChannel;
		BSTSmallArray<BSTSmartPointer<BShkbAnimationGraph>, 1> graph;
		BSTArray<DependentManagerSmartPtr> subManagers;
		BSTArray<BSTTuple<BSFixedString, BSFixedString>> eventQueuea;
		BSAnimationGraphVariableCache variableCache;
		BSSpinLock updateLock;
		BSSpinLock dependentManagerLock;
		std::uint32_t activeGraph;
		std::uint32_t generateDepth;
	};
	static_assert(sizeof(BSAnimationGraphManager) == 0xE0);

	enum BSVisitControl : uint32_t
	{
		kContinue = 0,
		kStop = 1
	};

	class BShkbAnimationGraph
	{
	public:
		uint8_t pad00[0x1C8];
		hkbCharacter character;

		void VisitGraph(class BShkbVisitor& a_visitor)
		{
			using func_t = decltype(&BShkbAnimationGraph::VisitGraph);
			REL::Relocation<func_t> func{ REL::ID(194777) };
			return func(this, a_visitor);
		}
	};
	static_assert(offsetof(BShkbAnimationGraph, character) == 0x1C8);

	struct hkbCharacterData : public hkReferencedObject
	{
		uint8_t unk10[0xA0];  // probe: stringData found at data+0xB0 (0x10 base + 0xA0 pad)
		hkRefPtr<hkbCharacterStringData> stringData;  // +0xB0
	};

	struct hkbCharacterSetup : public hkReferencedObject
	{
		hkArray<void*, hkContainerHeapAllocator> retargetingSkeletonMappers;  // +0x10
		hkRefPtr<hkReferencedObject> animationSkeleton;                      // +0x20
		hkRefPtr<hkReferencedObject> ragdollToAnimationSkeletonMapper;       // +0x28
		hkRefPtr<hkReferencedObject> animationToRagdollSkeletonMapper;       // +0x30
		uint8_t pad38[8];                                                     // +0x38 (probe: data at +0x40)
		hkRefPtr<hkbCharacterData> data;                                     // +0x40
		hkRefPtr<hkReferencedObject> unscaledAnimationSkeleton;
		hkRefPtr<hkbMirroredSkeletonInfo> mirroredSkeletonInfo;
	};

	struct hkbAnimationBindingWithTriggers
	{
		float unk00[10];
	};

	struct hkbContext
	{
		hkbContext(hkbCharacter* a_char, void* a_physIntfc = nullptr, void* a_attchMngr = nullptr)
		{
			Ctor(a_char, a_physIntfc, a_attchMngr);
		}

		~hkbContext() { Dtor(); }

		hkbCharacter* character;
		uint8_t unk08[96];

	private:
		void Ctor(hkbCharacter* a_char, void* a_physIntfc = nullptr, void* a_attchMngr = nullptr)
		{
			using func_t = decltype(&hkbContext::Ctor);
			REL::Relocation<func_t> func{ REL::ID(1381136) };
			return func(this, a_char, a_physIntfc, a_attchMngr);
		}

		void Dtor()
		{
			using func_t = decltype(&hkbContext::Dtor);
			REL::Relocation<func_t> func{ REL::ID(144578) };
			return func(this);
		}
	};

	// ========================================================================
	// hkbGeneratorOutput layout (confirmed from Full Body Awareness F4SE RE_Shims.h)
	// Used by partial body animation layering to read/write per-bone transforms.
	// ========================================================================

	struct TrackMasterHeaderRaw {
		int32_t numBytes;
		int32_t numTracks;
		uint8_t unused[8];
	};
	static_assert(sizeof(TrackMasterHeaderRaw) == 16);

	struct TrackHeaderRaw {
		int16_t capacity;
		int16_t numData;           // number of bones (for pose track)
		int16_t dataOffset;        // byte offset from Tracks* start to data
		int16_t elementSizeBytes;
		float   onFraction;
		int8_t  flags;
		int8_t  type;
		int8_t  pad[2];
	};
	static_assert(sizeof(TrackHeaderRaw) == 16);

	// Track indices within hkbGeneratorOutput::Tracks
	static constexpr int kTrackIndex_Pose = 2;

	struct hkQsTransformRaw {
		float translation[4];  // xyz + w(usually 0)
		float rotation[4];     // quaternion xyzw
		float scale[4];        // xyz + w(usually 0)
	};
	static_assert(sizeof(hkQsTransformRaw) == 48);

	// hkaSkeleton layout (for bone resolution in partial body layering)
	// Access path: hkbCharacter.setup -> hkbCharacterSetup.animationSkeleton (offset +0x20)
	// hkaSkeleton:
	//   +0x18  hkArray<int16_t> parentIndices
	//   +0x28  hkArray<hkaBone>  bones  (each hkaBone: hkStringPtr name(8) + lockTranslation(1) + pad(7) = 16 bytes)
	struct hkArrayRawLayout {
		void* data;
		int32_t size;
		int32_t capacityAndFlags;
	};
	static_assert(sizeof(hkArrayRawLayout) == 16);

	static constexpr uintptr_t kSkeletonOffset_parentIndices = 0x18;
	static constexpr uintptr_t kSkeletonOffset_bones = 0x28;
	static constexpr size_t kHkaBoneStride = 16;  // hkStringPtr(8) + lockTranslation(1) + pad(7)

	// hkaAnimationBinding layout (verified from CommonLibSSE NG / Havok 2014):
	//   +0x00 vtable, +0x08 refCount/memSize
	//   +0x10 hkRefPtr<hkaSkeletonInfo> originalSkeletonName (deprecated, sometimes absent)
	//   +0x18 hkRefPtr<hkaAnimation> animation
	//   +0x20 hkArray<int16_t>     transformTrackToBoneIndices  ← what we need
	//   +0x30 hkArray<int16_t>     floatTrackToFloatSlotIndices
	//   +0x40 hkArray<int16_t>     partitionIndices
	//   +0x?? blendHint (BlendHint enum, byte)
	//   total size 0x48
	static constexpr uintptr_t kBindingOffset_transformTrackToBoneIndices = 0x20;

	namespace GraphUtils
	{
		struct GraphLock
		{
			GraphLock(GraphLock&) = delete;

			GraphLock(BSSpinLock& lock)
			{
				_lock = std::addressof(lock);
				_lock->lock();
			}

			GraphLock(GraphLock&& other) noexcept
			{
				_lock = other._lock;
				other._lock = nullptr;
				character = other.character;
				graph = other.graph;
				rootGen = other.rootGen;
			}

			GraphLock(std::nullptr_t) {}

			~GraphLock()
			{
				if (_lock != nullptr)
					_lock->unlock();
			}

			operator bool() const { return _lock != nullptr; }

			hkbCharacter* character = nullptr;
			hkbBehaviorGraph* graph = nullptr;
			hkbGenerator* rootGen = nullptr;

		protected:
			BSSpinLock* _lock = nullptr;
		};

		inline GraphLock AcquireGraphLock(IAnimationGraphManagerHolder* graphHolder)
		{
			if (!graphHolder)
				return nullptr;

			BSTSmartPointer<BSAnimationGraphManager> manager;
			if (!graphHolder->GetAnimationGraphManagerImpl(manager) || !manager)
				return nullptr;

			GraphLock result(manager->updateLock);
			if (manager->graph.size() < 1 || manager->graph.size() <= manager->activeGraph)
				return nullptr;

			auto& character = manager->graph[manager->activeGraph]->character;
			result.character = &character;
			result.graph = character.behaviorGraph._ptr;

			if (!result.graph)
				return nullptr;

			result.rootGen = result.graph->rootGenerator._ptr;
			if (!result.rootGen)
				return nullptr;

			return result;
		}
	}
}
