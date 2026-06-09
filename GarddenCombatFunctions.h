#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <cstdint>

#include "RE/A/Actor.h"



    // -----------------------------
    // ImmuneAV System
    // -----------------------------

struct ImmuneAVEntry;

    extern std::unordered_map<RE::FormID, std::unordered_map<std::string, ImmuneAVEntry>> g_ImmuneAVs;

    // Funções principais
    std::vector<std::string> GetImmuneAVStrings(RE::Actor* actor);

    void SetImmuneAV(RE::Actor* actor, const std::vector<std::string>& avNames, bool reflect, float percent);
    void ClearImmuneAV(RE::Actor* actor, const std::vector<std::string>& avNames);
    void GetImmuneAV(RE::Actor* actor);
    bool IsActorImmuneAV(RE::Actor* actor, RE::ActorValue av, float& reflectPercent);


    namespace GarddenCombat {

        using CombatTagMask = uint64_t;

        enum CombatTags : CombatTagMask {
            kNone = 0,

            // =========================
            // Effect types
            // =========================
            kDamage = 1ull << 0,
            kDoT = 1ull << 1,
            kDebuff = 1ull << 2,
            kControl = 1ull << 3,

            // =========================
            // Origin
            // =========================
            kMelee = 1ull << 4,
            kRanged = 1ull << 5,
            kSpell = 1ull << 6,
            kConcentration = 1ull << 7,

            // =========================
            // Hit Proprieties
            // =========================
            kPowerAttack = 1ull << 8,
            kSneakAttack = 1ull << 9,
            kBashAttack = 1ull << 10,
            kBlocked = 1ull << 11,

            // =========================
            // Elements
            // =========================
            kFire = 1ull << 12,
            kFrost = 1ull << 13,
            kShock = 1ull << 14,
            kPoison = 1ull << 15,

            // =========================
            // Spell Delivery
            // =========================
            kTouch = 1ull << 16,
            kSelf = 1ull << 17,
            kAimed = 1ull << 18,
            kTarget = 1ull << 19,

            // =========================
            // Hostility
            // =========================
            kHostile = 1ull << 20,
            kNonHostile = 1ull << 21,

        };

    }

