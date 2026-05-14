So lately I've been delving into the Skyrim executable and on these ventures I found something quite interesting. It seems that during development Bethesda used a structure called hkDescriptionAttribute (from my understanding it functions similar to an attribute in .net as the name implies :D ) to tag their new Havok class members, these have been left in the executable thanks to quite an extensive reflection framework. They aren't anything special but it may clear up some ambiguous members when you're deciphering the asset files. I will keep an eye out in my ventures for more of these descriptions but until then I'll dump what I've found so far here:

 

It is a bit messy, but I'll try to come back every once and a while and tidy it up a bit.

 

 

  Hide contents
BSIStateManagerModifier
iStateVar - Variable to set when a state in the array is activated

BSIStateManagerModifierBSiStateData
pStateMachine - The state machine whose state we want to watch for being entered
StateID - The state ID of the state we want to watch for being entered
iStateToSetAs - The iState_* variable to use for setting m_iStateVar with when this state is entered

BSIStateManagerModifierBSIStateManagerStateListener
pStateManager - The state machine whose state we want to watch for being entered

BSTweenerModifier
tweenPosition - Whether or not to tween the position
tweenRotation - Whether or not to tween the rotation
useTweenDuration - Whether or not to use the specified tween duration. If false, the tween will be applied for the duration of the modifier
tweenDuration - A user specified tween duration. This value is only applicable if m_useTweenDuration is enabled
targetPosition
targetRotation - The target rotation
duration
startTransform
time

BSTimerModifier
secondsElapsed
resetAlarm - Whether or not to reset the alarm when the timer expires
alarmEvent - The event to send when the timer goes off
alarmTimeSeconds - An event is sent at this time

BSSpeedSamplerModifier
speedOut - Output variable used in the blendgenerator for the directional speed animations
goalSpeed - Variable which defines the speed the character should be moving
direction - Direction variable used to drive the direction blends of the character
state - State variable used by the behavior graph

BSRagdollContactListenerModifier
ragdollRigidBodies - The rigid bodies to watch for collision in the ragdoll
throwEvent
contactEvent - The event to send when the ragdoll collides with it's environment

BSPassByTargetTriggerModifier
targetPosition - The target position
targetPassed
movementDirection - The movement direction

radius - The radius at which the pass by can trigger
triggerEvent - Event that is raised when the target is passed by

BSModifyOnceModifier
pOnDeactivateModifier
pOnActivateModifier - The modifier to run on activation

BSLookAtModifier
lookAtTarget - Whether or not to look at target
eyeBones - The properties of each eye bone in the look chain
bones - The properties of each bone in the look chain
lookAtCameraZ
lookAtCameraY
lookAtCameraX
targetLocation - The position in the world that is being looked at
lookAtCamera - Whether or not to look at the camera instead of the target location
targetOutOfLimitEvent - The event that is raised when the target location is out of the look limit
targetOutsideLimits - Whether or not the target is outside of the limits for keeping the modifier active
useBoneGains - Whether or not to use the individual bone gains for interpolation
continueLookOutsideOfLimit - Whether or not to continue the look when the target is outside of the limit
limitAngleThresholdDegrees - LookAt range is limited by a cone having this angle, in degrees, between its center and its side
limitAngleDegrees - The range is limited by a cone having this angle, in degrees, between its center and its side
ballBonesValid
onGain - Gain used to smooth out the motion when the character starts looking at a target
offGain - Gain used to smooth out the motion when the character stops looking at a target

BSLookAtModifierBoneData
index
currentFwdAxisLS
fwdAxisLS - The forward axis of the bone, in local space of the bone
limitAngleDegrees - Angle offset, from m_limitAngleDegrees, at which the modifier should no longer be active
enabled - Whether or not the bone is enabled
onGain - Gain used to smooth out the motion when the bone starts looking at a target
offGain - Gain used to smooth out the motion when the bone stops looking at a target



BSLimbIKModifier
castOffset - The offset above the bones in the limb from which to begin the downward ray-cast from
boneRadius - The radius for the bones within the limb
currentAngle
limitAngleDegrees - Maximum pitch angle that the limb may be raised, in degrees
gain - Gain used to smooth out the motion when the limb is raised to a target angle
startBoneIndex - Index of the first bone in the limb chain
endBoneIndex - Index of the last bone in the limb chain

BSIsActiveModifier
bInvertActive4
bIsActive4 - A fifth bool to determine if this node is active

bInvertActive3
bIsActive3 - A fourth bool to determine if this node is active

bInvertActive2
bIsActive2 - A third bool to determine if this node is active

bInvertActive1
bIsActive1 - A second bool to determine if this node is active

bInvertActive0
bIsActive0 - Bool to determine if this node is active


BSInterpValueModifier
gain - The gain
result - The result value
source - The source value
target - The target value
timeStep - The current time step

BSEventOnFalseToTrueModifier
bSlot3ActivatedLastFrame
bSlot2ActivatedLastFrame
bSlot1ActivatedLastFrame
EventToSend3
bVariableToTest3
bEnableEvent3
EventToSend2
bVariableToTest2
bEnableEvent2
EventToSend1 - Event to send if we are enabled and variableToTest1 goes from false to true
bVariableToTest1 - - If active and this variable becomes true, send the event
bEnableEvent1 - Determine if we should be checking the variable below to send the event

BSEventOnDeactivateModifier
event - Event to be raised when this modifier deactivates

BSEventEveryNEventsModifier
calculatedNumberOfEventsBeforeSend
numberOfEventsSeen
randomizeNumberOfEvents - Randomize m_numberOfEventsBeforeSend on activation between m_numberOfEventsBeforeSend & m_numberOfEventsBeforeSend
minimumNumberOfEventsBeforeSend - If Randomize is selected, this allows you to set a minimum number for the randomization
numberOfEventsBeforeSend - Number of times to get m_eventToCheckFor before sending m_eventToSend
eventToSend - Event to send once m_numberOfEventsBeforeSend has been achieved
eventToCheckFor - Event to check for to total to m_numberOfEventsToSend before sending m_eventToSend

BSDistTriggerModifier
triggerEvent - Event that is raised when the the distance is within the range of the trigger distance
distance - The distance to the target position
distanceTrigger - The distance at which the event will be triggered
targetPosition - The world space position from which the distance is calculated


BSDirectAtModifier
boneChainIndices
directAtTargetLocation
hasTarget
timeStep
currentPitchOffset - The current pitch offset
currentHeadingOffset - The current heading offset
directAtCameraZ - The Z coordinate of the camera position
directAtCameraY - The Y coordinate of the camera position
directAtCameraX - The X coordinate of the camera position
directAtCamera - Whether or not to direct at the camera instead of the target location
userInfo - A field for user purposes
targetLocation - The position in the world that is being directed at
offGain - Gain used to smooth out the motion when the character stops directing at a target
onGain - Gain used to smooth out the motion when the character starts directing at a target
offsetPitchDegrees - A customizable pitch offset, in degrees
offsetHeadingDegrees - A customizable heading offset, in degrees
limitPitchDegrees - The pitch is limited by a cone having this angle, in degrees, between its center and its side
limitHeadingDegrees - The heading is limited by a cone having this angle, in degrees, between its center and its side
endBoneIndex - Index of the last bone in the chain
startBoneIndex - Index of the first bone in the chain
sourceBoneIndex - Index of the source bone for the calculations
directAtTarget - Whether or not to direct at the target
active - Flag indicating whether or not the modifier is active

BSDecomposeVectorModifier
vector - The vector being decomposed
x - The x component of the vector being decomposed
y - The y component of the vector being decomposed
z - The z component of the vector being decomposed
w - The w component of the vector being decomposed


BSComputeAddBoneAnimModifier
pSkeletonMemory
scaleLSOut
rotationLSOut
translationLSOut
boneIndex - The bone index


The rotational part of the transform to apply


BSSynchronizedClipGenerator
bAllCharactersAtMarks
bAllCharactersInScene
bAtMark
sAnimationBindingIndex
pEventMap
pLocalSyncBinding
fCurrentLerp
StartMarkMS
EndMarkWS
StartMarkWS
pSyncScene
bApplyMotionFromRoot - Whether or not the root motion in the animation is applied
bReorientSupportChar - Should the support character attempt to reorient itself as it moves to it's mark
bLeadCharacter - Is this actor allowed to move to get into place for the synchronized animation (?)
fMarkErrorThreshold - Error epsilon used to measure if the character is at it's mark
fGetToMarkTime - Seconds to move character into place before synchronized animation can play
bSyncClipIgnoreMarkPlacement - Allow actor to move to position while sync animation is playing (?)
SyncAnimPrefix - The prefix used on the skeleton for this animation
pClipGenerator - The synchronized clip containing all the characters. Please use only Clip generators


BGSGamebryoSequenceGenerator
eBlendModeFunction - Enum of which blend function you want to use with the percentage - if other than BMF_NONE
pSequence - Gamebryo Sequence name
fPercent- The percentage this clip should contribute to the blend, if blend mode not BMF_NONE Range:0.0 - 1.0


BlendModeFunction - BMF_ONE_MINUS_PERCENT BMF_PERCENT BMF_NONE

BSiStateTaggingGenerator
iPriority - The priority of the activated iState
iStateToSetAs - The iState to set when this state is activated
pDefaultGenerator - The child generator to set the istate for


BSOffsetAnimationGenerator
pDefaultGenerator - The child generator to modify
bOffsetValid
bZeroOffset
iCurrentFrame
fCurrentPercentage
BoneIndexA
BoneOffsetA
fOffsetRangeEnd - This is the end of the range that the offset variable will be compared against
fOffsetRangeStart - This is the beginning of the range that the offset variable will be compared against
fOffsetVariable - The variable that will determine where in the clip generator to test. The clip will be sampled at the same percentage through as this is between m_fOffsetRangeStart and m_fOffsetRangeEnd
pOffsetClipGenerator - The clip generator that contains the offset to apply to default generator



BSCyclicBlendTransitionGenerator
eBlendCurve - Which blend curve to use
fTransitionDuration - The amount of time to blend the poses from the frozen value of the blend parameter and the live one
fBlendParameter - The blend paramter to pass into the cyclic blend generator DO NOT SET A VARIABLE ON THE REFERENCED BLENDER GENERATOR OR THIS WILL NOT FUNCTION PROPERLY!
EventToCrossBlend - This event will make cause a cross blend for m_fTransitionDuration happen between the froze blender and the live updating one
EventToFreezeBlendValue - This event will make the blend parameter used stay static until this node is deactivated or the cross blend is started
pBlenderGenerator - The blender generator to create transitions for

CurrentBlendMode - MODE_WAITINGFORBLENDING,MODE_BLENDING,MODE_FROZEN,MODE_DEFAULT,MODE_INACTIVE

BSBoneSwitchGenerator
ChildrenA
pDefaultGenerator - The default generator that's output if no children overwrite a bone

BSBoneSwitchGeneratorBoneData
spBoneWeight
pGenerator





 

 

Enjoy! Although I do realise this is a pretty niche thing in the modding community, if it helps even one person I think it's worth posting.

 

EDIT: This definitely isn't it, but these are the 'undocumented' Bethesda ones.