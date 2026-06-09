#include <string>
#include <unordered_map>
#include <vector>
#include <shared_mutex>

#include "RE/A/Actor.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/Skyrim.h"
#include "SKSE/Logger.h"
#include "SKSE/SKSE.h"
#include "SKSE/Trampoline.h"

#include "GarddenCombatFunctions.h"
#include "Persistence.h"

// Struct definition
struct ImmuneAVEntry {
    bool reflectDamage = false;
    float reflectPercent = 0.0f;
};

static std::shared_mutex g_ImmuneAVMutex;


// Define GlobalMap
std::unordered_map<RE::FormID, std::unordered_map<std::string, ImmuneAVEntry>> g_ImmuneAVs;


RE::ActorValue StringToAV(const std::string& av) {
    if (av == "Health") return RE::ActorValue::kHealth;
    if (av == "Magicka") return RE::ActorValue::kMagicka;
    if (av == "Stamina") return RE::ActorValue::kStamina;

    return RE::ActorValue::kNone;
}

bool IsActorImmuneAV(RE::Actor* actor, RE::ActorValue av, float& reflectPercent) {
    if (!actor) return false;

    std::shared_lock lock(g_ImmuneAVMutex);

    auto it = g_ImmuneAVs.find(actor->GetFormID());
    if (it == g_ImmuneAVs.end()) return false;

    for (auto& [name, entry] : it->second) {
        if (StringToAV(name) == av) {
            reflectPercent = entry.reflectPercent;
            return true;
        }
    }

    return false;
}

    // Function Implementation
void SetImmuneAV(RE::Actor* actor, const std::vector<std::string>& avNames, bool reflect, float percent) {
    if (!actor) return;
    std::unique_lock lock(g_ImmuneAVMutex);

    auto& actorMap = g_ImmuneAVs[actor->GetFormID()];
    for (const auto& av : avNames) {
        actorMap[av] = ImmuneAVEntry{reflect, percent};
    }
}

    void ClearImmuneAV(RE::Actor* actor, const std::vector<std::string>& avNames) {
    if (!actor) return;
    std::unique_lock lock(g_ImmuneAVMutex);

    auto it = g_ImmuneAVs.find(actor->GetFormID());
    if (it == g_ImmuneAVs.end()) return;

    auto& actorMap = it->second;
    if (avNames.empty()) {
        actorMap.clear();
    } else {
        for (const auto& av : avNames) actorMap.erase(av);
    }
    if (actorMap.empty()) g_ImmuneAVs.erase(it);
}

    void GetImmuneAV(RE::Actor* actor) {
        if (!actor) {
            SKSE::log::warn("GetImmuneAV: actor is null!");
            return;
        }

        auto it = g_ImmuneAVs.find(actor->GetFormID());
        if (it == g_ImmuneAVs.end()) {
            SKSE::log::info("Actor {:08X} has no immune AVs.", actor->GetFormID());
            return;
        }

        SKSE::log::info("Actor {:08X} immune AVs:", actor->GetFormID());
        for (const auto& [avName, entry] : it->second) {
            SKSE::log::info("  AV: {} | Reflect: {} | Percent: {}", avName, entry.reflectDamage, entry.reflectPercent);
        }
    }


    bool IsActorImmune(RE::Actor* actor, const std::string& av, float& reflectPercent) {
        if (!actor) return false;
        auto it = g_ImmuneAVs.find(actor->GetFormID());
        if (it != g_ImmuneAVs.end()) {
            auto inner = it->second.find(av);
            if (inner != it->second.end()) {
                reflectPercent = inner->second.reflectPercent;
                return true;
            }
        }
        return false;
    }




