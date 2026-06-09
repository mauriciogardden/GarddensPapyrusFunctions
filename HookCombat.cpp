#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include "GarddenCombatFunctions.h"
#include "GarddenPapyrus.h"
#include "Hooks.h"
#include "Persistence.h"
#include "RE/A/Actor.h"
#include "RE/B/BSTEvent.h"
#include "RE/H/HitData.h"
#include "RE/S/ScriptEventSourceHolder.h"
#include "RE/T/TESObjectREFR.h"
#include "SKSE/SKSE.h"
#include "SKSE/Trampoline.h"

namespace logger = SKSE::log;



namespace {

    // Protection agains infinite reflect
    thread_local bool g_isReflecting = false;
}

namespace GarddenCombat {
    // =====================================================
    // CONFIG
    // =====================================================

    // ~1 frame (60fps).
    constexpr auto AFFECT_WINDOW = std::chrono::milliseconds(100);

    enum class AffectOrigin { Weapon, Spell, ConcentrationSpell };

    

    // =====================================================
    // CLASS
    // =====================================================

    CombatTagMask ClassifyEffect(RE::EffectSetting* effect) {
        if (!effect) return kNone;

        using Archetype = RE::EffectArchetypes::ArchetypeID;

        CombatTagMask tags = kNone;

        auto archetype = effect->GetArchetype();
        auto av = effect->data.primaryAV;

        switch (archetype) {
            case Archetype::kValueModifier:
            case Archetype::kPeakValueModifier:
                if (av == RE::ActorValue::kHealth) {
                    if (effect->data.flags.all(RE::EffectSetting::EffectSettingData::Flag::kDetrimental)) {
                        tags |= kDamage;
                    }
                } else {
                    tags |= kDebuff;
                }
                break;

            case Archetype::kScript:
            case Archetype::kCloak:
            case Archetype::kDualValueModifier:
                tags |= kDoT;
                break;

            case Archetype::kParalysis:
            case Archetype::kStagger:
            case Archetype::kDemoralize:
            case Archetype::kTurnUndead:
            case Archetype::kCalm:
            case Archetype::kFrenzy:
                tags |= kControl;
                break;
        }

        // =========================
        // Elements
        // =========================
        auto resistAV = effect->data.resistVariable;

        if (resistAV == RE::ActorValue::kResistFire) tags |= kFire;
        if (resistAV == RE::ActorValue::kResistFrost) tags |= kFrost;
        if (resistAV == RE::ActorValue::kResistShock) tags |= kShock;
        if (resistAV == RE::ActorValue::kPoisonResist) tags |= kPoison;

        return tags;
    }

    // =====================================================
    // KEY (Deduplication)
    // =====================================================

    struct AffectKey {
        RE::FormID attacker;
        RE::FormID victim;

        bool operator==(const AffectKey& other) const { return attacker == other.attacker && victim == other.victim; }
    };

    struct AffectKeyHash {
        std::size_t operator()(const AffectKey& k) const {
            return std::hash<uint32_t>()(k.attacker) ^ (std::hash<uint32_t>()(k.victim) << 1);
        }
    };

    struct AffectData {
        float time;
        RE::FormID lastSource;
    };

    

    struct EffectEvent {
        RE::Actor* attacker;
        RE::Actor* victim;
        RE::EffectSetting* effect;
        AffectOrigin origin;
        CombatTagMask subtype;
    };

    // =====================================================
    // CACHE (time)
    // =====================================================

    static std::unordered_map<AffectKey, AffectData, AffectKeyHash> affectCache;

    class HitEventSink : public RE::BSTEventSink<RE::TESHitEvent> {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* event, RE::BSTEventSource<RE::TESHitEvent>*) {
            if (!event) return RE::BSEventNotifyControl::kContinue;

            //logger::info("TESHitEvent called!");

            auto victim = event->target ? event->target->As<RE::Actor>() : nullptr;
            auto attacker = event->cause ? event->cause->As<RE::Actor>() : nullptr;

            //logger::info("Attacker ptr: {}", attacker ? "VALID" : "NULL");
            if (!victim) return RE::BSEventNotifyControl::kContinue;

            // =========================
            // Font: weapon or magic
            RE::FormID sourceID = event->source;
            if (sourceID == 0 && attacker) {
                if (auto weapon = attacker->GetEquippedObject(false)) {
                    sourceID = weapon->GetFormID();
                }
            }

            auto* sourceForm = RE::TESForm::LookupByID(sourceID);
            auto spell = sourceForm ? sourceForm->As<RE::SpellItem>() : nullptr;


            auto now = std::chrono::steady_clock::now();
            std::vector<EffectEvent> events;


            // =========================
            // Concentration spell → 1s cache
            if (spell && spell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration && attacker) {


                struct ConcentrationKey {
                    RE::FormID attacker;
                    RE::FormID victim;
                    RE::FormID spellID;
                    bool operator==(const ConcentrationKey& other) const {
                        return attacker == other.attacker && victim == other.victim && spellID == other.spellID;
                    }
                };
                struct ConcentrationHash {
                    std::size_t operator()(const ConcentrationKey& k) const {
                        return std::hash<RE::FormID>()(k.attacker) ^ (std::hash<RE::FormID>()(k.victim) << 1) ^
                               (std::hash<RE::FormID>()(k.spellID) << 2);
                    }
                };
                static std::unordered_map<ConcentrationKey, std::chrono::steady_clock::time_point, ConcentrationHash>
                    concentrationCache;
                constexpr auto CONCENTRATION_INTERVAL = std::chrono::seconds(1);

                ConcentrationKey ck{attacker->GetFormID(), victim->GetFormID(), spell->GetFormID()};
                auto itC = concentrationCache.find(ck);

                if (itC == concentrationCache.end() || std::chrono::duration_cast<std::chrono::milliseconds>(
                                                           now - itC->second) >= CONCENTRATION_INTERVAL) {
                    concentrationCache[ck] = now;

                    // Subtype by spell's first effect
                    CombatTagMask subtype = kNone;

                    // DELIVERY
                    switch (spell->GetDelivery()) {
                        case RE::MagicSystem::Delivery::kTouch:
                            subtype |= kTouch;
                            break;
                        case RE::MagicSystem::Delivery::kSelf:
                            subtype |= kSelf;
                            break;
                        case RE::MagicSystem::Delivery::kAimed:
                            subtype |= kAimed;
                            break;
                        case RE::MagicSystem::Delivery::kTargetActor:
                        case RE::MagicSystem::Delivery::kTargetLocation:
                            subtype |= kTarget;
                            break;
                    }

                    // HOSTILE
                    bool isHostile = false;

                    for (auto& eff : spell->effects) {
                        if (eff && eff->baseEffect &&
                            eff->baseEffect->data.flags.all(RE::EffectSetting::EffectSettingData::Flag::kDetrimental)) {
                            isHostile = true;
                            break;
                        }
                    }

                    subtype |= isHostile ? kHostile : kNonHostile;

                    // CLASSIFY
                    if (!spell->effects.empty() && spell->effects[0] && spell->effects[0]->baseEffect) {
                        subtype |= ClassifyEffect(spell->effects[0]->baseEffect);
                    }

                    events.push_back({attacker, victim, nullptr, AffectOrigin::ConcentrationSpell, subtype});
                }
            }
            // =========================
            // Weapons and regular spells - 50ms cache
            else {
                struct SimpleKey {
                    RE::FormID attacker;
                    RE::FormID victim;
                    RE::FormID source;
                    uint32_t flags;

                    bool operator==(const SimpleKey& other) const {
                        return attacker == other.attacker && victim == other.victim && source == other.source &&
                               flags == other.flags;
                    }
                };
                struct SimpleKeyHash {
                    std::size_t operator()(const SimpleKey& k) const {
                        return std::hash<RE::FormID>()(k.attacker) ^ (std::hash<RE::FormID>()(k.victim) << 1) ^
                               (std::hash<RE::FormID>()(k.source) << 2) ^ (std::hash<uint32_t>()(k.flags) << 3);
                    }
                };
                struct CacheData {
                    std::chrono::steady_clock::time_point time;
                };
                static std::unordered_map<SimpleKey, CacheData, SimpleKeyHash> simpleCache;
                constexpr auto MIN_HIT_INTERVAL = std::chrono::milliseconds(50);

                SimpleKey sk{attacker ? attacker->GetFormID() : 0, victim->GetFormID(), event->source,
                             event->flags.underlying()};
                auto itS = simpleCache.find(sk);

                if (itS == simpleCache.end() ||
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - itS->second.time) >= MIN_HIT_INTERVAL) {
                    simpleCache[sk] = {now};

                    AffectOrigin origin = AffectOrigin::Weapon;
                    CombatTagMask subtype = kDamage;
                    RE::EffectSetting* effect = nullptr;

                    // =========================
                    // SPELL (override weapon)
                    // =========================
                    if (spell) {
                        origin = AffectOrigin::Spell;
                        subtype = kNone;

                        // DELIVERY
                        switch (spell->GetDelivery()) {
                            case RE::MagicSystem::Delivery::kTouch:
                                subtype |= kTouch;
                                break;

                            case RE::MagicSystem::Delivery::kSelf:
                                subtype |= kSelf;
                                break;

                            case RE::MagicSystem::Delivery::kAimed:
                                subtype |= kAimed;
                                break;

                            case RE::MagicSystem::Delivery::kTargetActor:
                            case RE::MagicSystem::Delivery::kTargetLocation:
                                subtype |= kTarget;
                                break;
                        }

                        // HOSTILITY
                        bool isHostile = false;

                        for (auto& eff : spell->effects) {
                            if (eff && eff->baseEffect) {
                                if (eff->baseEffect->data.flags.all(
                                        RE::EffectSetting::EffectSettingData::Flag::kDetrimental)) {
                                    isHostile = true;
                                    break;
                                }
                            }
                        }

                        subtype |= isHostile ? kHostile : kNonHostile;

                        // EFFECT CLASSIFICATION
                        if (!spell->effects.empty() && spell->effects[0] && spell->effects[0]->baseEffect) {
                            effect = spell->effects[0]->baseEffect;

                            subtype |= ClassifyEffect(effect);
                        }
                    }

                    // =========================
                    // ADD SINGLE EVENT
                    // =========================
                    events.push_back({attacker, victim, effect, origin, subtype});
                }
            }

             // =========================
            // PAPYRUS SendOnAffectedEvent
            for (auto& e : events) {
                bool powerAttack = (event->flags & RE::TESHitEvent::Flag::kPowerAttack) != RE::TESHitEvent::Flag::kNone;
                bool sneakAttack = (event->flags & RE::TESHitEvent::Flag::kSneakAttack) != RE::TESHitEvent::Flag::kNone;
                bool bashAttack = (event->flags & RE::TESHitEvent::Flag::kBashAttack) != RE::TESHitEvent::Flag::kNone;
                bool hitBlocked = (event->flags & RE::TESHitEvent::Flag::kHitBlocked) != RE::TESHitEvent::Flag::kNone;

                CombatTagMask tags = kNone;

                // origin
                switch (e.origin) {
                    case AffectOrigin::Weapon: {
                        auto weapon = sourceForm ? sourceForm->As<RE::TESObjectWEAP>() : nullptr;

                        if (weapon && weapon->IsRanged()) {
                            tags |= kRanged;
                        } else {
                            tags |= kMelee;
                        }
                        break;
                    }
                    case AffectOrigin::Spell:
                        tags |= kSpell;
                        break;
                    case AffectOrigin::ConcentrationSpell:
                        tags |= kSpell | kConcentration;
                        break;
                       
                }

                // subtype
                if (e.subtype != kNone) {
                    tags |= e.subtype;
                }

                // hit flags
                if (powerAttack) tags |= kPowerAttack;
                if (sneakAttack) tags |= kSneakAttack;
                if (bashAttack) tags |= kBashAttack;
                if (hitBlocked) tags |= kBlocked;


                //if (tags == kNone) {
                //    continue;
                //}
               

                GarddenPapyrus::SendOnAffectedEvent(e.attacker, e.victim, sourceForm, tags);

            // =========================
            // Final log

                RE::FormID logSource = event->source ? event->source : 0;

                 //logger::info("EffectEvent | Victim {:08X} | Attacker {:08X} | Source {:08X} | Tags {:016X}",
                 //            e.victim->GetFormID(), e.attacker ? e.attacker->GetFormID() : 0,
                 //            event->source ? event->source : 0, tags);
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };




    // =====================================================
    // SINGLETON
    // =====================================================

    static HitEventSink g_hitSink;

     // =====================================================
    // INSTALL
    // =====================================================   

    void Install() {
        static bool installed = false;
        if (installed) {
            return;
        }
        installed = true;

        logger::info("Installing GarddenCombat...");

        // Event sink
        auto source = RE::ScriptEventSourceHolder::GetSingleton();
        if (!source) {
            logger::critical("Failed to get ScriptEventSourceHolder");
            return;
        }

        source->AddEventSink<RE::TESHitEvent>(&g_hitSink);


        logger::info("TESHitEvent sink registered");
    }

}



