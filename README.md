# GarddensPapyrusFunctions
This is the source code of a Skyrim SKSE Plugin that adds new functions and events to papyrus.
Please note that it was created with help of AI, as I'm no dev.

Here is the list of new functions it adds:


Combat:

#This stores the name of an actor value of an actor, to make it immune to changes made to the value (Currently not implemented)
Function SetImmuneAV(Actor akTarget, String[] avNames, Bool reflect = False, Float percent = 0.0) Global Native

#This clears actor value of an actor, that was set to be immune (Currently not implemented)
Function ClearImmuneAV(Actor akTarget, String[] avNames) Global Native

#The quest will receive all OnAffected calls (NOT RECOMMENDED) - OnAffected does not need to be registered on magic effects and works like 'OnHit'
Function RegisterForOnAffected(Form akListener) Global Native

#The quest will receive OnAffected calls created by the specific actor when it hits another
Function RegisterForOnAffectedAggressor(Form akListener, Actor akAggressor) Global Native

#The quest will receive OnAffected calls created by the specific actor when it is hit
Function RegisterForOnAffectedVictim(Form akListener, Actor akVictim) Global Native

#The quest will not receive OnAffected calls
Function UnregisterForOnAffected(Form akListener) Global Native

Event OnAffected(ObjectReference akAggressor, ObjectReference akVictim, Form akSource, int Tags)
#This works like 'OnHit', but concentration effects are only accounted for once every second, and you can get some information about the hit through the tags (below)   
EndEvent

Tags:
kDamage
kDoT
kDebuff
kControl
kMelee
kRanged
kSpell
kConcentration
kPowerAttack
kSneakAttack
kBashAttack
kBlocked
kFire
kFrost
kShock
kPoison
kTouch
kAimed
kTarget
kHostile
kNonHostile



Dialogue:

#The quest will listen for any line spoken on the dialogue menu
Function RegisterForLineSpoken(Form akListener) Global Native

#The quest will listen for a specif line spoken on the dialogue menu
Function RegisterForSpecificLine(Form akListener, Form akTopic) Global Native

#The quest will listen for all lines spoken by an specific actor on the dialogue menu
Function RegisterForSpeaker(Form akListener, Actor akSpeaker) Global Native

#The quest will listen for all lines of an quest spoken on the dialogue menu
Function RegisterForQuestLines(Form akListener, Quest akQuest) Global Native

Function UnregisterForLineSpoken(Form akListener) Global Native

Function UnregisterForSpecificLine(Form akListener, Form akTopic) Global Native

Function UnregisterForSpeaker(Form akListener, Actor akSpeaker) Global Native

Function UnregisterForQuestLines(Form akListener, Quest akQuest) Global Native

Event OnLineSpoken(ObjectReference akSpeaker, Form akTopicInfo, string akHexTopicInfo, int akDecTopicInfo, Form akTopicBranch, string akHexTopicBranch, int akDecTopicBranch, String akLine, int akFavorLevel)
    #The quest will receive this event everytime a line starts being spoken in the dialogue menu
    ; IMPORTANT: I recomend to use akDecTopicInfo instead of akTopicInfo to compare, as it's more reliable
EndEvent

Event OnPlayerChoice(ObjectReference akSpeaker, Form akTopicInfo, string akHexTopicInfo, int akDecTopicInfo, Form akTopicBranch, string akHexTopicBranch, int akDecTopicBranch, String akLine, int akFavorLevel)
    #The quest will receive this event everytime the player chooses an option on dialogue menu
    ; IMPORTANT: I recomend to use akDecTopicInfo instead of akTopicInfo to compare, as it's more reliable
EndEvent

Event OnLineEnd(ObjectReference akSpeaker, Form akTopicInfo, string akHexTopicInfo, int akDecTopicInfo, Form akTopicBranch, string akHexTopicBranch, int akDecTopicBranch, String akLine, int akFavorLevel)
    #The quest will receive this event everytime a line finishes in the dialogue menu
    ; IMPORTANT: I recomend to use akDecTopicInfo instead of akTopicInfo to compare, as it's more reliable
EndEvent

Function SetFavorPoints(Form akTopicInfo, int FavorLevel) Global Native
#Since favor points are unused in Skyrim, you can set and get it's value for lines to use as conditions. 
int Function GetFavorPoints(Form akTopicInfo) Global Native

#This function will copy the audio/subtitle of the NewTopic and overwrite ONLY  the audio/subtitle of the TopicToChange.
Function ReplaceTopicInfo(Form akNewTopic, Form akTopicToChange) Global Native




Itens and Objects:

#This will place a reference in a random location on the ground (terrain) around the target. You can also set other surfaces that the reference can be spawned in a formlist
ObjectReference Function PlaceOnGround(ObjectReference center, Form akForm, Float minRadius, Float maxRadius, Bool avoidWater, Bool requireOutOfSight, Float zOffset = 0.0, Float minClearDistance = 64.0, FormList validSurfaceList = None) global native
;Recommended:
;minRadius = 100
;maxRadius = 300
;zOffset = 0
;minClearDistance = 64

#This will place a copy of an object reference at the target, with the same size (scale) values as the original one
ObjectReference Function PlaceRefAtMe(ObjectReference akTarget, ObjectReference akSourceRef) global native

#Works like "AddItem", but specific for leveled lists and returns the number of itens that were added to the actor
Int Function GiveLeveledLoot(Actor akActor, LeveledItem akList) Global Native
