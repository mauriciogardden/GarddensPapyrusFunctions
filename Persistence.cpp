#include <spdlog/sinks/basic_file_sink.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace logger = SKSE::log;

#include "GarddenCombatFunctions.h"
#include "GarddenPapyrus.h"
#include "GarddenTopicFunctions.h"
#include "Persistence.h"

namespace Garddens::Persistence {

    // =========================
    // DATA STRUCTS
    // =========================

    struct ImmuneAVOp {
        RE::FormID actor;
        std::vector<std::string> avs;
        bool reflect;
        float percent;
    };

    static std::vector<ReplaceOp> replaces;
    static std::vector<FavorOp> favors;
    static std::vector<ListenerOp> listeners;          // listeners normais
    std::vector<ListenerOp> deferredListeners;  // listeners OnAffected para runtime seguro
    static std::vector<ImmuneAVOp> immuneAVs;

    static bool g_isReapplying = false;
    static bool g_ready = false;

    std::mutex persistenceMutex;

    constexpr std::uint32_t kReplaceRecord = 'GREP';
    constexpr std::uint32_t kFavorRecord = 'GFAV';
    constexpr std::uint32_t kListenerRecord = 'GLIS';
    constexpr std::uint32_t kImmuneAVRecord = 'GIMM';

    // =========================
    // READY STATE
    // =========================

    bool IsReady() {
        std::lock_guard lock(persistenceMutex);
        return g_ready;
    }


    // =========================
    // ADD FUNCTIONS
    // =========================

    void AddReplace(RE::FormID source, RE::FormID target) {
        std::lock_guard lock(persistenceMutex);
        if (g_isReapplying) return;
        for (auto& r : replaces) {
            if (r.source == source) {
                r.target = target;
                return;
            }
        }
        replaces.push_back({source, target});
    }

    void AddFavor(RE::FormID info, std::uint8_t favor) {
        std::lock_guard lock(persistenceMutex);
        if (g_isReapplying) return;
        for (auto& f : favors) {
            if (f.info == info) {
                f.favor = favor;
                return;
            }
        }
        favors.push_back({info, favor});
    }

    void AddListener(RE::FormID listener, RE::FormID target, std::uint8_t type) {
        std::lock_guard lock(persistenceMutex);
        if (g_isReapplying) return;
        for (auto& l : listeners) {
            if (l.listener == listener && l.target == target && l.type == type) return;
        }
        listeners.push_back({listener, target, type});
    }

      void AddOnAffectedListener(RE::FormID listener) {
        std::lock_guard lock(persistenceMutex);
        if (g_isReapplying) return;
        for (auto& l : listeners) {
            if (l.listener == listener && l.type == kListenerTypeOnAffected) return;
        }
        listeners.push_back({listener, 0, kListenerTypeOnAffected});
    }


    void AddImmuneAV(RE::FormID actor, const std::vector<std::string>& avs, bool reflect, float percent) {
        std::lock_guard lock(persistenceMutex);
        if (g_isReapplying) return;
        for (auto& op : immuneAVs) {
            if (op.actor == actor) {
                op.avs = avs;
                op.reflect = reflect;
                op.percent = percent;
                return;
            }
        }
        immuneAVs.push_back({actor, avs, reflect, percent});
    }

    // =========================
    // REMOVE FUNCTIONS
    // =========================

    void RemoveImmuneAV(RE::FormID actor, const std::vector<std::string>& avs) {
        std::lock_guard lock(persistenceMutex);
        if (g_isReapplying) return;
        auto it =
            std::find_if(immuneAVs.begin(), immuneAVs.end(), [actor](const auto& op) { return op.actor == actor; });
        if (it == immuneAVs.end()) return;
        for (auto& av : avs) it->avs.erase(std::remove(it->avs.begin(), it->avs.end(), av), it->avs.end());
        if (it->avs.empty()) immuneAVs.erase(it);
        logger::info("Persistence: Removed AVs from actor {:08X}", actor);
    }

    void ClearImmuneAVs(RE::FormID actor) {
        std::lock_guard lock(persistenceMutex);
        if (g_isReapplying) return;
        immuneAVs.erase(
            std::remove_if(immuneAVs.begin(), immuneAVs.end(), [actor](const auto& op) { return op.actor == actor; }),
            immuneAVs.end());
        logger::info("Persistence: Cleared all AVs for actor {:08X}", actor);
    }

    void RemoveListener(RE::FormID listener, RE::FormID target, std::uint8_t type) {
        std::lock_guard lock(persistenceMutex);
        if (g_isReapplying) return;
        listeners.erase(std::remove_if(listeners.begin(), listeners.end(),
                                       [&](const ListenerOp& l) {
                                           return l.listener == listener && l.target == target && l.type == type;
                                       }),
                        listeners.end());
    }

    void RemoveOnAffectedListener(RE::FormID listener) {
        std::lock_guard lock(persistenceMutex);
        if (g_isReapplying) return;
        listeners.erase(std::remove_if(listeners.begin(), listeners.end(),
                                       [&](const ListenerOp& l) {
                                           return l.listener == listener && l.type == kListenerTypeOnAffected;
                                       }),
                        listeners.end());
    }

    // =========================
    // REAPPLY
    // =========================

    void ReapplyAll() {
        std::lock_guard lock(persistenceMutex);
        logger::info("Persistence: Reapplying all operations...");
        logger::info("REAPPLY: listeners total {}", listeners.size());
        g_isReapplying = true;

        // Replace
        for (auto& r : replaces) {
            auto* src = RE::TESForm::LookupByID<RE::TESTopicInfo>(r.source);
            auto* tgt = RE::TESForm::LookupByID<RE::TESTopicInfo>(r.target);
            if (src && tgt) Garddens::TopicFunctions::ReplaceTopicInfo(src, tgt);
        }

        // Favor
        for (auto& f : favors) {
            auto* info = RE::TESForm::LookupByID<RE::TESTopicInfo>(f.info);
            if (info) {
                info->favorLevel = static_cast<RE::TESTopicInfo::FavorLevel>(f.favor);
                info->data.flags |= RE::TOPIC_INFO_DATA::TOPIC_INFO_FLAGS::kSpendsFavorPoints;
            }
        }

        // ImmuneAV
        for (auto& op : immuneAVs) {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(op.actor);
            if (actor) SetImmuneAV(actor, op.avs, op.reflect, op.percent);
        }

        // Listeners (exceto OnAffected) aplicados agora
        for (auto& l : listeners) {
            if (l.type != kListenerTypeOnAffected && l.type != kListenerTypeOnAffectedAggressor &&
                l.type != kListenerTypeOnAffectedVictim) {
                auto* form = RE::TESForm::LookupByID(l.listener);
                if (!form) continue;
                switch (l.type) {
                    case 0:
                        GarddenPapyrus::AddGlobalListener(form);
                        break;
                    case 1:
                        GarddenPapyrus::AddTopicListener(l.target, form);
                        break;
                    case 2:
                        GarddenPapyrus::AddSpeakerListener(l.target, form);
                        break;
                    case 3:
                        GarddenPapyrus::AddQuestListener(l.target, form);
                        break;
                }
            }
        }


        // OnAffected listeners vão para deferred
        deferredListeners.clear();
        for (auto& l : listeners) {
            if (l.type == kListenerTypeOnAffected || l.type == kListenerTypeOnAffectedAggressor ||
                l.type == kListenerTypeOnAffectedVictim)
                deferredListeners.push_back(l);
        }

        logger::info("REAPPLY: deferred listeners {}", deferredListeners.size());

        g_isReapplying = false;
        g_ready = true;

        logger::info("Persistence: Reapply complete. Call ReapplyListenersDeferred() at runtime safe point!");
    }

    void Garddens::Persistence::ReapplyListenersDeferred() {
        std::lock_guard lock(persistenceMutex);

        // Copia os listeners para registrar fora do lock
        auto listenersToApply = deferredListeners;
        deferredListeners.clear();

        // Adiciona uma task para execução segura no runtime
        SKSE::GetTaskInterface()->AddTask([listenersToApply]() {

            logger::info("DEFERRED: Applying {} listeners", listenersToApply.size());
            for (auto& l : listenersToApply) {

                logger::info("DEFERRED: Processing listener {:08X} type {}", l.listener, l.type);
                auto* form = RE::TESForm::LookupByID(l.listener);
                if (!form) {
                    logger::warn("DEFERRED: Form {:08X} NOT FOUND", l.listener);
                    continue;
                }

                switch (l.type) {
                    case kListenerTypeOnAffected:
                        // >>> registra de forma segura apenas aqui
                        GarddenPapyrus::AddAffectedListener(form);
                        break;
                    case kListenerTypeOnAffectedAggressor:
                        GarddenPapyrus::AddOnAffectedAggressorListener(l.target, form);
                        break;

                    case kListenerTypeOnAffectedVictim:
                        GarddenPapyrus::AddOnAffectedVictimListener(l.target, form);
                        break;
                    case 0:
                        GarddenPapyrus::AddGlobalListener(form);
                        break;
                    case 1:
                        GarddenPapyrus::AddTopicListener(l.target, form);
                        break;
                    case 2:
                        GarddenPapyrus::AddSpeakerListener(l.target, form);
                        break;
                    case 3:
                        GarddenPapyrus::AddQuestListener(l.target, form);
                        break;
                }
                              
            }

            logger::info("Deferred listener reapply complete");
        });
    }

    // =========================
    // SAVE / LOAD
    // =========================

    void Save(SKSE::SerializationInterface* ser) {
        std::lock_guard lock(persistenceMutex);
        logger::info("Persistence: Saving...");

        if (!replaces.empty()) {
            ser->OpenRecord(kReplaceRecord, 1);
            std::uint32_t count = replaces.size();
            ser->WriteRecordData(&count, sizeof(count));
            ser->WriteRecordData(replaces.data(), count * sizeof(ReplaceOp));
        }

        if (!favors.empty()) {
            ser->OpenRecord(kFavorRecord, 1);
            std::uint32_t count = favors.size();
            ser->WriteRecordData(&count, sizeof(count));
            ser->WriteRecordData(favors.data(), count * sizeof(FavorOp));
        }

        if (!listeners.empty()) {
            ser->OpenRecord(kListenerRecord, 1);
            std::uint32_t count = listeners.size();
            ser->WriteRecordData(&count, sizeof(count));
            for (auto& l : listeners) ser->WriteRecordData(&l, sizeof(ListenerOp));
        }

        if (!immuneAVs.empty()) {
            ser->OpenRecord(kImmuneAVRecord, 1);
            std::uint32_t count = immuneAVs.size();
            ser->WriteRecordData(&count, sizeof(count));
            for (auto& op : immuneAVs) {
                ser->WriteRecordData(&op.actor, sizeof(op.actor));
                ser->WriteRecordData(&op.reflect, sizeof(op.reflect));
                ser->WriteRecordData(&op.percent, sizeof(op.percent));

                std::uint32_t avCount = op.avs.size();
                ser->WriteRecordData(&avCount, sizeof(avCount));
                for (auto& s : op.avs) {
                    std::uint32_t len = s.size();
                    ser->WriteRecordData(&len, sizeof(len));
                    ser->WriteRecordData(s.data(), len);
                }
            }
        }

        logger::info("Persistence: Save complete");
    }

    void Load(SKSE::SerializationInterface* ser) {
        std::lock_guard lock(persistenceMutex);
        logger::info("Persistence: Loading...");

        std::uint32_t type, version, length;

        replaces.clear();
        favors.clear();
        listeners.clear();
        deferredListeners.clear();
        immuneAVs.clear();

        while (ser->GetNextRecordInfo(type, version, length)) {
            switch (type) {
                case kReplaceRecord: {
                    std::uint32_t count;
                    ser->ReadRecordData(&count, sizeof(count));
                    replaces.resize(count);
                    ser->ReadRecordData(replaces.data(), count * sizeof(ReplaceOp));
                    break;
                }
                case kFavorRecord: {
                    std::uint32_t count;
                    ser->ReadRecordData(&count, sizeof(count));
                    favors.resize(count);
                    ser->ReadRecordData(favors.data(), count * sizeof(FavorOp));
                    break;
                }
                case kListenerRecord: {
                    std::uint32_t count;
                    ser->ReadRecordData(&count, sizeof(count));

                    logger::info("LOAD: Found {} listeners in save", count);

                    listeners.resize(count);

                    for (auto& l : listeners) {
                        ser->ReadRecordData(&l, sizeof(ListenerOp));

                        logger::info("LOAD: Listener {:08X} type {}", l.listener, l.type);
                    }

                    break;
                }
                case kImmuneAVRecord: {
                    std::uint32_t count;
                    ser->ReadRecordData(&count, sizeof(count));
                    immuneAVs.clear();
                    immuneAVs.reserve(count);
                    for (std::uint32_t i = 0; i < count; ++i) {
                        ImmuneAVOp op;
                        ser->ReadRecordData(&op.actor, sizeof(op.actor));
                        ser->ReadRecordData(&op.reflect, sizeof(op.reflect));
                        ser->ReadRecordData(&op.percent, sizeof(op.percent));

                        std::uint32_t avCount;
                        ser->ReadRecordData(&avCount, sizeof(avCount));
                        op.avs.resize(avCount);
                        for (auto& s : op.avs) {
                            std::uint32_t len;
                            ser->ReadRecordData(&len, sizeof(len));
                            s.resize(len);
                            ser->ReadRecordData(s.data(), len);
                        }
                        immuneAVs.push_back(std::move(op));
                    }
                    break;
                }
                default:
                    logger::warn("Persistence: Unknown record type {:08X}", type);
                    break;
            }
        }

        // Deferred listeners
        logger::info("LOAD: Total listeners after load: {}", listeners.size());

        deferredListeners.clear();
        for (auto& l : listeners) {
            if (l.type == kListenerTypeOnAffected || l.type == kListenerTypeOnAffectedAggressor ||
                l.type == kListenerTypeOnAffectedVictim) {
                deferredListeners.push_back(l);

                logger::info("LOAD: Deferred OnAffected listener {:08X}", l.listener);
            }
        }

        logger::info("LOAD: Deferred listeners count: {}", deferredListeners.size());

        g_ready = true;
        logger::info("Persistence: Load complete. OnAffected listeners will be applied deferred.");
    }

    // =========================
    // REVERT / CLEAR
    // =========================

    void Revert() {
        std::lock_guard lock(persistenceMutex);
        replaces.clear();
        favors.clear();
        listeners.clear();
        deferredListeners.clear();
        immuneAVs.clear();
        g_ready = false;
        g_isReapplying = false;
        logger::info("Persistence: Reverted");
    }

    void ClearRuntimeListeners() {
        std::lock_guard lock(persistenceMutex);
        GarddenPapyrus::ClearAllListeners();
    }

    void ClearAll() {
        std::lock_guard lock(persistenceMutex);
        replaces.clear();
        favors.clear();
        listeners.clear();
        deferredListeners.clear();
        immuneAVs.clear();
        logger::info("Persistence: Cleared all");
    }

    bool WasLoadedFromSave() {
        std::lock_guard lock(persistenceMutex);
        return !replaces.empty() || !favors.empty() || !listeners.empty() || !immuneAVs.empty();
    }

}  // namespace Garddens::Persistence