Over the last couple of months, I've explored a lot of the functionality of HKX or Havok files. As a result of reflecting on what is known and what information is out there, I feel as though I'm obligated to TRY to share what I've learned. My hope is that this guide will inspire new ideas and generally help out the Fallout 4 modding community.

Please leave a comment if : you find something confusing, are curious about something or don't understand something etc, if you have information and would like to add to this guide, If you find something that you know is wrong, if you found the guide useful.
Feel free to also leave any suggestions or criticism. I don't usually try to write guides, so anything will be helpful, but please be constructive.

Disclaimer : I am by no means an expert on animations or HAVOK. In fact, I'm still learning new thing's everyday. Please keep this in mind.

Resources Used :
I've found a couple of useful resources that shed some light on Havok.

The thought didn't occur to me for longest time but this site is very useful!
I don't believe this manual is complete, but there's still a lot of information here.

https://web.archive.org/web/*/http://anarchy.cn/manual*

I believe this is the 2013 version of HAVOK and this Github gives a lot of insight into the system.
It is technically outdated for the fallout 4 Havok system, but a lot of the information remains very useful!

https://github.com/Bewolf2/projectanarchy

These are some notes from other modders.

https://forums.nexusmods.com/index.php?/topic/3755680-bgs-custom-behavior-class-member-descriptions/

https://www.nexusmods.com/skyrim/articles/50508/?

Tools used/needed :

This tool by Dexestpp is incredibly useful, is invaluable and is required to edit and create HKX files without the offical havok tools.
https://github.com/Dexesttp/hkxpack

Preferably some text editor which has xml syntax checking functionality (make your life easier).
I use Notepad++ which I downloaded a plugin for named XML Tools.


Goals of this guide :

Go over the base information of the HKX file's used in Fallout 4.
Walk through manually creating a HKX file set.
Show off certain unique aspects of Havok system.
Explain how to use HAVOK's most basic animation type.
Highlight useful information on the Havok system.
Establish basic knowledge needed to explore HAVOK files.

HKXPack Setup :

This part of the guide will assume you're inexperienced with windows CMD or SHELL.

To unpack and pack HKX files, we need HKXPack. This program takes the machine code (from what I understand) and converts it a xml format so that it can be read and edited.

Dexestpp unfortunately never developed a UI for HKXPack. There is a UI included with ShadeAnimators animation kit, but I just use CMD or SHELL so I will go over those.

When downloading HKXpack, hkxpack-cli.jar will be the file you're after.

Note that java JRE 8 or higher is required to run HKXpack.

Open up a CMD or SHELL.

To unpack a HKX file, you have 2 options.

Either set the current directory of your command line to the path of HKXPack or reference it's path when calling it.

An example of a path : C:\Users\ajs52\Desktop\HKX_Examples\Baby\Animations

Referencing the path :

"hkxpack-cli.jar Path" unpack "HKX File to unpack path"

For me this looks like this.

C:\Users\ajs52\Desktop\hkxpack-cli.jar unpack "C:\Users\ajs52\Desktop\My_Skeleton.hkx"

Changing Directory :

CD "HKXPack Path (no file name)"

For me :

CD "C:\Users\ajs52\Desktop"

By doing this you can call HKXPack without referencing the full path.

For me :

hkxpack-cli.jar unpack My_Skeleton.hkx


To pack, simply replace "unpack" with "Pack" and make sure you reference a xml file rather than HKX file.

C:\Users\ajs52\Desktop\hkxpack-cli.jar pack "C:\Users\ajs52\Desktop\My_Skeleton.xml"

This will unpack a HKX file.

The unpacked or packed file is created in the same directory as the HKX or XML file your unpacking/packing.
You can change this. The how-to is on HKXPack's github.

Be certain the file type is XML and not TXT when packing(applies to all unpacked/created files).

To test unpacking/packing, using one of the methods above, paste the information contained in the spoiler below into a new xml file and pack it.

If a new file with the extension HKX appears, you've packed file. Now delete the xml file and unpack the HKX file. A new xml should appear.


  Reveal hidden contents
 

 


You should now be able to unpack and pack HKX files at this point.

Folder Setup :
From what I can tell, only certain folders can be used if you want functioning HKX files for FO4.
Some folders only work with specific object types, for instance actors (not 100% on this).
I don't know of every folder, but I have learned of one that works for most if not all object types.

Navigate to "Steam\steamapps\common\Fallout 4\Data\" and look for a your Meshes folder. If you can't find one, create one.
Then look for a folder named GenericBehaviors. Once again create one if necessary. GenericBehaviors is a folder used for
HKX Behavior's which are applied to many different objects.

This will be the directory in which your created HKX files will exist. There may be others that work.

The folders within GenericBehaviors and the files inside, I like to call HKX file sets.

Every set of HKX files follows the same format.

Folder (name whatever you want) :

Folder named Animations. Only needed if you have HKX animations.

Folder named Behaviors

Folder named CharacterAssets. Only needed if you use your own skeleton.

Folder named Character

HKX Project File (My_Project.hkx for example).

So for my current HKX project :

Fallout 4\Data\Meshes\GenericBehaviors\ASMOD is my main folder name.
(These are within ASMOD)
Fallout 4\Data\Meshes\GenericBehaviors\ASMOD\Animations is my animation folder.
Fallout 4\Data\Meshes\GenericBehaviors\ASMOD\Behaviors is my behavior folder.
Fallout 4\Data\Meshes\GenericBehaviors\ASMOD\CharacterAssets is my skeleton folder.
Fallout 4\Data\Meshes\GenericBehaviors\ASMOD\Character is my character folder.
Fallout 4\Data\Meshes\GenericBehaviors\ASMOD\ASMOD.hkx is my project file.


This for instance would be my HKX data for my first object.
If I wanted another object to have its own HKX data, I would repeat the process.

If you wanted to say, use animations from another HKX file set, you can. I haven't fully tested using other HKX file set data in a different set so you will have to play around with this. But it should work for animations.

You can also use the same HKX data for different object's.

Go ahead and create this file structure with a blank xml file in each of the subfolders and a one in the main folder(names don't matter for now).



Basic XML HKX file knowledge :

XML uses tags.
For example : <hkobject> </hkobject>
Tags can be anything.
All elements must be closed off : </hkobject>.
Elements can be nested.
For HKX files your 2 most common tags are hkObject and hkParam.
Elements can have attributes : <hkobject class="hkbVariableBindingSet" name="#6009" signature="0xe942f331"></hkobject>.
This has a tag of hkobject and three attributes. Class, name and signature.
Signature you can ignore.
Most of the time you reference other elements by their "name" attribute.
<hkparam name="variableBindingSet">#6009</hkparam>
This references another object by its name which is #6009.
Sometimes you reference data by its index.
<hkparam name="eventId">1</hkparam>
This references an event at index 1 of the event list.
This is a comment <!-- hi -->.
It will be ignored when packing the file. If any comment text is not within the comment syntax, you will not be able to pack.
The XML Prolog in HKX files is "<?xml version="1.0" encoding="ASCII" standalone="no"?>".
This cant have any lines or spaces behind it.

Very useful site for more xml information.
https://www.w3schools.com/xml/default.asp

Creating the base HKX data :
I highly suggest after you read this guide that you explore base game HKX files.
There's a bunch of cool stuff I haven't even explored and you will learn a lot!

Each HKX file set consists of 3 core files.

A Behavior File, Character File and Project File.
If needed there's also the Skeleton file and Animation files.

Project File :

This can be named whatever you like.

This file doesn't do much in itself but is needed.

Go ahead and paste the information contained within the spoiler into your project xml file. Don't close it as you need to edit some values.


  Hide contents
 

<?xml version="1.0" encoding="ASCII" standalone="no"?>

<hkpackfile classversion="11" contentsversion="hk_2014.1.0-r1">

<hksection name="__data__">

<hkobject class="hkRootLevelContainer" name="#90" signature="0x2772c11e">

<hkparam name="namedVariants" numelements="1">

<hkobject>

<hkparam name="name">hkbProjectData</hkparam>

<hkparam name="className">hkbProjectData</hkparam>

<hkparam name="variant">#91</hkparam>

</hkobject>

</hkparam>

</hkobject>

<hkobject class="hkbProjectData" name="#91" signature="0x363c1159">

<hkparam name="worldUpWS">(0.0 0.0 1.0 0.0)</hkparam>

<hkparam name="stringData">#92</hkparam>

<hkparam name="defaultEventMode">2</hkparam>

</hkobject>

<hkobject class="hkbProjectStringData" name="#92" signature="0xca08c2ba">

<hkparam name="animationFilenames" numelements="0"/>

<hkparam name="behaviorFilenames" numelements="0"/>

<hkparam name="characterFilenames" numelements="1">

<hkcstring>Characters\MyCharacter.hkx</hkcstring>

</hkparam>

<hkparam name="eventNames" numelements="0"/>

<hkparam name="animationPath"/>

<hkparam name="behaviorPath"/>

<hkparam name="characterPath"/>

<hkparam name="scriptsPath">Scripts\</hkparam>

<hkparam name="fullPathToSource"/>

</hkobject>

</hksection>

</hkpackfile>


 

 


This is your first glimpse into what you will messing with. I found this kind of data scary at first, but I've learned to not mind it as much.

Most of the values don't need to be changed here EXCEPT for characterFilenames.
This value must have the name of the xml file you created in the character folder.
So take some time to establish what your character file's name should be and replace MyCharacter with your xml file name.

WorldUpWS established what is "Up" (this setting is available in a couple of places).

From what project files I've seen, this is what they look like. Feel free to explore this file type.

Once you've set the name, you can close the file. We won't be packing any of these HKX file's until were finished.

Character File :

I will be using the xml comment syntax here in between objects (<!-- -->).

Paste this into your character xml file.

  Hide contents
 

<?xml version="1.0" encoding="ASCII" standalone="no"?>

<hkpackfile classversion="11" contentsversion="hk_2014.1.0-r1">

<hksection name="__data__">


<!-- No need to mess with this -->

<hkobject class="hkRootLevelContainer" name="#90" signature="0x2772c11e">

<hkparam name="namedVariants" numelements="1">

<hkobject>

<hkparam name="name">hkbCharacterData</hkparam>

<hkparam name="className">hkbCharacterData</hkparam>

<hkparam name="variant">#91</hkparam>

</hkobject>

</hkparam>

</hkobject>



<!-- Mo need to mess with this except when you are messing with physics stuff. -->

<hkobject class="hkbCharacterData" name="#91" signature="0xfec46c1f">

<hkparam name="characterControllerSetup">

<hkobject class="hkbCharacterControllerSetup" name="characterControllerSetup" signature="0xaf5f7339">

<hkparam name="rigidBodySetup">

<hkobject class="hkbRigidBodySetup" name="rigidBodySetup" signature="0x3b082f95">

<hkparam name="collisionFilterInfo">1</hkparam>

<hkparam name="type">255</hkparam>

<hkparam name="shapeSetup">

<hkobject class="hkbShapeSetup" name="shapeSetup" signature="0xd7ff86be">

<hkparam name="capsuleHeight">1.7000000476837158</hkparam>

<hkparam name="capsuleRadius">0.4000000059604645</hkparam>

<hkparam name="fileName"/>

<hkparam name="type">CAPSULE</hkparam>

</hkobject>

</hkparam>

</hkobject>

</hkparam>

<hkparam name="controllerCinfo">null</hkparam>

</hkobject>

</hkparam>

<!-- There's that "what is up" again

but now we can also define what axis is left/right forward/back.

-->

<hkparam name="modelUpMS">(0.0 0.0 1.0 0.0)</hkparam>

<hkparam name="modelForwardMS">(0.0 1.0 0.0 0.0)</hkparam>

<hkparam name="modelRightMS">(1.0 0.0 0.0 0.0)</hkparam>

<hkparam name="characterPropertyInfos" numelements="0"/>

<hkparam name="numBonesPerLod" numelements="0"/>

<hkparam name="characterPropertyValues">#92</hkparam>

<hkparam name="footIkDriverInfo">null</hkparam>

<hkparam name="handIkDriverInfo">null</hkparam>

<hkparam name="aiControlDriverInfo">null</hkparam>

<hkparam name="stringData">#93</hkparam>

<hkparam name="mirroredSkeletonInfo">#94</hkparam>

<hkparam name="boneAttachmentBoneIndices" numelements="0"/>

<hkparam name="boneAttachmentTransforms" numelements="0"/>

<hkparam name="scale">1.0</hkparam>

</hkobject>


<!-- I will explain this when we get to behaviors. This is how you would "globally" define variables. -->

<hkobject class="hkbVariableValueSet" name="#92" signature="0xeb5f7e25">

<hkparam name="wordVariableValues" numelements="0"/>

<hkparam name="quadVariableValues" numelements="0"/>

<hkparam name="variantVariableValues" numelements="0">

</hkparam>

</hkobject>


<!--This is where you need to change some things.


When your defining your havok animations, you must reference them under "animationBundleNameData".

From what I've seen, you can also use animationBundleFilenameData but I'm unaware of the difference so try at your own risk.


Set animationBundleNameData to 1 and then assetNames's numelements attribute to however many animations you plan to have.


Then, type <hkcstring>Animations\"Whatever you plan to name your animation".hkx</hkcstring>.

In xml <> says your starting something. </> says your ending it.

You can also use just <\> to say your not using anything for example <hkparam name="characterPropertyNames" numelements="0"/>. See below for more examples.


If you don't plan to use any HKX animations, set animationBundleNameData to 0 and remove the object held within animationBundleNameData.

Remove this if you don't want havok animations:


<hkobject>

<hkparam name="bundleName"/>

<hkparam name="assetNames" numelements="2">

<hkcstring>Animations\My_Animation_1.hkt</hkcstring>

<hkcstring>Animations\My_Animation_2.hkt</hkcstring>

</hkparam>

</hkobject>


Now you must name your character file under the name parameter. This can be anything but make sure it matches the file name.


The rigname parameter points to your skeleton file. If you plan to have a skeleton (needed for HKX animations) set rigname to

CharacterAssets\"whatever you want to name your skeleton".hkt


You also need to name your behavior file. Then reference it under behaviorFilename. Make sure you use the extension HKX rather than hkt in this case.


Everything else you don't need to mess with.

-->

<hkobject class="hkbCharacterStringData" name="#93" signature="0xb9d8a52">

<hkparam name="skinNames" numelements="0">

</hkparam>

<hkparam name="boneAttachmentNames" numelements="0"/>

<hkparam name="animationBundleNameData" numelements="1">

<hkobject>

<hkparam name="bundleName"/>

<hkparam name="assetNames" numelements="2">

<hkcstring>Animations\My_Animation_1.hkt</hkcstring>

<hkcstring>Animations\My_Animation_2.hkt</hkcstring>

</hkparam>

</hkobject>

</hkparam>

<hkparam name="animationBundleFilenameData" numelements="0">

</hkparam>

<hkparam name="characterPropertyNames" numelements="0"/>

<hkparam name="retargetingSkeletonMapperFilenames" numelements="0"/>

<hkparam name="lodNames" numelements="0"/>

<hkparam name="mirroredSyncPointSubstringsA" numelements="0"/>

<hkparam name="mirroredSyncPointSubstringsB" numelements="0"/>

<hkparam name="name">My_Character</hkparam>

<hkparam name="rigName">CharacterAssets\Simple_Single_Event_Skeleton.hkt</hkparam>

<hkparam name="ragdollName"/>

<hkparam name="behaviorFilename">Behaviors\Simple_Single_Event_Behavior.hkx</hkparam>

<hkparam name="luaScriptOnCharacterActivated"/>

<hkparam name="luaScriptOnCharacterDeactivated"/>

<hkparam name="luaFiles" numelements="0"/>

</hkobject>


<hkobject class="hkbMirroredSkeletonInfo" name="#94" signature="0x9f13052e">

<hkparam name="mirrorAxis">(1.0 0.0 0.0 0.0)</hkparam>

<hkparam name="bonePairMap" numelements="0"/>

<hkparam name="partitionPairMap" numelements="0"/>

</hkobject>


</hksection>

</hkpackfile>


 



Skeleton File :

If you don't care about animations skip this.

Paste this into your skeleton xml file.


  Hide contents
 

<?xml version="1.0" encoding="ASCII" standalone="no"?>

<hkpackfile classversion="11" contentsversion="hk_2014.1.0-r1">

<hksection name="__data__">

<!-- No need to mess with this.-->

<hkobject class="hkRootLevelContainer" name="#90" signature="0x2772c11e">

<hkparam name="namedVariants" numelements="1">

<hkobject>

<hkparam name="name">Merged Animation Container</hkparam>

<hkparam name="className">hkaAnimationContainer</hkparam>

<hkparam name="variant">#91</hkparam>

</hkobject>

</hkparam>

</hkobject>

<!-- Depending on what you plan to do, you may need to mess with this object. -->

<hkobject class="hkaAnimationContainer" name="#91" signature="0x26859f4c">

<hkparam name="skeletons" numelements="1">

#92

</hkparam>

<hkparam name="animations" numelements="0">

</hkparam>

<hkparam name="bindings" numelements="0">

</hkparam>

<hkparam name="attachments" numelements="0">

</hkparam>

<hkparam name="skins" numelements="0">

</hkparam>

</hkobject>


<!-- This is the important part.


From what I understand "Skeletons" in fallout can either be a completely separate nif file or be apart of the object the HKX data is attached to.

The different nif setup that I've seen appears to mostly involve ragdolls, so I wont go over that.


When creating a skeleton HKX file, you are specifying what nodes in a nif file are bones.


"Name" is referenced by certain things and I believe, based off of some base games files, that is should be the name of the first bone referenced.


From what skeletons I've looked into, the bone order should be root to children to children.

So in nif terms :


First NiNode I wanted to a be bone.


Child of First NiNode.


etc...


Second Child of First NiNode.


etc...



You will have to play around with this but it should be obvious if something is wrong as you wont get an animation/desired animation.


So based off the example my nif has a node named "Root Node" and that node has a child Node named "Child Node".


Be careful when referencing the actual "Root Node" of the nif, I'm not certain if this works and have observed some confusing outcomes from doing this.

So I wouldn't recommend it.


parentIndices is simply how you parent your bones.

So if I have 5 bones and wanted my fourth bone to be child of my fifth bone parent indices would be:


65535 0 0 4 0 This is index based so 0 is 1 etc. As a result 4 is the 5th bone.

Bone : 1 2 3 4 5


Parenting a bone just means that if the parent bone moves, so do the child bones.


The value 65535 is always used for the first bone and I believe refers to some kind of world to model parent. Something like that.


Lock Translation is something I haven't used but I think it restricts movement in certain situations.


ReferencePose :


Each bone needs a reference pose.

The reference pose consists of three vector values.

(0.0 0.0 0.0 0.0) is an example of a vector value.

The first one is translation, the second is rotation and the third is scale.

Rotation uses quaternions, I will include a link to a tool which converts between Euler angles

when we get to animations.


Reference poses are like the starting position's of your model.

Be certain when creating poses in your skeleton file, that the nif node's pose's (the translation rotation and scale) match the skeleton havok file poses.


So if my nif has a bone which is x45 y50 z55, my skeleton bone vector will be (45.0 50.0 55.0).


If I wanted my object to be rotated 90 degrees along the x axis, my quaternions would be

(0.0 0.707 0.0 0.707), this is all relative of course.


Finally scale which should be 1.0 1.0 1.0 1.0 by default.


For translation and scale, I'm unaware if the fourth value in the vector has any meaning. For the character's pose, occasionally it has a value other than 0.0. Can't say why.


All together :


(45.0 50.0 55.0 0.0)(0.0 0.707 0.0 0.707)(1.0 1.0 1.0 0.0)


Be certain to set the quaternion's fourth value to 1.0 and everything else to 0.0 if you don't desire any rotation.


Everything else I haven't touched and generally doesn't need to be modified.


Be sure to set the numelements to the correct amount.

-->

<hkobject class="hkaSkeleton" name="#92" signature="0xfec1cedb">

<hkparam name="name">Root Node</hkparam>

<hkparam name="parentIndices" numelements="2">65535 0</hkparam>

<hkparam name="bones" numelements="2">

<hkobject>

<hkparam name="name">Root Node</hkparam>

<hkparam name="lockTranslation">false</hkparam>

</hkobject>

<hkobject>

<hkparam name="name">Child Node</hkparam>

<hkparam name="lockTranslation">false</hkparam>

</hkobject>

</hkparam>

<hkparam name="referencePose" numelements="2">

(0.0 0.0 0.0 1.0)(0.0 0.0 0.0 1.0)(1.0 1.0 1.0 0.0)

(0.0 0.0 0.0 1.0)(0.0 0.0 0.0 1.0)(1.0 1.0 1.0 0.0)

</hkparam>

<hkparam name="referenceFloats" numelemensts="0"/>

<hkparam name="floatSlots" numelements="0"/>

<hkparam name="localFrames" numelements="0"/>

<hkparam name="partitions" numelements="0"/>

</hkobject>

</hksection>

</hkpackfile>


 



Behavior Files :

This is where thing's get really interesting and confusing. Behavior file's do a hole slew of thing's but the really cool part is with the object's which can manipulate your animation's.
This is also where the scale of the HKX file's gets big and a lot more complicated.

Base Object's and Setup up a behavior file :

This is the base information of all behavior file's that will generally exist.
Notice how each, object except for hkRootLevelContainer, is referenced by another object.
Go ahead and paste this into your behavior xml file. Make sure there's no spaces above the first line or below the last line.


  Hide contents

<?xml version="1.0" encoding="ASCII" standalone="no"?>
<hkpackfile classversion="11" contentsversion="hk_2014.1.0-r1">
<hksection name="__data__">



<hkobject class="hkRootLevelContainer" name="#90" signature="0x2772c11e">
<hkparam name="namedVariants" numelements="1">
<hkobject>
<hkparam name="name">hkbBehaviorGraph</hkparam>
<hkparam name="className">hkbBehaviorGraph</hkparam>
<hkparam name="variant">#91</hkparam>
</hkobject>
</hkparam>
</hkobject>



<hkobject class="hkbBehaviorGraph" name="#91" signature="0xfdedb83b">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">My Behavior File Name.hkb</hkparam>
<hkparam name="variableMode">VARIABLE_MODE_DISCARD_WHEN_INACTIVE</hkparam>
<hkparam name="rootGenerator">#92</hkparam>
<hkparam name="data">#106</hkparam>
</hkobject>



<hkobject class="hkbStateMachine" name="#92" signature="0xa5896bcf">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">RootStateMachine</hkparam>
<hkparam name="eventToSendWhenStateOrTransitionChanges">
<hkobject class="hkbEvent" name="eventToSendWhenStateOrTransitionChanges" signature="0x3e0fd810">
<hkparam name="id">-1</hkparam>
<hkparam name="payload">null</hkparam>
</hkobject>
</hkparam>
<hkparam name="startStateIdSelector">null</hkparam>
<hkparam name="startStateId">0</hkparam>
<hkparam name="returnToPreviousStateEventId">-1</hkparam>
<hkparam name="randomTransitionEventId">-1</hkparam>
<hkparam name="transitionToNextHigherStateEventId">-1</hkparam>
<hkparam name="transitionToNextLowerStateEventId">-1</hkparam>
<hkparam name="syncVariableIndex">-1</hkparam>
<hkparam name="wrapAroundStateId">false</hkparam>
<hkparam name="maxSimultaneousTransitions">32</hkparam>
<hkparam name="startStateMode">START_STATE_MODE_DEFAULT</hkparam>
<hkparam name="selfTransitionMode">SELF_TRANSITION_MODE_NO_TRANSITION</hkparam>
<hkparam name="states" numelements="0">
</hkparam>
<hkparam name="wildcardTransitions">null</hkparam>
</hkobject>


<hkobject class="hkbBehaviorGraphData" name="#106" signature="0x907a8222">
<hkparam name="attributeDefaults" numelements="0"/>
<hkparam name="variableInfos" numelements="0">
</hkparam>
<hkparam name="characterPropertyInfos" numelements="0"/>
<hkparam name="eventInfos" numelements="0">
</hkparam>
<hkparam name="variableBounds" numelements="0"/>
<hkparam name="variableInitialValues">#117</hkparam>
<hkparam name="stringData">#118</hkparam>
</hkobject>



<hkobject class="hkbVariableValueSet" name="#117" signature="0xeb5f7e25">
<hkparam name="wordVariableValues" numelements="0">
</hkparam>
<hkparam name="quadVariableValues" numelements="0"/>
<hkparam name="variantVariableValues" numelements="0"/>
</hkobject>



<hkobject class="hkbBehaviorGraphStringData" name="#118" signature="0x1bd27f38">
<hkparam name="eventNames" numelements="0">
</hkparam>
<hkparam name="attributeNames" numelements="0"/>
<hkparam name="variableNames" numelements="0">
</hkparam>
<hkparam name="characterPropertyNames" numelements="0"/>
</hkobject>


</hksection>
</hkpackfile>



Common Information. :
Up to this point, I've given you the entirety of the HKX information to paste into each file. But once you get to behavior graphs, you'll be spending a lot of time pasting and removing new object's.
So to give you experience doing this, I've separated each object for you to manually place and edit.
With the base information in place, now we will add some common elements of behavior files.
These object's will be a part of the final behavior file your constructing.
You should paste each of these object's into your xml behavior file. Don't worry about there location. Place them wherever it's least confusing.

These base object's which we've already defined need to be adjusted when we want to create new variables and events.
hkbVariableValueSet
hkbBehaviorGraphStringData
hkbBehaviorGraphData

Defining Variables.


  Hide contents

Defining a variable requires changing a minimum of 3 parameters. VariableInfos, wordVariableValues and variableNames.
VariableInfos is a parameter of hkbBehaviorGraphData. wordVariableValues is a parameter of hkbVariableValueSet and variableNames is a parameter of hkbBehaviorGraphStringData.

I will define 1 variable.

This is located within the hkbBehaviorGraphData object.
The basic object within variableInfos for each variable is this :

<hkobject>
<hkparam name="role">
<hkobject class="hkbRoleAttribute" name="role" signature="0xfecef669">
<hkparam name="role">ROLE_DEFAULT</hkparam>
<hkparam name="flags">FLAG_NONE</hkparam>
</hkobject>
</hkparam>
<hkparam name="type">VARIABLE_TYPE_REAL</hkparam>
</hkobject>
So for each variable you define, you can copy and paste this object under variableInfos.
Make sure you adjust the numelements.

Example :

<hkparam name="variableInfos" numelements="1">
<hkobject>
<hkparam name="role">
<hkobject class="hkbRoleAttribute" name="role" signature="0xfecef669">
<hkparam name="role">ROLE_DEFAULT</hkparam>
<hkparam name="flags">FLAG_NONE</hkparam>
</hkobject>
</hkparam>
<!-- "Type" specifys the type of variable. Here's a quick reference to the possible values.
Keep in mind this is from the 2013 version of HAVOK .
https://github.com/Bewolf2/projectanarchy/blob/bec6e7270c9dc797a12d167d3e8942866065364e/Source/Behavior/Behavior/Variables/hkbVariableInfo.h
-->
<hkparam name="type">VARIABLE_TYPE_REAL</hkparam>
</hkobject>
</hkparam>

This is located within the hkbVariableValueSet.

For each variable here, this is the object you paste :
Make sure the value isn't a float(reason may be because of a hkx bug).

<hkobject>
<hkparam name="value">30</hkparam>
</hkobject>


Example :

<hkparam name="wordVariableValues" numelements="1">
<hkobject>
<hkparam name="value">30</hkparam>
</hkobject>
</hkparam>

This is within the hkbBehaviorGraphStringData object.

This one uses the tag hkcstring. For each variable define :

<hkcstring>Whatever you want for a name</hkcstring>

Example :

<hkparam name="variableNames" numelements="1">
<!-- Name your variable whatever you want. The index will be how you reference it. -->
<hkcstring>Blending_Duration</hkcstring>
</hkparam>

Finally there's the bounds which is an optional thing. It is located within hkbBehaviorGraphData.

<hkparam name="variableBounds" numelements="1">
<hkobject>
<hkparam name="min">
<hkobject class="hkbVariableValue" name="min" signature="0xb99bd6a">
<hkparam name="value">0</hkparam>
</hkobject>
</hkparam>
<hkparam name="max">
<hkobject class="hkbVariableValue" name="max" signature="0xb99bd6a">
<hkparam name="value">5</hkparam>
</hkobject>
</hkparam>
</hkobject>
</hkparam>

This would restrict the value to with 0 - 5.



Defining Events.

 

Same routine as defining variables.
I will define 1 event.

  Hide contents


Located within the hkbBehaviorGraphData object.

Object for each event :
<hkobject>
<hkparam name="flags">0</hkparam>
</hkobject>

Example :
<hkparam name="eventInfos" numelements="1">
<hkobject>
<hkparam name="flags">0</hkparam>
</hkobject>
</hkparam>

Located within the hkbBehaviorGraphStringData object.

Object for each event :

<hkcstring>Whatever you want your event to be named</hkcstring>

Examoke :
<hkparam name="eventNames" numelements="1">
<hkcstring>Starting_State_Event</hkcstring>
</hkparam>




Additional Information On Events:


  Hide contents


When defining event's, they do not need to be associated with a state. You could define them just to receive them in papyrus if you wanted.
I believe the game engine is always listening for some event's such as SoundPlay.
It's possible to send event's to a object's behavior graph through it's nif's animation annotation's by writing the event's name in the annotation. All of this can be observed in the base game files.




Defining these variables and events will be something that you will do often.

VariableBindingSet.


  Hide contents


VariableBindingSet allow you to reference defined variables which are then binded to object parameters thus
allowing you to control these parameters from a external source such as papyrus.
When defining variables in behavior files, these variable are only usable by the behavior file there defined in.
If you wanted, you could define these variable in the character HKX file and use them across all behavior files of your HKX set.

Here's a simple example.

This is a BSTimerModifer. There are two timer modifiers available. The HAVOK one and the Bethesda one.
The main difference between the two is that the Bethesda modifier allows you to continuously repeat the timer
while the HAVOK one only allows the timer to run once. For this specific modifier, you can optionally send out an event when the timer ends. If you don't wish to send out an event, write -1.
ID refers to the index of the event to send. Payload is a way to send information with the event.
I haven't explored using payload yet but it's used by the base game behavior file's so you can reference them as examples.

<!-- Paste This Object. -->
<hkobject class="BSTimerModifier" name="#4000" signature="0x9266ffac">
<hkparam name="variableBindingSet">#4001</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">My Timer Modifier</hkparam>
<hkparam name="enable">true</hkparam>
<hkparam name="alarmTimeSeconds">30.0</hkparam>
<hkparam name="alarmEvent">
<hkobject class="hkbEventProperty" name="alarmEvent" signature="0xdb38a15">
<hkparam name="id">-1</hkparam>
<hkparam name="payload">null</hkparam>
</hkobject>
</hkparam>
<hkparam name="resetAlarm">true</hkparam>
</hkobject>

With the bindingset below, I'm binding a variable which I've defined at index 0 to the parameter named alarmTimeSeconds.
This will allow me to call the papyrus function SetAnimationVariableFloat and pass it the variable name and then any float
value which will then result in my timer duration being set to the passed value.
One of the cool things about this is that a lot of time you can update the parameters as the object
the data is binded to, is running. So I could be playing an animation with this modifier attached to it which
would then send out an event when the timer ends causing a state switch. This event can trigger a state switch.
By calling SetAnimationVariableFloat, I can change when the state switch will occur, even if it's already active.
You can apply this "binding" logic to as many parameters and object's as you like but remember that papyrus can only send integers, floats and booleans to the behavior graph, so you might have to get creative.


bitIndex is generally always 255.
bindingtype might need to be changed depending on the type of variable, but mostly shouldn't need to be changed.
indexOfBindingToEnable doesn't need to be touched. Just something else to explore.
Notice that this block is referenced by it's name at the timermodifier's variableBindingSet parameter.

<!-- Paste This Object. -->
<hkobject class="hkbVariableBindingSet" name="#4001" signature="0xe942f339">
<hkparam name="bindings" numelements="1">
<!-- This is the first binding. -->
<hkobject>
<hkparam name="memberPath">alarmTimeSeconds</hkparam>
<hkparam name="variableIndex">0</hkparam>
<hkparam name="bitIndex">255</hkparam>
<hkparam name="bindingType">BINDING_TYPE_VARIABLE</hkparam>
</hkobject>

</hkparam>
<hkparam name="indexOfBindingToEnable">-1</hkparam>
</hkobject>




Conditions :
If you've ever programmed, you know what this is.
These simply allow you to "condition" things so that they only occur if the conditions are satisfied.

  Hide contents


Note that this isn't the correct structure for this block but don't worry as HKXpack will remove the unneeded parameters.

This will allow you to condition things based off the evaluation of the expression.
Here I conditioned something based off of whether or not Time_To_End (a variable of type int I've defined) is 0.
Make sure you don't use "if", it's not necessary and won't work.
Here's a reference for possible supported operators.
https://github.com/Bewolf2/projectanarchy/blob/bec6e7270c9dc797a12d167d3e8942866065364e/Source/Behavior/Behavior/Utils/hkbCompiledExpressionSet.h

Once again this information is all located at the Github.

<!-- Paste This Object. -->
<hkobject class="hkbExpressionCondition" name="#1555" signature="0x4a3ac449">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">My_Condition</hkparam>
<hkparam name="enable">true</hkparam>
<hkparam name="expression"> First variable name in which I've defined == 0</hkparam>
</hkobject>

It might be possible to send an event if something evaluates to true but I only tested this with hkbEvaluateExpressionModifier (included in my notes).





StateMachine :

Information on what a State Machine is .

A state machine object establishes a "state machine".
We can establish what state we start in, what transition's can be used by any state, the state's that we have and other things.

StartStateId, wildcardTransitions and states are the three parameter's you'll be adjusting most of the time.


  Hide contents

<hkobject class="hkbStateMachine" name="#92" signature="0xa5896bcf">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">RootStateMachine</hkparam>
<hkparam name="eventToSendWhenStateOrTransitionChanges">
<hkobject class="hkbEvent" name="eventToSendWhenStateOrTransitionChanges" signature="0x3e0fd810">
<hkparam name="id">-1</hkparam>
<hkparam name="payload">null</hkparam>
</hkobject>
</hkparam>
<hkparam name="startStateIdSelector">null</hkparam>
<hkparam name="startStateId">0</hkparam>
<hkparam name="returnToPreviousStateEventId">-1</hkparam>
<hkparam name="randomTransitionEventId">-1</hkparam>
<hkparam name="transitionToNextHigherStateEventId">-1</hkparam>
<hkparam name="transitionToNextLowerStateEventId">-1</hkparam>
<hkparam name="syncVariableIndex">-1</hkparam>
<hkparam name="wrapAroundStateId">false</hkparam>
<hkparam name="maxSimultaneousTransitions">32</hkparam>
<hkparam name="startStateMode">START_STATE_MODE_DEFAULT</hkparam>
<hkparam name="selfTransitionMode">SELF_TRANSITION_MODE_NO_TRANSITION</hkparam>
<hkparam name="states" numelements="0">
</hkparam>
<hkparam name="wildcardTransitions">null</hkparam>
</hkobject>




Read about transition array's (little farther down) before this.
Wild Card Transition's is a way to define transition's in which any state that's apart of the state machine can use.
So If I had five state's and I wanted to be able to transition from any four of those state's to a fifth state, I could easily do so without defining multiple transition array objects's -
by just using a single wildcard transitions.
To create a wildcard transition, reference the transition array information below. Then adjust your state machine's wildcardTransitions value to the name attribute value of your transition array.
Finally, change your transition array's flag parameter to the value "FLAG_IS_LOCAL_WILDCARD".

Here's how this would look :


  Hide contents

<hkobject class="hkbStateMachine" name="#92" signature="0xa5896bcf">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">RootStateMachine</hkparam>
<hkparam name="eventToSendWhenStateOrTransitionChanges">
<hkobject class="hkbEvent" name="eventToSendWhenStateOrTransitionChanges" signature="0x3e0fd810">
<hkparam name="id">-1</hkparam>
<hkparam name="payload">null</hkparam>
</hkobject>
</hkparam>
<hkparam name="startStateIdSelector">null</hkparam>
<hkparam name="startStateId">0</hkparam>
<hkparam name="returnToPreviousStateEventId">-1</hkparam>
<hkparam name="randomTransitionEventId">-1</hkparam>
<hkparam name="transitionToNextHigherStateEventId">-1</hkparam>
<hkparam name="transitionToNextLowerStateEventId">-1</hkparam>
<hkparam name="syncVariableIndex">-1</hkparam>
<hkparam name="wrapAroundStateId">false</hkparam>
<hkparam name="maxSimultaneousTransitions">32</hkparam>
<hkparam name="startStateMode">START_STATE_MODE_DEFAULT</hkparam>
<hkparam name="selfTransitionMode">SELF_TRANSITION_MODE_NO_TRANSITION</hkparam>
<hkparam name="states" numelements="0">
</hkparam>
<hkparam name="wildcardTransitions">#88888</hkparam>
</hkobject>
<hkobject class="hkbStateMachineTransitionInfoArray" name="#88888" signature="0x704a19af">
<hkparam name="transitions" numelements="1">
<hkobject>
<hkparam name="triggerInterval">
<hkobject class="hkbStateMachineTimeInterval" name="triggerInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<hkparam name="initiateInterval">
<hkobject class="hkbStateMachineTimeInterval" name="initiateInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<hkparam name="transition">null</hkparam>
<hkparam name="condition">null</hkparam>
<hkparam name="eventId">0</hkparam>
<hkparam name="toStateId">0</hkparam>
<hkparam name="fromNestedStateId">0</hkparam>
<hkparam name="toNestedStateId">0</hkparam>
<hkparam name="priority">0</hkparam>
<hkparam name="flags">FLAG_IS_LOCAL_WILDCARD</hkparam>
</hkobject>
</hkparam>
</hkobject>



We will be using the wildcard transition to transition back to the starting state ( our starting state ID is 0) from any state when we receive the event at index 0.

States :

Each of these object's represent a state. They must be reference by the state machine (coming up).
enterNotifyEvents sends an events when the state is transitioned to.
exitNotifyEvents sends an events when the state has been left.
Transitions will reference a Transition array(up next).
generator will reference a Clipgenerator, also up next.
ID must be a unique number as this how the state if referenced.

<!-- Paste two of these into your behavior file. Adjust there name attribute's, there name's and there ID's so they are unique. -->
<hkobject class="hkbStateMachineStateInfo" name="#109" signature="0x39d76713">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="listeners" numelements="0"/>
<hkparam name="enterNotifyEvents">null</hkparam>
<hkparam name="exitNotifyEvents">null</hkparam>
<hkparam name="transitions">null</hkparam>
<hkparam name="generator">null</hkparam>
<hkparam name="name">Put a unique name here</hkparam>
<hkparam name="stateId">0</hkparam>
<hkparam name="probability">1.0</hkparam>
<hkparam name="enable">true</hkparam>
</hkobject>

Transition arrays.


Transitions arrays are the logic which determines what state you will "transition" to.
They are referenced by states under their "transitions" attribute.
Statemachines also use them.

 

  Hide contents

<!-- Paste one of these into your behavior file. Adjust it's name attribute's, name and ID so that it's unique.
<hkobject class="hkbStateMachineTransitionInfoArray" name="#111" signature="0x704a19af">
<hkparam name="transitions" numelements="1">
<hkobject>
<!-- I haven't played with this but you should be able to figure this out by using the HAVOK 2013 source files. -->
<hkparam name="triggerInterval">
<hkobject class="hkbStateMachineTimeInterval" name="triggerInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<hkparam name="initiateInterval">
<hkobject class="hkbStateMachineTimeInterval" name="initiateInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<!-- Transition here refers to the "transition effect" that occurs when you leave this state. These are mostly blending effects.
EventID specifys which event will cause a transition to what state.
For this transition array, when we receive the second event which we defined, we will transition to the second state (assuming the second state has a ID of 1).
The rest I haven't explored.

-->
<hkparam name="transition">null</hkparam>
<hkparam name="condition">null</hkparam>
<hkparam name="eventId">1</hkparam>
<hkparam name="toStateId">1</hkparam>
<hkparam name="fromNestedStateId">0</hkparam>
<hkparam name="toNestedStateId">0</hkparam>
<hkparam name="priority">0</hkparam>
<hkparam name="flags">0</hkparam>
</hkobject>
</hkparam>
</hkobject>




At this point, using the guide above, your behavior file should look something like this.
We've defined the base behavior file data. Created two state's. Defined two transition data array's (including wildcard).
Defined a single variable and event. Defined a modifier with a variablebinding set and finally defined a single condition object.


  Hide contents

<?xml version="1.0" encoding="ASCII" standalone="no"?>
<hkpackfile classversion="11" contentsversion="hk_2014.1.0-r1">
<hksection name="__data__">



<hkobject class="hkRootLevelContainer" name="#90" signature="0x2772c11e">
<hkparam name="namedVariants" numelements="1">
<hkobject>
<hkparam name="name">hkbBehaviorGraph</hkparam>
<hkparam name="className">hkbBehaviorGraph</hkparam>
<hkparam name="variant">#91</hkparam>
</hkobject>
</hkparam>
</hkobject>



<hkobject class="hkbBehaviorGraph" name="#91" signature="0xfdedb83b">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">My Behavior File Name.hkb</hkparam>
<hkparam name="variableMode">VARIABLE_MODE_DISCARD_WHEN_INACTIVE</hkparam>
<hkparam name="rootGenerator">#92</hkparam>
<hkparam name="data">#106</hkparam>
</hkobject>


<hkobject class="hkbStateMachine" name="#92" signature="0xa5896bcf">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">RootStateMachine</hkparam>
<hkparam name="eventToSendWhenStateOrTransitionChanges">
<hkobject class="hkbEvent" name="eventToSendWhenStateOrTransitionChanges" signature="0x3e0fd810">
<hkparam name="id">-1</hkparam>
<hkparam name="payload">null</hkparam>
</hkobject>
</hkparam>
<hkparam name="startStateIdSelector">null</hkparam>
<hkparam name="startStateId">0</hkparam>
<hkparam name="returnToPreviousStateEventId">-1</hkparam>
<hkparam name="randomTransitionEventId">-1</hkparam>
<hkparam name="transitionToNextHigherStateEventId">-1</hkparam>
<hkparam name="transitionToNextLowerStateEventId">-1</hkparam>
<hkparam name="syncVariableIndex">-1</hkparam>
<hkparam name="wrapAroundStateId">false</hkparam>
<hkparam name="maxSimultaneousTransitions">32</hkparam>
<hkparam name="startStateMode">START_STATE_MODE_DEFAULT</hkparam>
<hkparam name="selfTransitionMode">SELF_TRANSITION_MODE_NO_TRANSITION</hkparam>
<hkparam name="states" numelements="0">
</hkparam>
<hkparam name="wildcardTransitions">#88888</hkparam>
</hkobject>

<hkobject class="hkbStateMachineTransitionInfoArray" name="#88888" signature="0x704a19af">
<hkparam name="transitions" numelements="1">
<hkobject>
<hkparam name="triggerInterval">
<hkobject class="hkbStateMachineTimeInterval" name="triggerInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<hkparam name="initiateInterval">
<hkobject class="hkbStateMachineTimeInterval" name="initiateInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<hkparam name="transition">null</hkparam>
<hkparam name="condition">null</hkparam>
<hkparam name="eventId">0</hkparam>
<hkparam name="toStateId">0</hkparam>
<hkparam name="fromNestedStateId">0</hkparam>
<hkparam name="toNestedStateId">0</hkparam>
<hkparam name="priority">0</hkparam>
<hkparam name="flags">FLAG_IS_LOCAL_WILDCARD</hkparam>
</hkobject>
</hkparam>
</hkobject>


<hkobject class="hkbBehaviorGraphData" name="#106" signature="0x907a8222">
<hkparam name="attributeDefaults" numelements="0"/>
<hkparam name="variableInfos" numelements="1">
<hkobject>
<hkparam name="role">
<hkobject class="hkbRoleAttribute" name="role" signature="0xfecef669">
<hkparam name="role">ROLE_DEFAULT</hkparam>
<hkparam name="flags">FLAG_NONE</hkparam>
</hkobject>
</hkparam>
<hkparam name="type">VARIABLE_TYPE_REAL</hkparam>
</hkobject>
</hkparam>
<hkparam name="characterPropertyInfos" numelements="0"/>
<hkparam name="eventInfos" numelements="1">
<hkobject>
<hkparam name="flags">0</hkparam>
</hkobject>
</hkparam>
<hkparam name="variableBounds" numelements="0"/>
<hkparam name="variableInitialValues">#117</hkparam>
<hkparam name="stringData">#118</hkparam>
</hkobject>



<hkobject class="hkbVariableValueSet" name="#117" signature="0xeb5f7e25">
<hkparam name="wordVariableValues" numelements="1">
<hkobject>
<hkparam name="value">30</hkparam>
</hkobject>
</hkparam>
<hkparam name="quadVariableValues" numelements="0"/>
<hkparam name="variantVariableValues" numelements="0"/>
</hkobject>



<hkobject class="hkbBehaviorGraphStringData" name="#118" signature="0x1bd27f38">
<hkparam name="eventNames" numelements="1">
<hkcstring>Start State Event</hkcstring>
</hkparam>
<hkparam name="attributeNames" numelements="0"/>
<hkparam name="variableNames" numelements="1">
<hkcstring>My_First_Variable</hkcstring>
</hkparam>
<hkparam name="characterPropertyNames" numelements="0"/>
</hkobject>


<hkobject class="BSTimerModifier" name="#4000" signature="0x9266ffac">
<hkparam name="variableBindingSet">#4001</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">My Timer Modifier</hkparam>
<hkparam name="enable">true</hkparam>
<hkparam name="alarmTimeSeconds">30.0</hkparam>
<hkparam name="alarmEvent">
<hkobject class="hkbEventProperty" name="alarmEvent" signature="0xdb38a15">
<hkparam name="id">-1</hkparam>
<hkparam name="payload">null</hkparam>
</hkobject>
</hkparam>
<hkparam name="resetAlarm">true</hkparam>
</hkobject>



<hkobject class="hkbVariableBindingSet" name="#4001" signature="0xe942f339">
<hkparam name="bindings" numelements="1">
<!-- This is the first binding. -->
<hkobject>
<hkparam name="memberPath">alarmTimeSeconds</hkparam>
<hkparam name="variableIndex">0</hkparam>
<hkparam name="bitIndex">255</hkparam>
<hkparam name="bindingType">BINDING_TYPE_VARIABLE</hkparam>
</hkobject>

</hkparam>
<hkparam name="indexOfBindingToEnable">-1</hkparam>
</hkobject>



<hkobject class="hkbExpressionCondition" name="#1555" signature="0x4a3ac449">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">My_Condition</hkparam>
<hkparam name="enable">true</hkparam>
<hkparam name="expression">My_First_Variable == 0</hkparam>
</hkobject>


<hkobject class="hkbStateMachineStateInfo" name="#8888" signature="0x39d76713">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="listeners" numelements="0"/>
<hkparam name="enterNotifyEvents">null</hkparam>
<hkparam name="exitNotifyEvents">null</hkparam>
<hkparam name="transitions">null</hkparam>
<hkparam name="generator">null</hkparam>
<hkparam name="name">My Starting State</hkparam>
<hkparam name="stateId">0</hkparam>
<hkparam name="probability">1.0</hkparam>
<hkparam name="enable">true</hkparam>
</hkobject>

<hkobject class="hkbStateMachineTransitionInfoArray" name="#111" signature="0x704a19af">
<hkparam name="transitions" numelements="1">
<hkobject>
<!-- I haven't played with this but you should be able to figure this out by using the HAVOK 2013 source files. -->
<hkparam name="triggerInterval">
<hkobject class="hkbStateMachineTimeInterval" name="triggerInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<hkparam name="initiateInterval">
<hkobject class="hkbStateMachineTimeInterval" name="initiateInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<!-- Transition here refers to the "transition effect" that occurs when you leave this state. These are mostly blending effects.
Condition's I will talk about next.
EventID specifies which event will cause a transition to what state.
Here my second event defined will transition to my second state if it is received while in this state.
The rest I haven't explored.

-->
<hkparam name="transition">null</hkparam>
<hkparam name="condition">null</hkparam> <!-- Read the next section for information on this. -->
<hkparam name="eventId">1</hkparam>
<hkparam name="toStateId">1</hkparam>
<hkparam name="fromNestedStateId">0</hkparam>
<hkparam name="toNestedStateId">0</hkparam>
<hkparam name="priority">0</hkparam>
<hkparam name="flags">0</hkparam>
</hkobject>
</hkparam>
</hkobject>

<hkobject class="hkbStateMachineStateInfo" name="#9999" signature="0x39d76713">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="listeners" numelements="0"/>
<hkparam name="enterNotifyEvents">null</hkparam>
<hkparam name="exitNotifyEvents">null</hkparam>
<hkparam name="transitions">null</hkparam>
<hkparam name="generator">null</hkparam>
<hkparam name="name">My Second State</hkparam>
<hkparam name="stateId">1</hkparam>
<hkparam name="probability">1.0</hkparam>
<hkparam name="enable">true</hkparam>
</hkobject>




</hksection>
</hkpackfile>




Now lets move on to adding our state's generators (one will have a modifier attached to it).

Generator's are generally how you implement the functionality to play animation's, into you behavior file.
The basic generator setup for Havok animations is created by using the ClipGenerator Object.
This object will be referenced in the generator parameter of the state that will use it.
I won't go over everything except the necessary parts.
animationBindingIndex should be 65535.
animationName must reference your animation folder and then the animation file name with the extension "hkt".
We haven't created any animation's yet, but you should have a empty xml file in your animation folder if you desire to create animations. Reference it's file name here.
This will run a HAVOK animation when the state which the generator is a child of is transitioned to.

<hkobject class="hkbClipGenerator" name="#776" signature="0xd4cc9f6">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">First Animation</hkparam>
<hkparam name="animationBundleName"/>
<hkparam name="animationName">Animations\My Animation Name.hkt</hkparam>
<hkparam name="triggers">null</hkparam>
<hkparam name="userPartitionMask">0</hkparam>
<hkparam name="cropStartAmountLocalTime">0.0</hkparam>
<hkparam name="cropEndAmountLocalTime">0.0</hkparam>
<hkparam name="startTime">0.0</hkparam>
<hkparam name="playbackSpeed">1.0</hkparam>
<hkparam name="enforcedDuration">0.0</hkparam>
<hkparam name="userControlledTimeFraction">0.0</hkparam>
<hkparam name="animationBindingIndex">65535</hkparam>
<hkparam name="mode">MODE_SINGLE_PLAY</hkparam>
<hkparam name="flags">0</hkparam>
</hkobject>

Go ahead and paste two of this object into your behavior file. Make sure you change both of the Clipgenerator's name parameter's and the Clipgenerator's name's to something unique.
To hook them up with the state, reference there name attribute's under the state's generator parameter like this :

<hkobject class="hkbStateMachineStateInfo" name="#119" signature="0x39d76713">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="listeners" numelements="0"/>
<hkparam name="enterNotifyEvents">null</hkparam>
<hkparam name="exitNotifyEvents">null</hkparam>
<hkparam name="transitions">null</hkparam>
<hkparam name="generator">#776</hkparam> <!--Right here. -->
<hkparam name="name">Second State</hkparam>
<hkparam name="stateId">1</hkparam>
<hkparam name="probability">1.0</hkparam>
<hkparam name="enable">true</hkparam>
</hkobject>


Now lets setup that timer modifier we defined.

Each generator can have as many modifiers as you desire.

To use a modifier, you have to create a hkbModifierGenerator object.
This is simply a wrapper object for your clip generator.

Here's the object :

<!-- Paste This. -->
<hkobject class="hkbModifierGenerator" name="#121" signature="0xc499fc9e">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">1</hkparam>
<hkparam name="name">My Modifier Generator</hkparam>
<hkparam name="modifier">null</hkparam>
<hkparam name="generator">null</hkparam>
</hkobject>

So instead of referencing your clip generator's name attribute value in the state, reference this generator's name.
Then under hkbModifierGenerator "generator" reference the clip generator's name attribute value.
The modifier attribute of hkbModifierGenerator should then reference the timer modifier's name.

For instance :

  Hide contents

<hkobject class="hkbStateMachineStateInfo" name="#8888" signature="0x39d76713">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="listeners" numelements="0"/>
<hkparam name="enterNotifyEvents">null</hkparam>
<hkparam name="exitNotifyEvents">null</hkparam>
<hkparam name="transitions">null</hkparam>
<hkparam name="generator">#7665</hkparam>
<hkparam name="name">My Starting State</hkparam>
<hkparam name="stateId">0</hkparam>
<hkparam name="probability">1.0</hkparam>
<hkparam name="enable">true</hkparam>
</hkobject>

<hkobject class="hkbModifierGenerator" name="#7665" signature="0xc499fc9e">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">1</hkparam>
<hkparam name="name">My Modifier Generator</hkparam>
<hkparam name="modifier">#4000</hkparam>
<hkparam name="generator">#776</hkparam>
</hkobject>

<hkobject class="BSTimerModifier" name="#4000" signature="0x9266ffac">
<hkparam name="variableBindingSet">#4001</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">My Timer Modifier</hkparam>
<hkparam name="enable">true</hkparam>
<hkparam name="alarmTimeSeconds">30.0</hkparam>
<hkparam name="alarmEvent">
<hkobject class="hkbEventProperty" name="alarmEvent" signature="0xdb38a15">
<hkparam name="id">-1</hkparam>
<hkparam name="payload">null</hkparam>
</hkobject>
</hkparam>
<hkparam name="resetAlarm">true</hkparam>
</hkobject>

<hkobject class="hkbClipGenerator" name="#776" signature="0xd4cc9f6">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">My Animation 1</hkparam>
<hkparam name="animationBundleName"/>
<hkparam name="animationName">Animations\My Animation Name.hkt</hkparam>
<hkparam name="triggers">null</hkparam>
<hkparam name="userPartitionMask">0</hkparam>
<hkparam name="cropStartAmountLocalTime">0.0</hkparam>
<hkparam name="cropEndAmountLocalTime">0.0</hkparam>
<hkparam name="startTime">0.0</hkparam>
<hkparam name="playbackSpeed">1.0</hkparam>
<hkparam name="enforcedDuration">0.0</hkparam>
<hkparam name="userControlledTimeFraction">0.0</hkparam>
<hkparam name="animationBindingIndex">65535</hkparam>
<hkparam name="mode">MODE_SINGLE_PLAY</hkparam>
<hkparam name="flags">0</hkparam>
</hkobject>



And now we have to deal with Userdata.

My understanding of Userdata is that it's a parameter in which the "user" can reference data which Havok will not touch or use
(the next piece of information contradicts this) and could be used by a game engine etc...
The annoying part of this is that certain HAVOK block type's need to have Userdata set to a certain value to function.

Based off of what i've seen, my assumption is that bethesda uses userdata values as another way to enable/disable objects. So using 1 should generally work.
The easiest way to figure out what the value should be is to unpack base game files and find a behavior file which
uses the block class. So if you have everything working, but for some reason a modifier or other object isn't functioning, it's probably because of the user data value.

Once your finished setting things up, your modifier will now "modify" the Clipgenerator using a single modifier. If it isn't obvious yet, some modifier's don't actually modify the animation.
They simply do some kind of extra behavior along side the animation.

Now on to multiple modifiers.
This won't be apart of the example.
To use multiple modifiers, you have to once again use another wrapper block.

The process here is roughly the same as above.
The previous hkbModifierGenerator's modifier parameter will reference this block's name rather than a specific modifier.
Reference each modifier's name attribute in this object's "modifiers" attribute.
When your done, you can now reference multiple modifiers instead of just one.

<hkobject class="hkbModifierList" name="#8771" signature="0xded564c">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">1</hkparam>
<hkparam name="name">Multi Modifier Wrapper</hkparam>
<hkparam name="enable">true</hkparam>
<hkparam name="modifiers" numelements="0">
</hkparam>
</hkobject>


Now we have all the object's we need to create a functioning behavior graph, but not everything is setup.
We still need to adjust the state's value's, define another event for our second state. Reference the transition array in one of our state's and finally, assign our condition to something.

You should know how to define event's now. Go ahead and define another event.
To reference a transition array, get the transition array's name attribute value and reference it in a state's (not your starting state) transitions parameter.
If you haven't already, create a wildcard transition for your state machine.
With the current object's you have, there's only one place you can reference a condition object, which is under your transition arrays. Pick one of the array's and reference it's name.
For now, set the condition to something like "My first Defined Variable" >= 0. This is so it doesn't get in the way.
Lastly, Our statemachine doesn't have any state's assigned to it. Find your state's name value's and add them under your hkbStateMachine's states parameter.

Your behavior file should now be ready to go, but you still need animations!

Here's the complete behavior file (I've reorganized to make things clearer):



  Hide contents

<?xml version="1.0" encoding="ASCII" standalone="no"?>
<hkpackfile classversion="11" contentsversion="hk_2014.1.0-r1">
<hksection name="__data__">



<hkobject class="hkRootLevelContainer" name="#90" signature="0x2772c11e">
<hkparam name="namedVariants" numelements="1">
<hkobject>
<hkparam name="name">hkbBehaviorGraph</hkparam>
<hkparam name="className">hkbBehaviorGraph</hkparam>
<hkparam name="variant">#91</hkparam>
</hkobject>
</hkparam>
</hkobject>


<hkobject class="hkbBehaviorGraph" name="#91" signature="0xfdedb83b">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">My Behavior Name.hkb</hkparam>
<hkparam name="variableMode">VARIABLE_MODE_DISCARD_WHEN_INACTIVE</hkparam>
<hkparam name="rootGenerator">#92</hkparam>
<hkparam name="data">#106</hkparam>
</hkobject>


<hkobject class="hkbStateMachine" name="#92" signature="0xa5896bcf">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">RootStateMachine</hkparam>
<hkparam name="eventToSendWhenStateOrTransitionChanges">
<hkobject class="hkbEvent" name="eventToSendWhenStateOrTransitionChanges" signature="0x3e0fd810">
<hkparam name="id">-1</hkparam>
<hkparam name="payload">null</hkparam>
</hkobject>
</hkparam>
<hkparam name="startStateIdSelector">null</hkparam>
<hkparam name="startStateId">0</hkparam>
<hkparam name="returnToPreviousStateEventId">-1</hkparam>
<hkparam name="randomTransitionEventId">-1</hkparam>
<hkparam name="transitionToNextHigherStateEventId">-1</hkparam>
<hkparam name="transitionToNextLowerStateEventId">-1</hkparam>
<hkparam name="syncVariableIndex">-1</hkparam>
<hkparam name="wrapAroundStateId">false</hkparam>
<hkparam name="maxSimultaneousTransitions">32</hkparam>
<hkparam name="startStateMode">START_STATE_MODE_DEFAULT</hkparam>
<hkparam name="selfTransitionMode">SELF_TRANSITION_MODE_NO_TRANSITION</hkparam>
<hkparam name="states" numelements="2">
#8888
#9999
</hkparam>
<hkparam name="wildcardTransitions">#88888</hkparam>
</hkobject>
<hkobject class="hkbStateMachineTransitionInfoArray" name="#88888" signature="0x704a19af">
<hkparam name="transitions" numelements="1">
<hkobject>
<hkparam name="triggerInterval">
<hkobject class="hkbStateMachineTimeInterval" name="triggerInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<hkparam name="initiateInterval">
<hkobject class="hkbStateMachineTimeInterval" name="initiateInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<hkparam name="transition">null</hkparam>
<hkparam name="condition">null</hkparam>
<hkparam name="eventId">0</hkparam>
<hkparam name="toStateId">0</hkparam>
<hkparam name="fromNestedStateId">0</hkparam>
<hkparam name="toNestedStateId">0</hkparam>
<hkparam name="priority">0</hkparam>
<hkparam name="flags">FLAG_IS_LOCAL_WILDCARD</hkparam>
</hkobject>
</hkparam>
</hkobject>

<hkobject class="hkbStateMachineStateInfo" name="#8888" signature="0x39d76713">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="listeners" numelements="0"/>
<hkparam name="enterNotifyEvents">null</hkparam>
<hkparam name="exitNotifyEvents">null</hkparam>
<hkparam name="transitions">#111</hkparam>
<hkparam name="generator">#7665</hkparam>
<hkparam name="name">My Starting State</hkparam>
<hkparam name="stateId">0</hkparam>
<hkparam name="probability">1.0</hkparam>
<hkparam name="enable">true</hkparam>
</hkobject>
<hkobject class="hkbStateMachineTransitionInfoArray" name="#111" signature="0x704a19af">
<hkparam name="transitions" numelements="1">
<hkobject>
<hkparam name="triggerInterval">
<hkobject class="hkbStateMachineTimeInterval" name="triggerInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<hkparam name="initiateInterval">
<hkobject class="hkbStateMachineTimeInterval" name="initiateInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<hkparam name="transition">null</hkparam>
<hkparam name="condition">#1555</hkparam>
<hkparam name="eventId">1</hkparam>
<hkparam name="toStateId">1</hkparam>
<hkparam name="fromNestedStateId">0</hkparam>
<hkparam name="toNestedStateId">0</hkparam>
<hkparam name="priority">0</hkparam>
<hkparam name="flags">0</hkparam>
</hkobject>
</hkparam>
</hkobject>
<hkobject class="hkbExpressionCondition" name="#1555" signature="0x4a3ac449">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">My_Condition</hkparam>
<hkparam name="enable">true</hkparam>
<hkparam name="expression">My_First_Variable >= 0.0</hkparam>
</hkobject>
<hkobject class="hkbModifierGenerator" name="#7665" signature="0xc499fc9e">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">1</hkparam>
<hkparam name="name">My Modifier Generator</hkparam>
<hkparam name="modifier">#4000</hkparam>
<hkparam name="generator">#776</hkparam>
</hkobject>
<hkobject class="BSTimerModifier" name="#4000" signature="0x9266ffac">
<hkparam name="variableBindingSet">#4001</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">My Timer Modifier</hkparam>
<hkparam name="enable">true</hkparam>
<hkparam name="alarmTimeSeconds">30.0</hkparam>
<hkparam name="alarmEvent">
<hkobject class="hkbEventProperty" name="alarmEvent" signature="0xdb38a15">
<hkparam name="id">-1</hkparam>
<hkparam name="payload">null</hkparam>
</hkobject>
</hkparam>
<hkparam name="resetAlarm">true</hkparam>
</hkobject>
<hkobject class="hkbVariableBindingSet" name="#4001" signature="0xe942f339">
<hkparam name="bindings" numelements="1">
<hkobject>
<hkparam name="memberPath">alarmTimeSeconds</hkparam>
<hkparam name="variableIndex">0</hkparam>
<hkparam name="bitIndex">255</hkparam>
<hkparam name="bindingType">BINDING_TYPE_VARIABLE</hkparam>
</hkobject>

</hkparam>
<hkparam name="indexOfBindingToEnable">-1</hkparam>
</hkobject>
<hkobject class="hkbClipGenerator" name="#776" signature="0xd4cc9f6">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">First Animation</hkparam>
<hkparam name="animationBundleName"/>
<hkparam name="animationName">Animations\My Animation Name.hkt</hkparam>
<hkparam name="triggers">null</hkparam>
<hkparam name="userPartitionMask">0</hkparam>
<hkparam name="cropStartAmountLocalTime">0.0</hkparam>
<hkparam name="cropEndAmountLocalTime">0.0</hkparam>
<hkparam name="startTime">0.0</hkparam>
<hkparam name="playbackSpeed">1.0</hkparam>
<hkparam name="enforcedDuration">0.0</hkparam>
<hkparam name="userControlledTimeFraction">0.0</hkparam>
<hkparam name="animationBindingIndex">65535</hkparam>
<hkparam name="mode">MODE_SINGLE_PLAY</hkparam>
<hkparam name="flags">0</hkparam>
</hkobject>

<hkobject class="hkbStateMachineStateInfo" name="#9999" signature="0x39d76713">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="listeners" numelements="0"/>
<hkparam name="enterNotifyEvents">null</hkparam>
<hkparam name="exitNotifyEvents">null</hkparam>
<hkparam name="transitions">null</hkparam>
<hkparam name="generator">#77699</hkparam>
<hkparam name="name">My Second State</hkparam>
<hkparam name="stateId">1</hkparam>
<hkparam name="probability">1.0</hkparam>
<hkparam name="enable">true</hkparam>
</hkobject>
<hkobject class="hkbClipGenerator" name="#77699" signature="0xd4cc9f6">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">Second Animation</hkparam>
<hkparam name="animationBundleName"/>
<hkparam name="animationName">Animations\My Animation Name.hkt</hkparam>
<hkparam name="triggers">null</hkparam>
<hkparam name="userPartitionMask">0</hkparam>
<hkparam name="cropStartAmountLocalTime">0.0</hkparam>
<hkparam name="cropEndAmountLocalTime">0.0</hkparam>
<hkparam name="startTime">0.0</hkparam>
<hkparam name="playbackSpeed">1.0</hkparam>
<hkparam name="enforcedDuration">0.0</hkparam>
<hkparam name="userControlledTimeFraction">0.0</hkparam>
<hkparam name="animationBindingIndex">65535</hkparam>
<hkparam name="mode">MODE_SINGLE_PLAY</hkparam>
<hkparam name="flags">0</hkparam>
</hkobject>


<hkobject class="hkbBehaviorGraphData" name="#106" signature="0x907a8222">
<hkparam name="attributeDefaults" numelements="0"/>
<hkparam name="variableInfos" numelements="1">
<hkobject>
<hkparam name="role">
<hkobject class="hkbRoleAttribute" name="role" signature="0xfecef669">
<hkparam name="role">ROLE_DEFAULT</hkparam>
<hkparam name="flags">FLAG_NONE</hkparam>
</hkobject>
</hkparam>
<hkparam name="type">VARIABLE_TYPE_REAL</hkparam>
</hkobject>
</hkparam>
<hkparam name="characterPropertyInfos" numelements="0"/>
<hkparam name="eventInfos" numelements="2">
<hkobject>
<hkparam name="flags">0</hkparam>
</hkobject>
<hkobject>
<hkparam name="flags">0</hkparam>
</hkobject>
</hkparam>
<hkparam name="variableBounds" numelements="0"/>
<hkparam name="variableInitialValues">#117</hkparam>
<hkparam name="stringData">#118</hkparam>
</hkobject>



<hkobject class="hkbVariableValueSet" name="#117" signature="0xeb5f7e25">
<hkparam name="wordVariableValues" numelements="1">
<hkobject>
<hkparam name="value">30</hkparam>
</hkobject>
</hkparam>
<hkparam name="quadVariableValues" numelements="0"/>
<hkparam name="variantVariableValues" numelements="0"/>
</hkobject>



<hkobject class="hkbBehaviorGraphStringData" name="#118" signature="0x1bd27f38">
<hkparam name="eventNames" numelements="2">
<hkcstring>Start State Event</hkcstring>
<hkcstring>Second State Event</hkcstring>
</hkparam>
<hkparam name="attributeNames" numelements="0"/>
<hkparam name="variableNames" numelements="1">
<hkcstring>My_First_Variable</hkcstring>
</hkparam>
<hkparam name="characterPropertyNames" numelements="0"/>
</hkobject>

</hksection>
</hkpackfile>



Animation Basics :

For every frame of an animation, each bone of the object which is being animated is in a pose.

A bone, when not animated or previously animated, is in an starting pose (these are defined in the skeleton file).

A pose is simply the position, rotation and scale of the bone ( or nif node if that helps).

Keyframes are defined poses.

When you define keyframe's, your basically saying you want the bone to be at a position, rotation and scale at a certain time.

During an animation, a portion of a bone's pose's are generated so that it appears to be moving towards the defined keyframe pose.

As you can imagine, if your at a constant 60 frames a second and you had a 1 second duration animation with a single keyframe at the 1 second mark and you hadn't previously run an animation, 58 difference poses would be generated and 2 would be defined (starting pose and keyframe pose). Your frames aren't always constant of course, but I wont get into that.

The process of generating poses is called tweening or inbetweening if that's clearer.

When using modifier's that affect an animation, you are modifying specific bone pose's which could be poses that were inbetweened or keyframe poses (difference being one pose was generated and another was defined), based off of the modifier's parameters.

 


Creating A simple Interleaved Havok animation :

Before you look into this I'd like to suggest a suggestion. Bethesda themselves still used nif animation's in fallout 4.
So I assume they felt that nif animation's are still useful, have a place and shouldn't be completely replaced by the HAVOK animation system.
Try to keep this in mind when your considering how to animate objects.
You'll find that creating nif animation's requires a lot less work than HAVOK animations (depending on the animation of course).
There's also no need to setup a HKX file set just to create an animation if you animate in your nif.


  Hide contents

There are two relatively simple Havok animation types which can easily be edited.
hkaLosslessCompressedAnimation and hkaInterleavedUncompressedAnimation.
I won't go over hkaLosslessCompressedAnimation as it's a bit more difficult to manually create data for than hkaInterleavedUncompressedAnimation and there's still a lot I have yet to figure out about. But don't worry, there's no important difference between them when it comes to creating simple HAVOK animations (I will include animation data for what I consider a complicated animation soon). It's also important to note that this havok animation type has NO COMPRESSION. What this means is that you want to keep your animation's simple as possible. They are not very memory efficient. This should not be an issue for animation's which involve few bones, but If you try to create a complex animation (like animating a character) I do not know how it will effect the game. It could crash or simply use more memory. Don't let this restrict you from using them though. In the situation in which you create fairly large animations, just be certain you mention that the animation are uncompressed on your mod page so your users know that it could affect there game in a negative way.

Here's the basic structure.

<!-- Paste this in your animation xml file. -->
<?xml version="1.0" encoding="ASCII" standalone="no"?>
<hkpackfile classversion="11" contentsversion="hk_2014.1.0-r1">
<hksection name="__data__">
<hkobject class="hkRootLevelContainer" name="#90" signature="0x2772c11e">
<hkparam name="namedVariants" numelements="2">
<hkobject>
<hkparam name="name">Merged Animation Container</hkparam>
<hkparam name="className">hkaAnimationContainer</hkparam>
<hkparam name="variant">#91</hkparam>
</hkobject>
<hkobject>
<hkparam name="name">Resource Data</hkparam>
<hkparam name="className">hkMemoryResourceContainer</hkparam>
<hkparam name="variant">#94</hkparam>
</hkobject>
</hkparam>
</hkobject>
<hkobject class="hkaAnimationContainer" name="#91" signature="0x26859f4c">
<hkparam name="skeletons" numelements="0">
</hkparam>
<hkparam name="animations" numelements="1">
#92
</hkparam>
<hkparam name="bindings" numelements="1">
#93
</hkparam>
<hkparam name="attachments" numelements="0">
</hkparam>
<hkparam name="skins" numelements="0">
</hkparam>
</hkobject>
<!-- keeping it simple, numberOfTransformTracks is how many bones you are animating. It needs to match the amount of bones referenced in your skeleton file
The amount of annotation tracks is equal to the amount of bones being animated.
This object represents one animation track.
<hkobject>
<hkparam name="trackName"/>
<hkparam name="annotations" numelements="1">
</hkparam>
</hkobject>

This would represent 1 annotation for previously mentioned annotation track.
<hkobject>
<hkparam name="trackName"/>
<hkparam name="annotations" numelements="1">
<hkobject>
<hkparam name="time">0.13333334028720856</hkparam>
<hkparam name="text">weaponSwing</hkparam>
</hkobject>
</hkparam>
</hkobject>
Each bone has an annotation track.

-->
<hkobject class="hkaInterleavedUncompressedAnimation" name="#92" signature="0x278bffe8">
<hkparam name="type">HK_INTERLEAVED_ANIMATION</hkparam>
<hkparam name="duration">10.0</hkparam>
<hkparam name="numberOfTransformTracks">3</hkparam>
<hkparam name="numberOfFloatTracks">0</hkparam>
<hkparam name="extractedMotion">null</hkparam>
<hkparam name="annotationTracks" numelements="3">
<hkobject>
<hkparam name="trackName"/>
<hkparam name="annotations" numelements="0"/>
</hkobject>
<hkobject>
<hkparam name="trackName"/>
<hkparam name="annotations" numelements="0"/>
</hkobject>
<hkobject>
<hkparam name="trackName"/>
<hkparam name="annotations" numelements="0"/>
</hkobject>
</hkparam>

<!-- In general for all animation types, the structure's above this comment always exist. -->

<!-- Here's your animation. This is using the same format as the poses.
Translation vector, quaternion rotation vector, scale vector.
Here's a simple web tool which will convert Euler rotation's to quaternions.
https://quaternions.online/
If you need more than this you'll have to research the math and create a script to convert the values.
As a side note, I plan to incorporate this functionality in a tool which I'm working on.

Because I have 3 transforms tracks (animating 3 bones) I need 3 values for each "set".
My animation understanding isn't perfect so bare with me in the next part.
-->
<hkparam name="transforms" numelements="6">

<!--This is our first keyframe for each bone. -->
(0.0 0.0 1.0 0.0)(0.0 0.0 0.0 1.0)(1.0 1.0 1.0 0.0)
(0.0 0.0 1.0 0.0)(0.0 0.0 0.0 1.0)(1.0 1.0 1.0 0.0)
(0.0 0.0 1.0 0.0)(0.0 0.0 0.0 1.0)(1.0 1.0 1.0 0.0)

<!--This is our second keyframe for each bone. -->
(0.0 0.0 1.0 0.0)(0.0 0.0 0.0 1.0)(1.0 1.0 1.0 0.0)
(50.0 0.0 100.0 0.0)(0.0 0.0 0.0 1.0)(1.0 1.0 1.0 0.0)
(0.0 100.0 1.0 0.0)(0.0 0.0 0.0 1.0)(1.0 1.0 1.0 0.0)

<!--
With this setup, the bone's will animate from there starting pose's (or there last pose's) to the first keyframe values at 5.0 seconds and then to the second keyframe -
values at 10.0 seconds.
You can of course increase the amount of keyframes.
Make sure the amount of transforms is equal to amount of keyframes * bones.

The "Interleaved" part of the animation type name comes from the fact that the animation data for each bone and keyframe is combined. So instead of Bone 1: Kyfrme 1, Kyfrme 2

Kyfrme 3. It's Bone1 kyfrme 1, Bone 2 kyfrme 1, etc. According to the github information, this is a performance thing and I believe all the animation types follow this basic format.
-->

</hkparam>
<hkparam name="floats" numelements="0"/>
</hkobject>

<!--transformTrackToBoneIndices is how we define what values affect what bones.
For instance if I wanted the second bone transforms for the first keyframe to affect
the first bone instead of the second, I would right 1 0 2 instead of 0 1 2.
You always need to specify the values for this parameter.
OriginalSkeletonname reference's the skeleton file's name attribute value.
-->
<hkobject class="hkaAnimationBinding" name="#93" signature="0xfaf9150">
<hkparam name="originalSkeletonName">My skeleton's name (not file name)</hkparam>
<hkparam name="animation">#92</hkparam>
<hkparam name="transformTrackToBoneIndices" numelements="3">0 1 2</hkparam>
<hkparam name="floatTrackToFloatSlotIndices" numelements="0"/>
<hkparam name="partitionIndices" numelements="0"/>
<hkparam name="blendHint">NORMAL</hkparam>
</hkobject>
<hkobject class="hkMemoryResourceContainer" name="#94" signature="0x1de13a73">
<hkparam name="name"/>
<hkparam name="resourceHandles" numelements="0">
</hkparam>
<hkparam name="children" numelements="0">
</hkparam>
</hkobject>
</hksection>
</hkpackfile>

Set the values you wish for the keyframes.
If you want, you can create two different animation's for your states. Just make sure you update the Clipgenerator's animationName parameters.


 

 

Now that we've got all of our hkx files done, you can go ahead and pack them.
You'll likely find that you've made a mistake somewhere and can't get something to pack, so to help you out, I've packed all the examples to compare to your own.
They will be attached to this forum post.
If you still can't get something working, feel free to leave a comment with your HKX files attached and I'll try to help you figure out what's wrong.

Attaching the HKX data to a nif :

This next part assumes you have some Nifscope experience.
To attach this information to your model, load up the mesh in Nifscope and create a object of type BSBehaviorGraphExtraData which can be found under the Bethesda category.
Place it under your root node's Extra Data List. Then under the objects "Behavior Graph File" write "GenericBehaviors\"My Folder Name"\"My Project File Name".hkx". For my mesh this would be
"GenericBehaviors\ASMOD\Simple_Single_Event.hkx". Be sure to also include a BSXFlags object in your nif with animated, complex and dynamic checked.
I can't say what flags you truly need. But this is what a lot of base game nif's use.

Now your ready to go!

Load up your game, create your object which the HKX data is attached to.
When the object first loads, you'll see the animation which you defined for your starting state generator play.
By calling "PlayAnimation" using the appropriate event name, you can transition from your starting state to your other state and play that animation.
Then try passing a variable to the behavior graph using SetAnimationVariableFloat to adjust the timer duration.
If you get a error in your papyrus log for any of this, you've done something wrong.
Now you should know that when you call "PlayAnimation", you are simply sending an event to the behavior graph.


Extra Information :

Gamebryo Generators :

  Hide contents

BGSGamebryoSequenceGenerator's are how you play Gamebryo animation's from behavior files.
So if you wanted to play a gamebryoanimation on your nif object's instead of a Havok animation, you'd simply define this object instead of a Clipgenerator and reference it's name attribute.

This can be useful if you don't need havok animation's but still want to take advantage of the havok behavior file system.

parameters :
Name can be anything.
pSequence must be the actual nif animation name.
eBlendModeFunction and fPercent I have yet figure out. This is meant to be how you blend between Gamebryo animations but from my test's, the animation's -
never blend from their starting poses.
eUseTimePercentage allows you to position the object being animated based on what position
it would be in at a certain point in its animation. So 0.50 would position the object in whatever position it would be in
if the animation had been running for 50% of its duration. This option causes the animation to not run if enabled. In other words -
the "positioning" is the animation. To enable this you would specify USING_TIME_PERCENTAGE instead.

<hkobject class="BGSGamebryoSequenceGenerator" name="#110" signature="0x4e708fb6">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">Start State For HKX Animation</hkparam>
<hkparam name="pSequence">Start State For HKX</hkparam>
<hkparam name="eBlendModeFunction">BMF_NONE</hkparam>
<hkparam name="fPercent">1.0</hkparam>
<hkparam name="eUseTimePercentage">NOT_USING_TIME_PERCENTAGE</hkparam>
<hkparam name="fTimePercent">0.0</hkparam>
</hkobject>



HAVOK basic animation blending :

  Hide contents

The most simple HAVOK animation blending is done by implementing the hkbBlendingTransitionEffect Object.

<hkobject class="hkbBlendingTransitionEffect" name="#6698" signature="0x14e54c5c">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">zeroDuration</hkparam>
<hkparam name="selfTransitionMode">SELF_TRANSITION_MODE_CONTINUE_IF_CYCLIC_BLEND_IF_ACYCLIC</hkparam>
<hkparam name="eventMode">EVENT_MODE_DEFAULT</hkparam>
<hkparam name="duration">10.0</hkparam>
<hkparam name="toGeneratorStartTimeFraction">0.0</hkparam>
<hkparam name="flags">FLAG_NONE</hkparam>
<hkparam name="endMode">END_MODE_NONE</hkparam>
<hkparam name="blendCurve">0</hkparam>
<hkparam name="alignmentBone">65535</hkparam>
</hkobject>

This object must be referenced in a transition array's transition parameter.

<hkobject class="hkbStateMachineTransitionInfoArray" name="#7709" signature="0x704a19af">
<hkparam name="transitions" numelements="1">
<hkobject>
<hkparam name="triggerInterval">
<hkobject class="hkbStateMachineTimeInterval" name="triggerInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<hkparam name="initiateInterval">
<hkobject class="hkbStateMachineTimeInterval" name="initiateInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<hkparam name="transition">#6698</hkparam>
<hkparam name="condition">null</hkparam>
<hkparam name="eventId">1</hkparam>
<hkparam name="toStateId">1</hkparam>
<hkparam name="fromNestedStateId">0</hkparam>
<hkparam name="toNestedStateId">0</hkparam>
<hkparam name="priority">0</hkparam>
<hkparam name="flags">0</hkparam>
</hkobject>
</hkparam>
</hkobject>

When you leave the state, the blendingtransitioneffect will be used to blend between the current pose to the state to transition to's first animation pose.

Here's a shortcut to the Github information.
https://github.com/Bewolf2/projectanarchy/blob/bec6e7270c9dc797a12d167d3e8942866065364e/Source/Behavior/Behavior/TransitionEffect/Blending/hkbBlendingTransitionEffect.h





There's a lot that I did not cover so I'd like to post some shortcuts to Github information on generators, behaviors and other useful stuff.

Behavior Information :
https://github.com/Bewolf2/projectanarchy/tree/bec6e7270c9dc797a12d167d3e8942866065364e/Source/Behavior/Behavior

Under previous link you can find the generators and modifiers available in the 2013 HAVOK system.
you can also look into other things such as transition effects, conditions and statemachines.
Keep in mind, this is from a older version of Havok, there's a good chance that a lot of this information is no longer true and definitely does not represent the features of the Havok system fallout 4 uses.

Here's a list of modifiers and generator's used in the base game:



  Hide contents


HAVOK Modifiers :
hkbDamping
hkbEvaluateExpression
hkbEventDriven
hkbEventsFromRange
hkbGetUp
hkbKeyframeBones
hkbMirror
hkbPoweredRagdollControls
hkbRigidBodyRagdollControls
hkbTimer
hkbGetHandleOnBone
hkbHandIkControls
hkbJiggler
hkbFootIkControls
hkbTwist

Bethesda Modifiers :
BSAssignVariablesModifier
BSDirectAtCapturePoseModifier
BSDirectAtModifier
BSEventEveryNEventsModifier
BSEventOnDeactivateModifier
BSEventOnFalseToTrueModifier
BSGetTimeStepModifier
BSInterpValueModifier
BSIsActiveModifier
BSLimbCycleModifier
BSLookAtCapturePoseModifier
BSLookAtModifier
BSModifyOnceModifier
BSPassByTargetTriggerModifier
BSRagdollContactListenerModifier
BSRandomAlarmModifier
BSReverseSpineTwistModifier
BSRootTwistModifier
BSSpeedSamplerModifier
BSTimerModifier
BSTweenerModifier
Generators :
"BGSGamebryoSequenceGenerator"
"BSBehaviorGraphSwapGenerator"
"BSBoneSwitchGenerator"
"BSBoneSwitchGeneratorBoneData"
"BSCyclicBlendTransitionGenerator"
"BSOffsetAnimationGenerator"
"BSiStateTaggingGenerator"
"DynamicAnimationTaggingGenerator"
"hkbBehaviorReferenceGenerator"
"hkbBlenderGenerator"
"hkbBlenderGeneratorChild"
"hkbClipGenerator"
"hkbGeneratorTransitionEffect"
"hkbLayerGenerator"
"hkbManualSelectorGenerator"
"hkbModifierGenerator"
"hkbPoseMatchingGenerator"

I might of missed a few. Even though these are the only objects used in the base game, there still could be more that are unknown or not used.





Tips :

  Hide contents

As a tip when testing, as long as there isn't some huge error in your HKX files, you can simply repack and reload a save to update HKX data.
This makes testing changes to your behavior and animation files easy.

When unpacking HKX files, there will be a lot of comments. In notepad++ you can remove these by highlighting "<!--", pressing ctrl f > Mark. Then select Bookmark line and press "Mark All".
Then go to Search > Bookmark > Remove bookmarked lines.
You could also write a simple script to do this for you.

I've written a simple batch file to unpack multiple HKX files for me. Here's the bat code :
for /R "C:\Users\ajs52\Desktop\HKX_Output" %%f in (*.hkx) do Java -jar C:\Users\ajs52\Desktop\hkxpack-cli.jar unpack %%f
pause

This will loop through the HKX file's which are located in a folder named "HKK_Output" and unpack them into the same location. The paths in this code will need to be adjusted.

If you desire exploring HAVOK file's, you will need this.





I don't think I could close out this guide without giving a shoutout to Dexesttp. He's the creator of HKXPack and none of this would have been possible without his tool. Thank you Dexesttp.

I'd also like to thank ChuckYufarley who helped me get over a slight bump when first learning how to create havok files.

Thanks for reading my attempt at a guide, I hope I've at least helped you acquire the knowledge necessary to start exploring HKX files.


At this point I'd like to dump my notes incase someone find's them useful. I've gone through and edited them so there coherent.
I can't guarantee everything is true as these notes span many months. Some of the information might even contradict what was previously mentioned. So take them with a grain of salt.

  Hide contents


HKX Files :

Process of creating hkx data :
Create a new directory under Fallout 4\Data\Meshes Called "GenericBehavior".
All behaviors like this that aren't referenced in the ck need to be under this folder.
Although "GenericBehaviors" isn't the only folder of its kind.
Create a folder within the "genericbehaviors" directory, name -
it whatever you want. This is where your hkx hiles will be.
At this point you have 2 options, create the data yourself (have a template copied somewhere) -
or duplicate a hkx file set yourself.
I'm going to use the duplicate method.
Using BAE, open fallout 4 "animations" BA2 and search for "stagesnoloops" (this is a simple hkx set).
Open a CMD window with admin privileges and paste "java -jar".
Next copy the path of the hkxpack jar file.
For me its C:\Users\ajs52\Desktop\hkxpack-cli.jar.
Paste this into cmd with a space between it and "java -jar".
Create another space and type "unpack".
Then get the path of "stagesnoloops" hkx file -
and paste it into cmd (with a space between unpack and the path).
Press enter, then repeat the process for the rest of the hkx files.
Now open up the "StagesNoLoops" xml file and the "Character" file.
Under "StagesNoLoops", change the path of "CharacterFileNames" -
(Will be Characters\StagesNoLoops.hkx) To Characters\Whatever your character file name is.
Save and open up the character xml file.
Under "behaviorFilename", repeat the same process.
Your now setup, all you have left to edit is the behavior graph.
When your done with the files, Open a CMD window with admin privileges.
Follow the first process except instead of pasting "unpack", paste "pack".
Also be sure to reference the xml files, instead of the hkx files.
Then place the files in the correct directory (Under "GenericBehaviors").

Notes :

OnAnimationEventUnregistered is recieved either when the player explicitely unregisters OR -
when the state which the animation we registered for is in is transitioned from.
This needs testing!

To recieve events within papyrus (without referencing events in annotations), you -
need to use data block which explicitly send events. An example would be the timer modifier.
Another example is the hkbStateMachine's "eventToSendWhenStateOrTransitionChanges".

Keep in mind that the starting state animation will play as soon as the model loads.
When building new objects, this is BEFORE you place the object.

When defining multiple "hkbEventDrivenModifier", all events can be disabled -
no matter their position in the modifider sequence. I'm unsure if this applies to -
enabling them!

Some sources Of Events : Annotations, Certain Modifiers within graph.

The transition's "Condition" entry can be any expression, it does not need to reference a block. Example -
if hi == 2, etc.

When using Variables, "variableBounds" entries are not neccessary!

My assumption about "UserData" is that it can be any variable. It's purpose
is to simply exist so that an engine can read it such as the creation engine.

Based off testing, it appears the characters files are always needed.
When Messing with hkx files, if you only want an animation to play when the object is first created, simply use a -
hkbStateMachineStateInfo without any transitions and have the starting state be its state ID.

The Wildcard Transition NEEDS the flag "2560".
I believe you can also replace 2560 with "FLAG_IS_LOCAL_WILDCARD".
This means flags can be either numbers or strings.

When an object with hkx data first loads, if it has an animation associated with its default state -
it will play that animation. But it will not move to another state.
Every "hkbStateMachineStateInfo" NEEDS some kind of generator!

Every state needs to have an event. The event name does not have to match the animation name.
When you call "PlayAnimation" papyrus function, -
you need to be sure the "asAnimation" argument is the event -
name and not the animation name.

The "EventID" of each "hkbStateMachineTransitionInfoArray" determines which state is transitioned to.
When "PlayAnimation" is called, its sends an animation event to the graph.
The animation graph goes to the current state, finds -
the transition that references the passed event -
and transitions to that state.
The "EventID" references the index of the event, not the name.

When references events in nifscope "textkeyextradata", if the event is associated with an -
state, a transition to that state will occur.

Only applies to gamebryo animation's:
If your "NiTextKeyExtraData" has an entry which has an empty value, you will crash.
To be more specific, you need a "start" and "end" annotation. You can also use a start annotation -
and an annotation that sends an event.


Behavior Graph Object Notes :
Issues : Not everything is known and some thing's may be false. All this can be fixed -
through testing!

<hkobject class="hkbBehaviorGraph" name="#91" signature="0xfdedb83b">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">Simple_Single_Event_Behavior.hkb</hkparam>
<hkparam name="variableMode">VARIABLE_MODE_DISCARD_WHEN_INACTIVE</hkparam>
<hkparam name="rootGenerator">#92</hkparam>
<hkparam name="data">#106</hkparam>
</hkobject>

Name : Needs to be the name of the root hkx file, using the "StagesNoLoops" hkx -
the path would be StagesNoLoops.hkb
Data : Points to "hkbBehaviorGraphData"
RootGenerator : Points to the root "hkbStateMachine", in other words, the starting "hkbStateMachine".


<hkobject class="hkbBehaviorGraphData" name="#106" signature="0x907a8222">
<hkparam name="attributeDefaults" numelements="0"/>
<hkparam name="variableInfos" numelements="0"/>
<hkparam name="characterPropertyInfos" numelements="0"/>
<hkparam name="eventInfos" numelements="4">
<hkobject>
<hkparam name="flags">0</hkparam>
</hkobject>
<hkobject>
<hkparam name="flags">0</hkparam>
</hkobject>
<hkobject>
<hkparam name="flags">0</hkparam>
</hkobject>
<hkobject>
<hkparam name="flags">0</hkparam>
</hkobject>
</hkparam>
<hkparam name="variableBounds" numelements="0"/>
<hkparam name="variableInitialValues">#107</hkparam>
<hkparam name="stringData">#108</hkparam>
</hkobject>

Each Animation needs to have an event. The name doesn't need to match the animation name.
Each Event needs 1 "EventInfos" and 1 "EventNames" Entry.
Each event does NOT need an animation.
This is how you're able to send events to the graph with annotations without -
stage transitions occuring. These events (with or without animation) can be registered in papyrus.
Be sure you reference the event name in the annotations and register -
function, not the animation name.
Variable bounds is used to restrict the values of variables.


<hkobject class="hkbBehaviorGraphStringData" name="#108" signature="0x1bd27f38">
<hkparam name="eventNames" numelements="4">
<hkcstring>Test1</hkcstring>
<hkcstring>Test2</hkcstring>
<hkcstring>Test4</hkcstring>
<hkcstring>Test4</hkcstring>
</hkparam>
<hkparam name="attributeNames" numelements="0"/>
<hkparam name="variableNames" numelements="0"/>
<hkparam name="characterPropertyNames" numelements="0"/>
</hkobject>

<hkobject class="hkbStateMachine" name="#92" signature="0xa5896bcf">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">RootStateMachine</hkparam>
<hkparam name="eventToSendWhenStateOrTransitionChanges">
<hkobject class="hkbEvent" name="eventToSendWhenStateOrTransitionChanges" signature="0x3e0fd810">
<hkparam name="id">-1</hkparam>
<hkparam name="payload">null</hkparam>
</hkobject>
</hkparam>
<hkparam name="startStateIdSelector">null</hkparam>
<hkparam name="startStateId">0</hkparam>
<hkparam name="returnToPreviousStateEventId">-1</hkparam>
<hkparam name="randomTransitionEventId">-1</hkparam>
<hkparam name="transitionToNextHigherStateEventId">-1</hkparam>
<hkparam name="transitionToNextLowerStateEventId">-1</hkparam>
<hkparam name="syncVariableIndex">-1</hkparam>
<hkparam name="wrapAroundStateId">false</hkparam>
<hkparam name="maxSimultaneousTransitions">32</hkparam>
<hkparam name="startStateMode">START_STATE_MODE_DEFAULT</hkparam>
<hkparam name="selfTransitionMode">SELF_TRANSITION_MODE_NO_TRANSITION</hkparam>
<hkparam name="states" numelements="0">

</hkparam>
<hkparam name="wildcardTransitions">#201</hkparam>
</hkobject>

eventToSendWhenStateOrTransitionChanges : My understanding of this is that when we -
transition to another state, we send an event which can trigger other behavior\transitions.
States : Reference the "hkbStateMachineStateInfo" ID's (#111 etc).
wildcardTransitions : This is an transition which any State can use if it does not -
have a transition of its own.



<hkobject class="hkbStateMachineTransitionInfoArray" name="#201" signature="0x704a19af">
<hkparam name="transitions" numelements="1">
<hkobject>
<hkparam name="triggerInterval">
<hkobject class="hkbStateMachineTimeInterval" name="triggerInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<hkparam name="initiateInterval">
<hkobject class="hkbStateMachineTimeInterval" name="initiateInterval" signature="0x60a881e5">
<hkparam name="enterEventId">-1</hkparam>
<hkparam name="exitEventId">-1</hkparam>
<hkparam name="enterTime">0.0</hkparam>
<hkparam name="exitTime">0.0</hkparam>
</hkobject>
</hkparam>
<hkparam name="transition">null</hkparam>
<hkparam name="condition">null</hkparam>
<hkparam name="eventId">1</hkparam>
<hkparam name="toStateId">0</hkparam>
<hkparam name="fromNestedStateId">0</hkparam>
<hkparam name="toNestedStateId">0</hkparam>
<hkparam name="priority">0</hkparam>
<hkparam name="flags">0</hkparam>
</hkobject>
</hkparam>
</hkobject>

transition : This can reference a Blending object like "hkbBlendingTransitionEffect". Note that there are blenders which are generators so -
those wont work here. Although this object is technically a generator.
EventID : This reference the event which will trigger this transition.
The ID's for the events referenced by name, so for instance 0 means the first event defined -
and 1 means the next event after.
ToStateID : The "hkbStateMachineStateInfo" ID or state to transition to.
hkbStateMachineTransitionInfoArray can have multiple transitions.

<hkobject class="hkbStateMachineStateInfo" name="#119" signature="0x39d76713">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="listeners" numelements="0"/>
<hkparam name="enterNotifyEvents">null</hkparam>
<hkparam name="exitNotifyEvents">null</hkparam>
<hkparam name="transitions">null</hkparam>
<hkparam name="generator">#122</hkparam>
<hkparam name="name">Faster Increase State</hkparam>
<hkparam name="stateId">1</hkparam>
<hkparam name="probability">1.0</hkparam>
<hkparam name="enable">true</hkparam>
</hkobject>

enterNotifyEvents : This sends an event when this state is entered.
exitNotifyEvents : This sends an event when we leave this state.
transitions : if transitions is null, it will use wildcard.
stateId : Make sure the state ID matches the index of the event which will call it.
WARNING : Not certain this is neccessary!

<hkobject class="hkbModifierGenerator" name="#122" signature="0xc499fc9e">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">Mod_Generator 24</hkparam>
<hkparam name="modifier">#124</hkparam>
<hkparam name="generator">#120</hkparam>
</hkobject>

hkbModifierGenerator allows you to do neat stuff with the generators.
An example would be using "hkbEventDrivenModifier" to listen for when a "hkbTimerModifier" -
timer ends.
To use this, you would reference it in place of a usual -
generator (BGSGamebryoSequenceGenerator etc), and then reference -
the generator.

<hkobject class="hkbModifierList" name="#124" signature="0xded564c">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">Mod List 1</hkparam>
<hkparam name="enable">true</hkparam>
<hkparam name="modifiers" numelements="0">
</hkparam>
</hkobject>
This allows you to specify multiple modifers to run.
You would reference this in the hkbModifierGenerator instead of a single modifier.

<hkobject class="BGSGamebryoSequenceGenerator" name="#130" signature="0x4e708fb6">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">Faster Decrease Animation</hkparam>
<hkparam name="pSequence">Faster Decrease</hkparam>
<hkparam name="eBlendModeFunction">BMF_NONE</hkparam>
<hkparam name="fPercent">1.0</hkparam>
<hkparam name="eUseTimePercentage">NOT_USING_TIME_PERCENTAGE</hkparam>
<hkparam name="fTimePercent">0.0</hkparam>
</hkobject>

eBlendModeFunction Values :
BMF_NONE
BMF_ONE_MINUS_PERCENT : i believe this is literally 1 - percent inputed as the value.
BMF_PERCENT
Blending seems bugged with this so this and fPercent are kinda pointless.

This is the generator which runs nifscope animations.
The "name" does not need to be the animation name, but the "pSequence" does.
eUseTimePercentage allows you to position the object being animated based on what position
it would be in at a certain point in its animation. So 0.50 would position the object in whatever position it would be in
if the animation had been running for 50% of its duration. This option causes the animation to not run if enabled. In other words -
the "positioning" is the animation.

<hkobject class="hkbExpressionCondition" name="#1555" signature="0x4a3ac449">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">Condition</hkparam>
<hkparam name="enable">true</hkparam>
<hkparam name="expression">Time_To_End == 0</hkparam>
</hkobject>

Used (as far as I know) in "Condition" Entries.
The expression cannot have "if".

<hkobject class="hkbEvaluateExpressionModifier" name="#1003" signature="0x4a3ac449">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">Determine If Pause</hkparam>
<hkparam name="enable">true</hkparam>
<hkparam name="expressions">#1004</hkparam>
</hkobject>
<hkobject class="hkbExpressionDataArray" name="#1004" signature="0x1ebfc6d7">
<hkparam name="expressionsData" numelements="1">
<hkobject>
<hkparam name="expression">Start State For HKX if Time_To_End != 0</hkparam>
<hkparam name="assignmentVariableIndex">-1</hkparam>
<hkparam name="assignmentEventIndex">-1</hkparam>
<hkparam name="eventMode">EVENT_MODE_SEND_ONCE</hkparam>
</hkobject>
</hkparam>
</hkobject>

hkbEvaluateExpressionModifier is how you set a value for a parameter of a block.
It supports condition checking and can send an event if said condition returns true/false .
To use the condition type (Example) : if My_Int == 0.
To use the condition with the event type : My_Event_To_Send If My_Int == 0.

<hkobject class="hkbEventDrivenModifier" name="#1005" signature="0x35ef356b">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">Event Mod Set Value</hkparam>
<hkparam name="enable">true</hkparam>
<hkparam name="modifier">#1010</hkparam>
<hkparam name="activateEventId">0</hkparam>
<hkparam name="deactivateEventId">1</hkparam>
<hkparam name="activeByDefault">false</hkparam>
</hkobject>

Allows you to listen for events, and if events are recieved, do some behavior.
Can be activated and deactivated when events are sent out (can be from anywhere).
All hkbEventDrivenModifier's in a sequence will be activated/deactivated if both are listening -
for an event and said event is sent out.


<hkobject class="hkbVariableBindingSet" name="#999" signature="0xe942f339">
<hkparam name="bindings" numelements="1">
<hkobject>
<hkparam name="memberPath">alarmTimeSeconds</hkparam>
<hkparam name="variableIndex">0</hkparam>
<hkparam name="bitIndex">255</hkparam>
<hkparam name="bindingType">BINDING_TYPE_VARIABLE</hkparam>
</hkobject>
</hkparam>
<hkparam name="indexOfBindingToEnable">-1</hkparam>
</hkobject>

This block is used to set the values of parameters based off of supplied variables.
Member path will be the name of the parameter except in cases where blocks are nested.
In these situations, you define the path of the parameter in the same way paths are assigned -
in windows. For example, if we wanted to set the value of "eventMode" under the -
hkbExpressionDataArray, we would type expressions\eventMode in the "memberPath" -
entry (You might use "/" instead, can't remember).


<hkobject class="BSRootTwistModifier" name="#5092" signature="0xcbc0b864">
<hkparam name="variableBindingSet">#6089</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">BSRootTwistModifier01</hkparam>
<hkparam name="enable">true</hkparam>
<hkparam name="rootTwistAngle">0.0</hkparam>
<hkparam name="twistType">ANGLE_DEGREES</hkparam>
</hkobject>
Twist an object only on the Z axis. Does not animate but you can blend to the twisted angle. the object will just jump to the angle specified.
I assume based off the name this twists the "root" bone of an object. I've only ever seen "ANGLE_DEGREES" being used.

<hkobject class="BSTimerModifier" name="#4000" signature="0x9266ffac">
<hkparam name="variableBindingSet">#4001</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">Accuracy Timer Modifier</hkparam>
<hkparam name="enable">true</hkparam>
<hkparam name="alarmTimeSeconds">30.0</hkparam>
<hkparam name="alarmEvent">
<hkobject class="hkbEventProperty" name="alarmEvent" signature="0xdb38a15">
<hkparam name="id">6</hkparam>
<hkparam name="payload">null</hkparam>
</hkobject>
</hkparam>
<hkparam name="resetAlarm">true</hkparam>
</hkobject>
Same as the other timer except 1 important thing, this timer can loop and continue indefinetely by setting resetAlarm to true.


<hkobject class="hkbGeneratorTransitionEffect" name="#101" signature="0x44208f07">
<hkparam name="variableBindingSet">null</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">GT_Equip_Transition</hkparam>
<hkparam name="selfTransitionMode">SELF_TRANSITION_MODE_BLEND</hkparam>
<hkparam name="eventMode">EVENT_MODE_DEFAULT</hkparam>
<hkparam name="transitionGenerator">#102</hkparam>
<hkparam name="blendInDuration">0.25</hkparam>
<hkparam name="blendOutDuration">0.10000000149011612</hkparam>
<hkparam name="syncToGeneratorStartTime">false</hkparam>
</hkobject>

This blends from a animation to an extra defined animation and then from that animation to the next states animation.
blendInDuration is to the extra animation.
blendOutDuration is to the next states animation.


<hkobject class="hkbBlendingTransitionEffect" name="#5045" signature="0x14e54c5c">
<hkparam name="variableBindingSet">#5021</hkparam>
<hkparam name="userData">0</hkparam>
<hkparam name="name">Needle Transition</hkparam>
<hkparam name="selfTransitionMode">SELF_TRANSITION_MODE_CONTINUE_IF_CYCLIC_BLEND_IF_ACYCLIC</hkparam>
<hkparam name="eventMode">EVENT_MODE_DEFAULT</hkparam>
<hkparam name="duration">0.0</hkparam>
<hkparam name="toGeneratorStartTimeFraction">0.0</hkparam>
<hkparam name="flags">FLAG_NONE</hkparam>
<hkparam name="endMode">END_MODE_NONE</hkparam>
<hkparam name="blendCurve">1</hkparam>
<hkparam name="alignmentBone">65535</hkparam>
</hkobject>

This blender seems very limited. In my testing the blend occured from the blend to animations object starting position to the blended from -
animations object position. It also didnt fully blend but instead a percentage of the way, even when increasing the duration. You -
also cant increase the speed of the blend. This was all tested using gamebryo animations and not hkx and used the rotate helper and
behaviors from the GenericBehaviors folder as examples/recreated these. All in all it appears that this is meant to be used with hkx -
animations and not gamebryo but to some extent can be used with gamebryo animation. My guess is hkbBlendingTransitionEffect is not -
completely hooked up with gamebryo animations. Additional behavior included most of the options not having an effect.
One of the interesting things that is different when using hkx vs gamebryo animation here is that with hkx the duration value determines -
how fast you blend to the next animation's object starting point. With gamebryo, that next animations starting point seems to be -
predefined and generalized, atleast thats how I percieve it.

<hkobject class="hkbTwistModifier" name="#8812" signature="0x98d623bc">
<hkparam name="variableBindingSet">#7708</hkparam>
<hkparam name="userData">1</hkparam>
<hkparam name="name">Twist Mod 2</hkparam>
<hkparam name="enable">false</hkparam>
<hkparam name="axisOfRotation">(0.0 1.0 0.0 0.0)</hkparam>
<hkparam name="twistAngle">0.0</hkparam>
<hkparam name="startBoneIndex">0</hkparam>
<hkparam name="endBoneIndex">0</hkparam>
<hkparam name="setAngleMethod">LINEAR</hkparam>
<hkparam name="rotationAxisCoordinates">ROTATION_AXIS_IN_LOCAL_COORDINATES</hkparam>
<hkparam name="isAdditive">false</hkparam>
</hkobject>

userData must be 1.
If desired is able to manipulate just 1 bone.
Doesn't Animate (atleast with my testing).
Some generator/modifer userdata values need to be a certain value. For instance I believe hkbTwistModifier needs 1. Just find an example and use it.
If you see a flash of the model when it first loads, your reference pose orientation is off from where the animation object starting position is.

Pose Values :
Translation is the first array, rotation the second, scale the third.
Translation value 1 - 3 is xyz axis. Value 4 doesnt seem to be anything.
Make sure the referencePose second vector (rotation) always atleast has the last value as 1 (1 is no rotation). This has to do with quanternions.

The DefaultRotateHelper script and scripts like it are basically nifs with only hkx and animation data. These "entities" are attached to objects and then manipulated and when doing so -
also manipulate the attached to objects. They seem to be attached to specific references placed in the world using the ck and
as such cant be found by looking through forms.

Blending if effectively creating an animation on demand that creates a transition between the current model position and the position -
of the model in the next animation which is to run. By this very concept, you can create animation simply by position an object -
in an animation one way, and then blending to another animation in which the object is positioned another way.

IDK what hkSimpleLocalFrame does.
hkSimpleLocalFrame needs atleast 4 seperate vector values. (0.0 0.0 0.0 0.0) x4.
It seems that if you have only a single bone in your skeleton, you can't adjust it position or rotation.
To have access to these properties you need atleast 1 parent bone and 1 child bone.

Some nifs can get bugged when using behavior graphs. The simpliest solution is to recreate the BGSBehaviorGraphExtraData and skeleton nodes from new.

CheckList for hkx Stuff :

hkbModifiers only work with hkbClipGenerators??? Hence the "hkb"? This also applies with some bethesda modifiers.
BSRootTwistModifier (Bethesda one) only works with hkbClipGenerators. The state you start in and transition to must use hkbClipGenerators or no animation.

Use must have a bone to animate, without one no animation can occur (wont crash).
You do not need any "parentIndices" for your clipgenerators (the variable name here is animationBindingIndex, use -1 for no value) -
or your skeleton. This might be because hkxpack fixes it for you when you pack.

I assume "parentIndeces" 65535 refers to the world? It might be that 65535 refers to some sort of root?? No value still works.
From what I can tell 65535 is always the first value set for "parentIndices" and so it's probably best to follow that lead.
All the numbers after possibly refer to bone indexes. If you only have 1 bone, you just need 65535.

numberOfTransformTracks must be greater than 0 or no animation. If it doesn't have a value the pack will fail.

Animation duration can be 0. Wont pack if non existent.

The directory for the hkx data and animations can be in Generic_Behaviors!

The hkx and animations files need to be in there expected named folders such as "Behaviors" and "CharacterAssets" folders.

originalSkeletonName in the animation files refers to the skeleton "name" string.

Character "Name" I assume refers to the file name, so set it to that.

When using blends with gamebryogenerator, make sure the generator animation your blending from and the one -
your blending to both animate the same node! You can't animate two different nodes and blend, atleast with gamebryo animations.

As far as I can tell, all blending behavior requires a skeleton or "rig".

Assigning a bone to the root node of a nif seems to break everything, avoid it. Except if the object is attached to something else?

retieved from http://www.gamesas.com/havok-animation-converter-t207307-50.html :
parentIndices is basically how you parent the bones your working with. For instance if you had 7 bones and you wanted the bone at index 5 -
to be parented to bone at index 7, you would write -1 -1 -1 -1 7 -1 -1. If you reference a bone at this section, it must either have a parent link -
or -1. If you only have 1 value that isnt 65535, you will crash.

It's safest to always define each bones parent connection as i've experienced crashing if I didn't.

Avoid using blenders with gamebryo animations, they seem buggy. From what I can tell, they were never properly implemented.

If you reference the root node as a bone, the nif will bug out if you dont attach the nif to another object.
This was tested using placeatme vs placeatnode with attach as true, so this issue might not occur if you play objects through -
the workshop build menu or other means.

From what I can tell with gamebryo animations, the skelton referencePose's aren't used, but not having them causes a crash.

When not using root node, if skeleton doesn't have a reference pose for each bone, mesh bugs out. Tested with hkbBlenderGenerator -
and using placeatme, no attaching.

If something is wrong with your behavior graph the creation kit preview window will crash will trying to open your model.

If objects have the same name, things will break. Everything needs a unique name!

If you see a flash of the model when it first loads, your reference pose orientation is off from where the animation object starting position is.