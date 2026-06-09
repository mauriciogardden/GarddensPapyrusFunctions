#pragma once

#include <shared_mutex>

#include "GarddenCombatFunctions.h"

#include "RE/A/Actor.h"
#include "RE/F/FunctionArguments.h"
#include "RE/Skyrim.h"
#include "RE/T/TESObjectREFR.h"

namespace GarddenPapyrus {

    extern RE::TESQuest* g_currentDialogueQuest;

    void SendLineSpokenEvent(RE::Actor* speaker, RE::TESForm* topicInfo, const std::string& text);
    void SendPlayerChoiceEvent(RE::Actor* player, RE::Actor* targetNPC, RE::TESForm* topicInfo,
                               const std::string& text);
    void SendLineEndEvent(RE::Actor* speaker, RE::TESForm* topicInfo, const std::string& text);

    void RegisterForSpecificLine(RE::StaticFunctionTag*, RE::TESForm* listenerForm, RE::TESForm* topicForm);
    void UnregisterForSpecificLine(RE::StaticFunctionTag*, RE::TESForm* listenerForm, RE::TESForm* topicForm);

    void RegisterForSpeaker(RE::StaticFunctionTag*, RE::TESForm* listener, RE::Actor* speaker);
    void UnregisterForSpeaker(RE::StaticFunctionTag*, RE::TESForm* listener, RE::Actor* speaker);

    void RegisterForQuestLines(RE::StaticFunctionTag*, RE::TESForm* listener, RE::TESQuest* quest);
    void UnregisterForQuestLines(RE::StaticFunctionTag*, RE::TESForm* listener, RE::TESQuest* quest);

    void SetFavorPoints(RE::StaticFunctionTag*, RE::TESForm* akForm, int akValue);
    int GetFavorPoints(RE::StaticFunctionTag*, RE::TESForm* akForm);

    void AddGlobalListener(RE::TESForm* form);
    void AddTopicListener(RE::FormID topicID, RE::TESForm* form);
    void AddSpeakerListener(RE::FormID speakerID, RE::TESForm* form);
    void AddQuestListener(RE::FormID questID, RE::TESForm* form);

    void AddAffectedListener(RE::TESForm* listener);
    void AddOnAffectedAggressorListener(RE::FormID aggressorID, RE::TESForm* listener);
    void AddOnAffectedVictimListener(RE::FormID victimID, RE::TESForm* listener);

    void ClearAllListeners();

    void SendOnAffectedEvent(RE::Actor* akAggressor, RE::Actor* akVictim, RE::TESForm* akSource,
                        GarddenCombat::CombatTagMask tags);

    void SetImmuneAV_Papyrus(RE::StaticFunctionTag*, RE::Actor* actor, std::vector<RE::BSFixedString> avNames, bool reflect, float percent);
    void ClearImmuneAV_Papyrus(RE::StaticFunctionTag*, RE::Actor* actor, std::vector<RE::BSFixedString> avNames);

    bool RegisterFuncs(RE::BSScript::IVirtualMachine* a_vm);

}