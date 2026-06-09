#include <spdlog/sinks/basic_file_sink.h>

#include <shared_mutex>

namespace logger = SKSE::log;

#include "GarddenTopicFunctions.h"
#include "Persistence.h"
#include "RE/B/BSFixedString.h"
#include "RE/F/FormTypes.h"
#include "RE/T/TESForm.h"
#include "RE/T/TESTopic.h"
#include "RE/T/TESTopicInfo.h"
#include "SKSE/Logger.h"

namespace Garddens {

    // =========================
    // SHARED MUTEX FOR THREAD SAFETY
    // =========================
    static std::shared_mutex topicMutex;

    // =========================
    // UTILITY
    // =========================

    std::vector<RE::TESTopicInfo*> TopicFunctions::GetTopicInfos(RE::TESTopic* topic) {
        std::vector<RE::TESTopicInfo*> result;
        if (!topic || topic->numTopicInfos == 0 || !topic->topicInfos) return result;

        // Just a secure array copya
        RE::TESTopicInfo** tempArray = nullptr;
        std::uint32_t count = 0;

        {
            std::shared_lock lock(topicMutex);  // minimal block
            count = topic->numTopicInfos;
            tempArray = topic->topicInfos;
        }

        // Builds vector outside of lock
        for (std::uint32_t i = 0; i < count; i++) {
            if (tempArray[i]) result.push_back(tempArray[i]);
        }

        return result;
    }

    void TopicFunctions::SetTopicInfos(RE::TESTopic* topic, const std::vector<RE::TESTopicInfo*>& infos) {
        if (!topic) return;

        auto size = infos.size();
        // Alocates and fills array outside of lock
        auto newArray = RE::malloc<RE::TESTopicInfo*>(size);
        for (std::size_t i = 0; i < size; i++) {
            newArray[i] = infos[i];
        }

        {
            std::unique_lock lock(topicMutex);  // excluve writing here

            // Frees old array
            if (topic->topicInfos) {
                RE::free(topic->topicInfos);
            }

            topic->topicInfos = newArray;
            topic->numTopicInfos = static_cast<std::uint32_t>(size);
        }
    }

    // =========================
    // MAIN
    // =========================

    void TopicFunctions::ReplaceTopicInfo(RE::TESTopicInfo* source, RE::TESTopicInfo* target) {
        if (!source || !target) {
            logger::warn("ReplaceTopicInfo called with null pointer");
            return;
        }

        std::unique_lock lock(topicMutex);  // exclusive writing

        if (source->dataInfo != target) {
            source->dataInfo = target;
            logger::info("ReplaceTopicInfo: info replaced successfully");

            Persistence::AddReplace(source->GetFormID(), target->GetFormID());
        }
    }
}