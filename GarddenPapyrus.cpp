#include <spdlog/sinks/basic_file_sink.h>

#include <algorithm>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace logger = SKSE::log;

#include "GarddenPapyrus.h"
#include "GarddenTopicFunctions.h"
#include "GarddenCombatFunctions.h"
#include "GarddenObjectFunctions.h"
#include "Persistence.h"
#include "RE/A/Actor.h"
#include "RE/B/BSFixedString.h"
#include "RE/F/FunctionArguments.h"
#include "RE/Skyrim.h"
#include "RE/T/TESObjectREFR.h"
#include "RE/T/TESForm.h"
#include "SKSE/SKSE.h"

namespace GarddenPapyrus {
    namespace logger = SKSE::log;

    // =========================
    // ON AFFECTED LISTENERS
    // =========================
    static std::vector<RE::TESForm*> g_onAffectedListeners;
    static std::shared_mutex g_onAffectedListenersMutex;

    static std::unordered_map<RE::FormID, std::vector<RE::TESForm*>> g_onAffectedAggressorListeners;
    static std::shared_mutex g_onAffectedAggressorMutex;

    static std::unordered_map<RE::FormID, std::vector<RE::TESForm*>> g_onAffectedVictimListeners;
    static std::shared_mutex g_onAffectedVictimMutex;

    
        // =========================
    // GLOBAL LISTENERS
    // =========================
    static std::vector<RE::TESForm*> g_listeners;
    static std::unordered_map<RE::FormID, std::vector<RE::TESForm*>> g_topicListeners;
    static std::unordered_map<RE::FormID, std::vector<RE::TESForm*>> g_speakerListeners;
    static std::unordered_map<RE::FormID, std::vector<RE::TESForm*>> g_questListeners;

    RE::TESQuest* g_currentDialogueQuest = nullptr;

    // =========================
    // MUTEXES
    // =========================
    static std::shared_mutex g_listenersMutex;
    static std::shared_mutex g_topicListenersMutex;
    static std::shared_mutex g_speakerListenersMutex;
    static std::shared_mutex g_questListenersMutex;
    static std::mutex g_topicInfoMutex;

    // =========================
    // ADD LISTENERS
    // =========================
    void AddGlobalListener(RE::TESForm* form) {
        if (!form) return;

        std::unique_lock lock(g_listenersMutex);

        if (std::find(g_listeners.begin(), g_listeners.end(), form) == g_listeners.end()) {
            g_listeners.push_back(form);
        }
    }

    void AddTopicListener(RE::FormID topicID, RE::TESForm* form) {
        if (!form) return;

        std::unique_lock lock(g_topicListenersMutex);

        auto& vec = g_topicListeners[topicID];
        if (std::find(vec.begin(), vec.end(), form) == vec.end()) {
            vec.push_back(form);
        }
    }

    void AddSpeakerListener(RE::FormID speakerID, RE::TESForm* form) {
        if (!form) return;

        std::unique_lock lock(g_speakerListenersMutex);

        auto& vec = g_speakerListeners[speakerID];
        if (std::find(vec.begin(), vec.end(), form) == vec.end()) {
            vec.push_back(form);
        }
    }

    void AddQuestListener(RE::FormID questID, RE::TESForm* form) {
        if (!form) return;

        std::unique_lock lock(g_questListenersMutex);

        auto& vec = g_questListeners[questID];
        if (std::find(vec.begin(), vec.end(), form) == vec.end()) {
            vec.push_back(form);
        }
    }

    void AddAffectedListener(RE::TESForm* listener) {
        if (!listener) return;

        std::unique_lock lock(g_onAffectedListenersMutex);

        logger::info("AddAffectedListener: total BEFORE {}", g_onAffectedListeners.size());

        if (std::find(g_onAffectedListeners.begin(), g_onAffectedListeners.end(), listener) !=
            g_onAffectedListeners.end()) {
            logger::info("AddAffectedListener: listener {:08X} already registered", listener->GetFormID());
            return;
        }

        g_onAffectedListeners.push_back(listener);

        // Persistência
        Garddens::Persistence::AddOnAffectedListener(listener->GetFormID());

        logger::info("AddAffectedListener: listener {:08X} successfully added", listener->GetFormID());
        logger::info("AddAffectedListener: total AFTER {}", g_onAffectedListeners.size());
    }

    void RemoveAffectedListener(RE::TESForm* listener) {
        if (!listener) return;

        std::unique_lock lock(g_onAffectedListenersMutex);

        auto it = std::remove(g_onAffectedListeners.begin(), g_onAffectedListeners.end(), listener);
        if (it == g_onAffectedListeners.end()) {
            logger::info("RemoveAffectedListener: listener {:08X} not found", listener->GetFormID());
            return;
        }

        g_onAffectedListeners.erase(it, g_onAffectedListeners.end());

        Garddens::Persistence::RemoveOnAffectedListener(listener->GetFormID());

        logger::info("RemoveAffectedListener: listener {:08X} removed", listener->GetFormID());
    }

        void AddOnAffectedAggressorListener(RE::FormID aggressorID, RE::TESForm* listener) {
        if (!listener) return;

        std::unique_lock lock(g_onAffectedAggressorMutex);

        auto& vec = g_onAffectedAggressorListeners[aggressorID];

        if (std::find(vec.begin(), vec.end(), listener) != vec.end()) {
            logger::info("AddOnAffectedAggressorListener: already registered {:08X} -> {:08X}", listener->GetFormID(),
                         aggressorID);
            return;
        }

        vec.push_back(listener);

        Garddens::Persistence::AddListener(listener->GetFormID(), aggressorID,
                                           Garddens::Persistence::kListenerTypeOnAffectedAggressor);

        logger::info("AddOnAffectedAggressorListener: {:08X} registered for aggressor {:08X}", listener->GetFormID(),
                     aggressorID);
    }

    void AddOnAffectedVictimListener(RE::FormID victimID, RE::TESForm* listener) {
        if (!listener) return;

        std::unique_lock lock(g_onAffectedVictimMutex);

        auto& vec = g_onAffectedVictimListeners[victimID];

        if (std::find(vec.begin(), vec.end(), listener) != vec.end()) {
            logger::info("AddOnAffectedVictimListener: already registered {:08X} -> {:08X}", listener->GetFormID(),
                         victimID);
            return;
        }

        vec.push_back(listener);

        Garddens::Persistence::AddListener(listener->GetFormID(), victimID,
                                           Garddens::Persistence::kListenerTypeOnAffectedVictim);

        logger::info("AddOnAffectedVictimListener: {:08X} registered for victim {:08X}", listener->GetFormID(),
                     victimID);
    }

    // =========================================================
    // REGISTER (Papyrus) - THREAD SAFE
    // =========================================================

    void RegisterForLineSpoken(RE::StaticFunctionTag*, RE::TESForm* listenerForm) {
        if (!listenerForm) {
            logger::warn("RegisterForLineSpoken: invalid listenerForm");
            return;
        }

        std::unique_lock lock(g_listenersMutex);

        if (std::find(g_listeners.begin(), g_listeners.end(), listenerForm) == g_listeners.end()) {
            g_listeners.push_back(listenerForm);
            Garddens::Persistence::AddListener(listenerForm->GetFormID(), 0, 0);
            logger::info("Listener registered! FormID: {:08X}", listenerForm->GetFormID());
        }
    }

    void RegisterForSpecificLine(RE::StaticFunctionTag*, RE::TESForm* listenerForm, RE::TESForm* topicForm) {
        if (!listenerForm || !topicForm) {
            logger::warn("RegisterForSpecificLine: invalid params");
            return;
        }

        std::unique_lock lock(g_topicListenersMutex);

        auto id = topicForm->GetFormID();
        auto& vec = g_topicListeners[id];

        if (std::find(vec.begin(), vec.end(), listenerForm) == vec.end()) {
            vec.push_back(listenerForm);
            Garddens::Persistence::AddListener(listenerForm->GetFormID(), id, 1);
            logger::info("Registered specific listener {:08X} for topic {:08X}", listenerForm->GetFormID(), id);
        }
    }

    void RegisterForSpeaker(RE::StaticFunctionTag*, RE::TESForm* listener, RE::Actor* speaker) {
        if (!listener || !speaker) return;

        std::unique_lock lock(g_speakerListenersMutex);

        auto id = speaker->GetFormID();
        auto& vec = g_speakerListeners[id];

        if (std::find(vec.begin(), vec.end(), listener) == vec.end()) {
            vec.push_back(listener);
            Garddens::Persistence::AddListener(listener->GetFormID(), id, 2);
            logger::info("Listener {:08X} registered for speaker {:08X}", listener->GetFormID(), id);
        }
    }

    void RegisterForQuestLines(RE::StaticFunctionTag*, RE::TESForm* listener, RE::TESQuest* quest) {
        if (!listener || !quest) return;

        std::unique_lock lock(g_questListenersMutex);

        auto id = quest->GetFormID();
        auto& vec = g_questListeners[id];

        if (std::find(vec.begin(), vec.end(), listener) == vec.end()) {
            vec.push_back(listener);
            Garddens::Persistence::AddListener(listener->GetFormID(), id, 3);
            logger::info("Listener {:08X} registered for quest lines {:08X}", listener->GetFormID(), id);
        }
    }

    void RegisterForOnAffected(RE::StaticFunctionTag*, RE::TESForm* listenerForm) {
        if (!listenerForm) return;

        std::unique_lock lock(g_onAffectedListenersMutex);

        if (std::find(g_onAffectedListeners.begin(), g_onAffectedListeners.end(), listenerForm) !=
            g_onAffectedListeners.end()) {
            logger::info("RegisterForOnAffected: listener {:08X} already registered (runtime)", listenerForm->GetFormID());
            return;
        }

        g_onAffectedListeners.push_back(listenerForm);

        Garddens::Persistence::AddListener(listenerForm->GetFormID(), 0,
                                           Garddens::Persistence::kListenerTypeOnAffected);

        logger::info("RegisterForOnAffected: listener {:08X} successfully registered", listenerForm->GetFormID());
    }

    void RegisterForOnAffectedAggressor(RE::StaticFunctionTag*, RE::TESForm* listener, RE::Actor* aggressor) {
        if (!listener || !aggressor) return;

        std::unique_lock lock(g_onAffectedAggressorMutex);

        auto id = aggressor->GetFormID();
        auto& vec = g_onAffectedAggressorListeners[id];

        if (std::find(vec.begin(), vec.end(), listener) == vec.end()) {
            vec.push_back(listener);

            Garddens::Persistence::AddListener(listener->GetFormID(), id,
                                               Garddens::Persistence::kListenerTypeOnAffectedAggressor);  // novo tipo
            logger::info("Registered OnAffectedAggressor {:08X} for {:08X}", listener->GetFormID(), id);
        }
    }

    void RegisterForOnAffectedVictim(RE::StaticFunctionTag*, RE::TESForm* listener, RE::Actor* victim) {
        if (!listener || !victim) return;

        std::unique_lock lock(g_onAffectedVictimMutex);

        auto id = victim->GetFormID();
        auto& vec = g_onAffectedVictimListeners[id];

        if (std::find(vec.begin(), vec.end(), listener) == vec.end()) {
            vec.push_back(listener);

            Garddens::Persistence::AddListener(listener->GetFormID(), id,
                                               Garddens::Persistence::kListenerTypeOnAffectedVictim);
            logger::info("Registered OnAffectedVictim {:08X} for {:08X}", listener->GetFormID(), id);
        }
    }

    // =========================================================
    // UNREGISTER (Papyrus) - THREAD SAFE
    // =========================================================

    void UnregisterForLineSpoken(RE::StaticFunctionTag*, RE::TESForm* listenerForm) {
        if (!listenerForm) return;

        std::unique_lock lock(g_listenersMutex);

        auto it = std::remove(g_listeners.begin(), g_listeners.end(), listenerForm);
        if (it != g_listeners.end()) {
            g_listeners.erase(it, g_listeners.end());
            Garddens::Persistence::RemoveListener(listenerForm->GetFormID(), 0, 0);
            logger::info("Listener removed! FormID: {:08X}", listenerForm->GetFormID());
        }
    }

    void UnregisterForSpecificLine(RE::StaticFunctionTag*, RE::TESForm* listenerForm, RE::TESForm* topicForm) {
        if (!listenerForm || !topicForm) return;

        std::unique_lock lock(g_topicListenersMutex);

        auto id = topicForm->GetFormID();
        auto it = g_topicListeners.find(id);
        if (it == g_topicListeners.end()) return;

        auto& vec = it->second;
        auto removeIt = std::remove(vec.begin(), vec.end(), listenerForm);
        if (removeIt != vec.end()) {
            vec.erase(removeIt, vec.end());
            Garddens::Persistence::RemoveListener(listenerForm->GetFormID(), id, 1);
            logger::info("Unregistered specific listener {:08X} for topic {:08X}", listenerForm->GetFormID(), id);
        }

        if (vec.empty()) {
            g_topicListeners.erase(it);
        }
    }

    void UnregisterForSpeaker(RE::StaticFunctionTag*, RE::TESForm* listener, RE::Actor* speaker) {
        if (!listener || !speaker) return;

        std::unique_lock lock(g_speakerListenersMutex);

        auto id = speaker->GetFormID();
        auto it = g_speakerListeners.find(id);
        if (it == g_speakerListeners.end()) return;

        auto& vec = it->second;
        auto removeIt = std::remove(vec.begin(), vec.end(), listener);
        if (removeIt != vec.end()) {
            vec.erase(removeIt, vec.end());
            Garddens::Persistence::RemoveListener(listener->GetFormID(), id, 2);
            logger::info("Listener {:08X} unregistered from speaker {:08X}", listener->GetFormID(), id);
        }

        if (vec.empty()) {
            g_speakerListeners.erase(it);
        }
    }

    void UnregisterForQuestLines(RE::StaticFunctionTag*, RE::TESForm* listener, RE::TESQuest* quest) {
        if (!listener || !quest) return;

        std::unique_lock lock(g_questListenersMutex);

        auto id = quest->GetFormID();
        auto it = g_questListeners.find(id);
        if (it == g_questListeners.end()) return;

        auto& vec = it->second;
        auto removeIt = std::remove(vec.begin(), vec.end(), listener);
        if (removeIt != vec.end()) {
            vec.erase(removeIt, vec.end());
            Garddens::Persistence::RemoveListener(listener->GetFormID(), id, 3);
            logger::info("Listener {:08X} unregistered from quest {:08X}", listener->GetFormID(), id);
        }

        if (vec.empty()) {
            g_questListeners.erase(it);
        }
    }

    void UnregisterForOnAffected(RE::StaticFunctionTag*, RE::TESForm* listenerForm) {
        if (!listenerForm) return;

        std::unique_lock lock(g_onAffectedListenersMutex);

        auto it = std::remove(g_onAffectedListeners.begin(), g_onAffectedListeners.end(), listenerForm);
        if (it == g_onAffectedListeners.end()) {
            logger::info("UnregisterForOnAffected: listener {:08X} not found", listenerForm->GetFormID());
            return;
        }

        g_onAffectedListeners.erase(it, g_onAffectedListeners.end());

        Garddens::Persistence::RemoveListener(listenerForm->GetFormID(), 0, 4);

        logger::info("UnregisterForOnAffected: listener {:08X} removed", listenerForm->GetFormID());
    }

    void UnregisterForOnAffectedAggressor(RE::StaticFunctionTag*, RE::TESForm* listener, RE::Actor* aggressor) {
        if (!listener || !aggressor) return;

        std::unique_lock lock(g_onAffectedAggressorMutex);

        auto id = aggressor->GetFormID();
        auto it = g_onAffectedAggressorListeners.find(id);
        if (it == g_onAffectedAggressorListeners.end()) return;

        auto& vec = it->second;

        auto removeIt = std::remove(vec.begin(), vec.end(), listener);
        if (removeIt != vec.end()) {
            vec.erase(removeIt, vec.end());

            Garddens::Persistence::RemoveListener(listener->GetFormID(), id,
                                                  Garddens::Persistence::kListenerTypeOnAffectedAggressor);

            logger::info("Unregistered OnAffectedAggressor {:08X} for {:08X}", listener->GetFormID(), id);
        }

        if (vec.empty()) {
            g_onAffectedAggressorListeners.erase(it);
        }
    }

    void UnregisterForOnAffectedVictim(RE::StaticFunctionTag*, RE::TESForm* listener, RE::Actor* victim) {
        if (!listener || !victim) return;

        std::unique_lock lock(g_onAffectedVictimMutex);

        auto id = victim->GetFormID();
        auto it = g_onAffectedVictimListeners.find(id);
        if (it == g_onAffectedVictimListeners.end()) return;

        auto& vec = it->second;

        auto removeIt = std::remove(vec.begin(), vec.end(), listener);
        if (removeIt != vec.end()) {
            vec.erase(removeIt, vec.end());

            Garddens::Persistence::RemoveListener(listener->GetFormID(), id,
                                                  Garddens::Persistence::kListenerTypeOnAffectedVictim);

            logger::info("Unregistered OnAffectedVictim {:08X} for {:08X}", listener->GetFormID(), id);
        }

        if (vec.empty()) {
            g_onAffectedVictimListeners.erase(it);
        }
    }

    // =========================================================
    // FAVOR LEVEL
    // =========================================================
    void SetFavorPoints(RE::StaticFunctionTag*, RE::TESForm* akForm, int akValue) {
        if (!akForm) return;

        auto topicInfo = akForm->As<RE::TESTopicInfo>();
        if (!topicInfo) {
            logger::warn("SetFavorPoints: akForm is not TESTopicInfo!");
            return;
        }

        // Clamp
        if (akValue < 0) akValue = 0;
        if (akValue > 3) akValue = 3;

        std::lock_guard<std::mutex> lock(g_topicInfoMutex);  // <-- proteção

        int current = static_cast<int>(topicInfo->favorLevel.get());
        if (current == akValue) {
            logger::info("SetFavorPoints: already set, skipping");
            return;
        }

        // APPLY
        topicInfo->favorLevel = static_cast<RE::TESTopicInfo::FavorLevel>(akValue);
        topicInfo->data.flags |= RE::TOPIC_INFO_DATA::TOPIC_INFO_FLAGS::kSpendsFavorPoints;

        logger::info("SetFavorPoints: Topic {:08X} now with FavorLevel {} and kSpendsFavorPoints flag activated",
                     topicInfo->GetFormID(), akValue);

        // PERSISTENCE
        Garddens::Persistence::AddFavor(topicInfo->GetFormID(), static_cast<std::uint8_t>(akValue));
    }

    int GetFavorPoints(RE::StaticFunctionTag*, RE::TESForm* akForm) {
        if (!akForm) return 0;

        auto topicInfo = akForm->As<RE::TESTopicInfo>();
        if (!topicInfo) {
            logger::warn("GetFavorPoints: akForm is not TESTopicInfo! FormID {:08X}", akForm->GetFormID());
            return 0;
        }

        std::lock_guard<std::mutex> lock(g_topicInfoMutex);  // <-- proteção

        int val = static_cast<int>(topicInfo->favorLevel.get());
        logger::info("GetFavorPoints: {}", val);
        return val;
    }

    // =========================================================
    // WRAPPERS
    // =========================================================

    // Wrapper for ReplaceTopicInfo
    void ReplaceTopicInfo(RE::StaticFunctionTag*, RE::TESForm* akSource, RE::TESForm* akTarget) {
        auto* source = akSource ? akSource->As<RE::TESTopicInfo>() : nullptr;
        auto* target = akTarget ? akTarget->As<RE::TESTopicInfo>() : nullptr;

        std::lock_guard<std::mutex> lock(g_topicInfoMutex);  // <-- proteção
        Garddens::TopicFunctions::ReplaceTopicInfo(source, target);
    }

    // Wrapper for PlaceStaticOnGround
    RE::TESObjectREFR* PlaceStaticOnGround_Papyrus(RE::StaticFunctionTag*, RE::TESObjectREFR* center, RE::TESForm* form,
                                                   float minRadius, float maxRadius, bool avoidWater,
                                                   bool requireOutOfSight, float zOffset, float minClearDistance,
                                                   RE::BGSListForm* validSurfaceList) {
        if (!center || !form) {
            logger::warn("PlaceStaticOnGround_Papyrus: invalid params");
            return nullptr;
        }

        return PlaceStaticOnGround(center, form, minRadius, maxRadius, avoidWater, requireOutOfSight, zOffset,
                                   minClearDistance, validSurfaceList);
    }

    // Wrapper for PlaceRefAtMe
    RE::TESObjectREFR* PlaceRefAtMe_Papyrus(RE::StaticFunctionTag*, RE::TESObjectREFR* target,
                                            RE::TESObjectREFR* templateRef) {
        return PlaceRefAtMe(target, templateRef);
    }

    // Wrapper for GiveLeveledLoot
    std::int32_t GiveLeveledLoot_Papyrus(RE::StaticFunctionTag*, RE::Actor* actor, RE::TESLevItem* lootList) {
        return GiveLeveledLoot(actor, lootList);
    }



    // Wrapper for SetImmuneAV
    void SetImmuneAV_Papyrus(RE::StaticFunctionTag*, RE::Actor* actor, std::vector<RE::BSFixedString> avNames,
                             bool reflect = false , float percent = 0.0f) {
        if (!actor) {
            logger::warn("SetImmuneAV_Papyrus: actor is null!");
            return;
        }

        if (avNames.empty()) {
            logger::warn("SetImmuneAV_Papyrus: no AVs provided!");
            return;
        }

        // Converter para std::string
        std::vector<std::string> avs;
        avs.reserve(avNames.size());
        for (const auto& bs : avNames) {
            if (!bs.empty()) avs.push_back(bs.c_str());
        }

        // Aplica o efeito
        SetImmuneAV(actor, avs, reflect, percent);                                      // runtime
        Garddens::Persistence::AddImmuneAV(actor->GetFormID(), avs, reflect, percent);  // persistência

        // Logging detalhado
        std::string listStr;
        for (auto& s : avs) listStr += s + " ";
        logger::info("SetImmuneAV called for actor {:08X} | AVs: {} | reflect: {} | percent: {}", actor->GetFormID(),
                     listStr, reflect, percent);
    }

    // Wrapper for ClearImmuneAV
    void ClearImmuneAV_Papyrus(RE::StaticFunctionTag*, RE::Actor* actor, std::vector<RE::BSFixedString> avNames) {
        if (!actor) return;

        std::vector<std::string> avs;
        avs.reserve(avNames.size());
        for (const auto& bs : avNames) {
            if (!bs.empty()) avs.push_back(bs.c_str());
        }

        ClearImmuneAV(actor, avs);  // runtime

        if (avs.empty()) {
            Garddens::Persistence::ClearImmuneAVs(actor->GetFormID());  // remove todos AVs persistidos
        } else {
            Garddens::Persistence::RemoveImmuneAV(actor->GetFormID(), avs);  // remove AVs específicos
        }
    }

    // Wrapper for GetImmuneAV
    void GetImmuneAV_Papyrus(RE::StaticFunctionTag*, RE::Actor* actor) { GetImmuneAV(actor); }

    // =========================
    // SEND EVENTS - THREAD SAFE
    // =========================

    void SendLineSpokenEvent(RE::Actor* speaker, RE::TESForm* topicInfo, const std::string& text) {
        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) return;
        auto policy = vm->GetObjectHandlePolicy();
        if (!policy) return;

        RE::TESObjectREFR* speakerRef = speaker ? speaker->AsReference() : nullptr;
        std::string topicIDStr = topicInfo ? std::format("{:08X}", topicInfo->GetFormID()) : "00000000";

        RE::TESTopic* topic = topicInfo ? topicInfo->As<RE::TESTopicInfo>()->parentTopic : nullptr;
        std::string topicHexStr = topic ? std::format("{:08X}", topic->GetFormID()) : "00000000";

        const char* npcName = "Unknown";
        if (speaker && speaker->GetActorBase() && speaker->GetActorBase()->GetName()[0] != '\0')
            npcName = speaker->GetActorBase()->GetName();

        int favorLevel = topicInfo ? static_cast<int>(static_cast<RE::TESTopicInfo*>(topicInfo)->favorLevel.get()) : 0;

        logger::info("[{}] Topic {} Branch {} Favor {} | \"{}\"", npcName, topicIDStr, topicHexStr, favorLevel, text);

        // =========================
        // ARGS PERMANECE INTACTO
        // =========================
        auto args = RE::MakeFunctionArguments(
            (RE::TESObjectREFR*)speakerRef, (RE::TESForm*)topicInfo, RE::BSFixedString(topicIDStr.c_str()),
            topicInfo ? topicInfo->GetFormID() : 0, (RE::TESForm*)topic, RE::BSFixedString(topicHexStr.c_str()),
            topic ? topic->GetFormID() : 0, RE::BSFixedString(text.c_str()), static_cast<int>(favorLevel));

        std::unordered_set<RE::TESForm*> sent;

        // =========================
        // COPIAR LISTAS THREAD-SAFE
        // =========================
        std::vector<RE::TESForm*> globalCopy, topicCopy, speakerCopy, questCopy;

        {
            std::shared_lock lock(g_listenersMutex);
            globalCopy = g_listeners;
        }
        if (topicInfo) {
            std::shared_lock lock(g_topicListenersMutex);
            auto it = g_topicListeners.find(topicInfo->GetFormID());
            if (it != g_topicListeners.end()) topicCopy = it->second;
        }
        if (speaker) {
            std::shared_lock lock(g_speakerListenersMutex);
            auto it = g_speakerListeners.find(speaker->GetFormID());
            if (it != g_speakerListeners.end()) speakerCopy = it->second;
        }
        if (g_currentDialogueQuest) {
            std::shared_lock lock(g_questListenersMutex);
            auto it = g_questListeners.find(g_currentDialogueQuest->GetFormID());
            if (it != g_questListeners.end()) questCopy = it->second;
        }

        // =========================
        // LIMPAR ELEMENTOS NULOS DAS COPIAS
        // =========================
        auto cleanVector = [](std::vector<RE::TESForm*>& vec) {
            vec.erase(std::remove(vec.begin(), vec.end(), nullptr), vec.end());
        };

        cleanVector(globalCopy);
        cleanVector(topicCopy);
        cleanVector(speakerCopy);
        cleanVector(questCopy);

        //logger::info("Sending 'OnLineSpoken' event for listeners -> global: {}, topic: {}, speaker: {}, quest: {}",
        //             globalCopy.size(), topicCopy.size(), speakerCopy.size(), questCopy.size());

        auto sendTo = [&](const std::vector<RE::TESForm*>& vec) {
            for (auto* form : vec) {
                if (!form) continue;
                RE::VMHandle handle = policy->GetHandleForObject(form->GetFormType(), form);
                if (!handle) continue;
                if (sent.insert(form).second) vm->SendEvent(handle, RE::BSFixedString("OnLineSpoken"), args);
            }
        };

        sendTo(globalCopy);
        sendTo(topicCopy);
        sendTo(speakerCopy);
        sendTo(questCopy);
    }

    void SendPlayerChoiceEvent(RE::Actor* player, RE::Actor* targetNPC, RE::TESForm* topicInfo,
                               const std::string& text) {
        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) return;
        auto policy = vm->GetObjectHandlePolicy();
        if (!policy) return;

        RE::TESObjectREFR* playerRef = player ? player->AsReference() : nullptr;
        std::string topicIDStr = topicInfo ? std::format("{:08X}", topicInfo->GetFormID()) : "00000000";
        RE::TESTopic* topic = topicInfo ? topicInfo->As<RE::TESTopicInfo>()->parentTopic : nullptr;
        std::string topicHexStr = topic ? std::format("{:08X}", topic->GetFormID()) : "00000000";

        std::string playerName = "Player";
        std::string npcName = "Unknown NPC";
        if (targetNPC && targetNPC->GetActorBase()) npcName = targetNPC->GetActorBase()->GetName();

        int favorLevel = topicInfo ? static_cast<int>(static_cast<RE::TESTopicInfo*>(topicInfo)->favorLevel.get()) : 0;

        logger::info("[{} -> {}] Topic {} Branch {} Favor {} | \"{}\"", playerName, npcName, topicIDStr, topicHexStr,
                     favorLevel, text);

        auto args = RE::MakeFunctionArguments(
            (RE::TESObjectREFR*)playerRef, (RE::TESForm*)topicInfo, RE::BSFixedString(topicIDStr.c_str()),
            topicInfo ? topicInfo->GetFormID() : 0, (RE::TESForm*)topic, RE::BSFixedString(topicHexStr.c_str()),
            topic ? topic->GetFormID() : 0, RE::BSFixedString(text.c_str()), static_cast<int>(favorLevel));

        std::unordered_set<RE::TESForm*> sent;

        // =========================
        // COPIAR LISTAS THREAD-SAFE
        // =========================
        std::vector<RE::TESForm*> globalCopy, topicCopy, speakerCopy, questCopy;

        {
            std::shared_lock lock(g_listenersMutex);
            globalCopy = g_listeners;
        }
        if (topicInfo) {
            std::shared_lock lock(g_topicListenersMutex);
            auto it = g_topicListeners.find(topicInfo->GetFormID());
            if (it != g_topicListeners.end()) topicCopy = it->second;
        }
        if (targetNPC) {
            std::shared_lock lock(g_speakerListenersMutex);
            auto it = g_speakerListeners.find(targetNPC->GetFormID());
            if (it != g_speakerListeners.end()) speakerCopy = it->second;
        }
        if (g_currentDialogueQuest) {
            std::shared_lock lock(g_questListenersMutex);
            auto it = g_questListeners.find(g_currentDialogueQuest->GetFormID());
            if (it != g_questListeners.end()) questCopy = it->second;
        }

        auto cleanVector = [](std::vector<RE::TESForm*>& vec) {
            vec.erase(std::remove(vec.begin(), vec.end(), nullptr), vec.end());
        };
        cleanVector(globalCopy);
        cleanVector(topicCopy);
        cleanVector(speakerCopy);
        cleanVector(questCopy);

        //logger::info("Sending 'OnPlayerChoice' event -> global: {}, topic: {}, speaker: {}, quest: {}",
        //             globalCopy.size(), topicCopy.size(), speakerCopy.size(), questCopy.size());

        auto sendTo = [&](const std::vector<RE::TESForm*>& vec) {
            for (auto* form : vec) {
                if (!form) continue;
                RE::VMHandle handle = policy->GetHandleForObject(form->GetFormType(), form);
                if (!handle) continue;
                if (sent.insert(form).second) vm->SendEvent(handle, RE::BSFixedString("OnPlayerChoice"), args);
            }
        };

        sendTo(globalCopy);
        sendTo(topicCopy);
        sendTo(speakerCopy);
        sendTo(questCopy);
    }

    void SendLineEndEvent(RE::Actor* speaker, RE::TESForm* topicInfo, const std::string& text) {
        if (!speaker || !topicInfo) return;

        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) return;
        auto policy = vm->GetObjectHandlePolicy();
        if (!policy) return;

        RE::TESObjectREFR* speakerRef = speaker->AsReference();
        std::string topicIDStr = std::format("{:08X}", topicInfo->GetFormID());
        RE::TESTopic* topic = topicInfo->As<RE::TESTopicInfo>()->parentTopic;
        std::string topicHexStr = topic ? std::format("{:08X}", topic->GetFormID()) : "00000000";

        int favorLevel = static_cast<int>(static_cast<RE::TESTopicInfo*>(topicInfo)->favorLevel.get());

        auto args = RE::MakeFunctionArguments(
            (RE::TESObjectREFR*)speakerRef, (RE::TESForm*)topicInfo, RE::BSFixedString(topicIDStr.c_str()),
            topicInfo->GetFormID(), (RE::TESForm*)topic, RE::BSFixedString(topicHexStr.c_str()),
            topic ? topic->GetFormID() : 0, RE::BSFixedString(text.c_str()), static_cast<int>(favorLevel));

        std::unordered_set<RE::TESForm*> sent;

        std::vector<RE::TESForm*> globalCopy, topicCopy, speakerCopy, questCopy;
        {
            std::shared_lock lock(g_listenersMutex);
            globalCopy = g_listeners;
        }
        {
            std::shared_lock lock(g_topicListenersMutex);
            auto it = g_topicListeners.find(topicInfo->GetFormID());
            if (it != g_topicListeners.end()) topicCopy = it->second;
        }
        {
            std::shared_lock lock(g_speakerListenersMutex);
            auto it = g_speakerListeners.find(speaker->GetFormID());
            if (it != g_speakerListeners.end()) {
                speakerCopy = it->second;
            }
        }
        if (g_currentDialogueQuest) {
            std::shared_lock lock(g_questListenersMutex);
            auto it = g_questListeners.find(g_currentDialogueQuest->GetFormID());
            if (it != g_questListeners.end()) questCopy = it->second;
        }

        auto cleanVector = [](std::vector<RE::TESForm*>& vec) {
            vec.erase(std::remove(vec.begin(), vec.end(), nullptr), vec.end());
        };
        cleanVector(globalCopy);
        cleanVector(topicCopy);
        cleanVector(speakerCopy);
        cleanVector(questCopy);

        auto sendTo = [&](const std::vector<RE::TESForm*>& vec, const char* eventName) {
            for (auto* form : vec) {
                if (!form) continue;
                RE::VMHandle handle = policy->GetHandleForObject(form->GetFormType(), form);
                if (!handle) continue;
                if (sent.insert(form).second) vm->SendEvent(handle, RE::BSFixedString(eventName), args);
            }
        };

        sendTo(globalCopy, "OnLineEnd");
        sendTo(topicCopy, "OnLineEnd");
        sendTo(speakerCopy, "OnLineEnd");
        sendTo(questCopy, "OnLineEnd");
    }

    

    void SendOnAffectedEvent(RE::Actor* akAggressor, RE::Actor* akVictim, RE::TESForm* akSource,
                             GarddenCombat::CombatTagMask tags) {
        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) return;

        auto policy = vm->GetObjectHandlePolicy();
        if (!policy) return;

        RE::TESObjectREFR* aggressorRef = akAggressor ? akAggressor->AsReference() : nullptr;
        RE::TESObjectREFR* victimRef = akVictim ? akVictim->AsReference() : nullptr;

        // =====================================
        // ARGUMENTOS (SIMPLIFICADO)
        // =====================================
        auto args = RE::MakeFunctionArguments((RE::TESObjectREFR*)aggressorRef, (RE::TESObjectREFR*)victimRef,
                                              (RE::TESForm*)akSource,
                                              static_cast<std::int32_t>(tags)  // 👈 aqui está a mágica
        );

        std::unordered_set<RE::TESForm*> sent;

        static RE::BSFixedString eventName = "OnAffected";

        if (akVictim) {
            RE::VMHandle handle = policy->GetHandleForObject(akVictim->GetFormType(), akVictim);

            if (handle) {
                sent.insert(akVictim);
                RE::SkyrimVM::GetSingleton()->SendAndRelayEvent(handle, &eventName, args, nullptr);
            }
        }

        // =========================
        // Thread-safe copy dos listeners
        // =========================
        std::unordered_set<RE::TESForm*> listenersSet;

        {
            std::shared_lock lock(g_onAffectedListenersMutex);
            listenersSet.insert(g_onAffectedListeners.begin(), g_onAffectedListeners.end());
        }

        {
            std::shared_lock lockA(g_onAffectedAggressorMutex);
            if (akAggressor) {
                auto it = g_onAffectedAggressorListeners.find(akAggressor->GetFormID());
                if (it != g_onAffectedAggressorListeners.end()) {
                    listenersSet.insert(it->second.begin(), it->second.end());
                }
            }
        }

        {
            std::shared_lock lockV(g_onAffectedVictimMutex);
            if (akVictim) {
                auto it = g_onAffectedVictimListeners.find(akVictim->GetFormID());
                if (it != g_onAffectedVictimListeners.end()) {
                    listenersSet.insert(it->second.begin(), it->second.end());
                }
            }
        }

        // =========================
        // Remove nulls
        // =========================
        //listenersCopy.erase(std::remove(listenersCopy.begin(), listenersCopy.end(), nullptr), listenersCopy.end());

        //logger::info("Sending 'OnAffected' event to {} listeners | tags {:016X}", listenersCopy.size(), tags);

        // =========================
        // DISPATCH
        // =========================
        

        for (auto* form : listenersSet) {
            if (!form) continue;

            RE::VMHandle handle = policy->GetHandleForObject(form->GetFormType(), form);
            if (!handle) continue;

            if (sent.insert(form).second) {
                RE::SkyrimVM::GetSingleton()->SendAndRelayEvent(handle, &eventName, args, nullptr);
            }
        }
    }



    void ClearAllListeners() {
        {
            std::unique_lock lock(g_listenersMutex);
            g_listeners.clear();
        }
        {
            std::unique_lock lock(g_topicListenersMutex);
            g_topicListeners.clear();
        }
        {
            std::unique_lock lock(g_speakerListenersMutex);
            g_speakerListeners.clear();
        }
        {
            std::unique_lock lock(g_questListenersMutex);
            g_questListeners.clear();
        }
        {
            std::unique_lock lock(g_onAffectedListenersMutex);
            g_onAffectedListeners.clear();
        }
        {
            std::unique_lock lock(g_onAffectedAggressorMutex);
            g_onAffectedAggressorListeners.clear();
        }
        {
            std::unique_lock lock(g_onAffectedVictimMutex);
            g_onAffectedVictimListeners.clear();
        }
    }


    // =========================================================
    // REGISTER PAPYRUS FUNCTIONS
    // =========================================================
    bool RegisterFuncs(RE::BSScript::IVirtualMachine* a_vm) {
        if (!a_vm) {
            logger::error("invalid VM Papyrus!");
            return false;
        }

        auto vm = reinterpret_cast<RE::BSScript::Internal::VirtualMachine*>(a_vm);

        vm->RegisterFunction("RegisterForLineSpoken", "GarddensPapyrusFunctions", RegisterForLineSpoken);
        vm->RegisterFunction("UnregisterForLineSpoken", "GarddensPapyrusFunctions", UnregisterForLineSpoken);

        vm->RegisterFunction("RegisterForSpecificLine", "GarddensPapyrusFunctions", RegisterForSpecificLine);
        vm->RegisterFunction("UnregisterForSpecificLine", "GarddensPapyrusFunctions", UnregisterForSpecificLine);

        vm->RegisterFunction("RegisterForSpeaker", "GarddensPapyrusFunctions", RegisterForSpeaker);
        vm->RegisterFunction("UnregisterForSpeaker", "GarddensPapyrusFunctions", UnregisterForSpeaker);

        vm->RegisterFunction("RegisterForQuestLines", "GarddensPapyrusFunctions", RegisterForQuestLines);
        vm->RegisterFunction("UnregisterForQuestLines", "GarddensPapyrusFunctions", UnregisterForQuestLines);

        vm->RegisterFunction("SetFavorPoints", "GarddensPapyrusFunctions", SetFavorPoints);
        vm->RegisterFunction("GetFavorPoints", "GarddensPapyrusFunctions", GetFavorPoints);

        vm->RegisterFunction("ReplaceTopicInfo", "GarddensPapyrusFunctions", ReplaceTopicInfo);

        vm->RegisterFunction("SetImmuneAV", "GarddensPapyrusFunctions", SetImmuneAV_Papyrus);
        vm->RegisterFunction("ClearImmuneAV", "GarddensPapyrusFunctions", ClearImmuneAV_Papyrus);
        vm->RegisterFunction("GetImmuneAV", "GarddensPapyrusFunctions", GarddenPapyrus::GetImmuneAV_Papyrus);

        vm->RegisterFunction("RegisterForOnAffected", "GarddensPapyrusFunctions", RegisterForOnAffected);
        vm->RegisterFunction("UnregisterForOnAffected", "GarddensPapyrusFunctions", UnregisterForOnAffected);

        vm->RegisterFunction("RegisterForOnAffectedAggressor", "GarddensPapyrusFunctions", RegisterForOnAffectedAggressor);
        vm->RegisterFunction("UnregisterForOnAffectedAggressor", "GarddensPapyrusFunctions", UnregisterForOnAffectedAggressor);

        vm->RegisterFunction("RegisterForOnAffectedVictim", "GarddensPapyrusFunctions", RegisterForOnAffectedVictim);
        vm->RegisterFunction("UnregisterForOnAffectedVictim", "GarddensPapyrusFunctions", UnregisterForOnAffectedVictim);

        vm->RegisterFunction("PlaceOnGround", "GarddensPapyrusFunctions", PlaceStaticOnGround_Papyrus);
        vm->RegisterFunction("PlaceRefAtMe", "GarddensPapyrusFunctions", PlaceRefAtMe_Papyrus);

       vm->RegisterFunction("GiveLeveledLoot", "GarddensPapyrusFunctions", GiveLeveledLoot_Papyrus);


        logger::info("Papyrus functions registered!");

        return true;
    }
}