#pragma once

#include <cstdint>
#include <vector>


#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include "GarddenPapyrus.h"

namespace Garddens::Persistence {

    // =========================
    // DATA STRUCTURES
    // =========================

    struct MoveOp {
        RE::FormID info;
        RE::FormID targetTopic;
        bool randomAll;
    };

    struct ReplaceOp {
        RE::FormID source;
        RE::FormID target;
    };

    struct FavorOp {
        RE::FormID info;
        std::uint8_t favor;
    };

    struct ListenerOp {
        RE::FormID listener;
        RE::FormID target;
        std::uint8_t type;  // 0=global, 1=topic, 2=speaker, 3=quest
    };

    // =========================
    // ADD OPERATIONS
    // =========================

    void AddMove(RE::FormID info, RE::FormID topic, bool randomAll);
    void AddReplace(RE::FormID source, RE::FormID target);
    void AddFavor(RE::FormID info, std::uint8_t favor);

    void AddListener(RE::FormID listener, RE::FormID target, std::uint8_t type);

    void AddOnAffectedListener(RE::FormID listener);
    void RemoveOnAffectedListener(RE::FormID listener);

    void RemoveListener(RE::FormID listener, RE::FormID target, std::uint8_t type);

    void AddImmuneAV(RE::FormID actor, const std::vector<std::string>& avs, bool reflect, float percent);

    void RemoveImmuneAV(RE::FormID actor, const std::vector<std::string>& avs);  // remove AVs específicos
    void ClearImmuneAVs(RE::FormID actor);                                       // remove todos AVs do ator

    void ReapplyListenersDeferred();


        // mutex para sincronização
    extern std::mutex persistenceMutex;

    // lista de listeners deferidos
    extern std::vector<ListenerOp> deferredListeners;

    // tipo do listener OnAffected
    constexpr std::uint8_t kListenerTypeOnAffected = 4;
    constexpr std::uint8_t kListenerTypeOnAffectedAggressor = 5;
    constexpr std::uint8_t kListenerTypeOnAffectedVictim = 6;

    // =========================
    // APPLY / STATE
    // =========================

    void ReapplyAll();

    bool IsReady();

    // =========================
    // SERIALIZATION
    // =========================

    void Save(SKSE::SerializationInterface* ser);
    void Load(SKSE::SerializationInterface* ser);
    void Revert();

    // =========================
    // OPTIONAL
    // =========================

    void ClearAll();
    bool WasLoadedFromSave();


    void ScheduleOnAffectedReapply();

   
}