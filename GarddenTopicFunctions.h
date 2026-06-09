#pragma once

#include <vector>

#include "RE/T/TESTopic.h"
#include "RE/T/TESTopicInfo.h"

namespace Garddens {
    class TopicFunctions {
    public:
        // Replaces a TESTopicInfo with another (source->target)
        static void ReplaceTopicInfo(RE::TESTopicInfo* source, RE::TESTopicInfo* target);

    private:
        // Utilitary function to get all TopicInfos of a Topic as vector
        static std::vector<RE::TESTopicInfo*> GetTopicInfos(RE::TESTopic* topic);

        // Utilitary function to set all TopicInfos of a Top from a vector
        static void SetTopicInfos(RE::TESTopic* topic, const std::vector<RE::TESTopicInfo*>& infos);
    };
}