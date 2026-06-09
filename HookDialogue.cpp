#include <spdlog/sinks/basic_file_sink.h>

namespace logger = SKSE::log;

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <cstring>

#include "GarddenPapyrus.h"
#include "Hooks.h"
#include "Persistence.h"
#include "RE/M/MenuTopicManager.h"
#include "RE/T/TESTopicInfo.h"
#include "RE/U/UI.h"
#include "SKSE/SKSE.h"


namespace {

    // =========================
    // THREAD CONTROL
    // =========================
    std::atomic<bool> running = false;
    std::thread loopThread;

    // =========================
    // STATE (PROTECTED)
    // =========================
    std::mutex stateMutex;

    RE::TESTopicInfo* activeTopic = nullptr;
    RE::Actor* activeSpeaker = nullptr;
    RE::Actor* lastSpeaker = nullptr;

    std::string lastValidText;

    // =========================
    // UTILS
    // =========================
    RE::Actor* GetSpeaker(RE::ObjectRefHandle handle) {
        auto ref = handle.get().get();
        return ref ? ref->As<RE::Actor>() : nullptr;
    }

    std::string GetTopicText(RE::TESTopicInfo* topic, RE::Actor* speaker) {
        if (!topic || !speaker) return "";

        try {
            auto dialogue = topic->GetDialogueData(speaker);

            if (dialogue.currentResponse) {
                auto response = dialogue.currentResponse->item;
                if (response && !response->text.empty()) {
                    return response->text.c_str();
                }
            }

            for (auto it = dialogue.responses.begin(); it != dialogue.responses.end(); ++it) {
                auto response = *it;
                if (response && !response->text.empty()) {
                    return response->text.c_str();
                }
            }
        } catch (...) {
            logger::warn("Error on getting text through DialogueItem");
        }

        return "";
    }

    // =========================
    // MAIN LOGIC (RUNS ON MAIN THREAD)
    // =========================
    void CheckDialogue() {
        std::lock_guard lock(stateMutex);

        if (!Garddens::Persistence::IsReady()) return;

        auto mtm = RE::MenuTopicManager::GetSingleton();
        if (!mtm) return;

        // =========================
        // STATE TRACKING
        // =========================
        static RE::FormID lastLineID = 0;
        static bool lineActive = false;

        // =========================
        // CURRENT TOPIC (INCLUDES INVISIBLE CONTINUE)
        // =========================
        RE::TESTopicInfo* currentTopic = mtm->currentTopicInfo ? mtm->currentTopicInfo : mtm->lastTopicInfo;
        RE::FormID currentID = currentTopic ? currentTopic->GetFormID() : 0;
        auto speaker = GetSpeaker(mtm->speaker);

        // =========================
        // PLAYER CHOICE
        // =========================
        auto selected = mtm->lastSelectedDialogue;
        static RE::FormID lastChoiceID = 0;

        if (selected && selected->parentTopicInfo && selected->parentTopicInfo->parentTopic) {
            auto choiceID = selected->parentTopicInfo->GetFormID();

            if (choiceID != lastChoiceID) {
                lastChoiceID = choiceID;

                if (selected->parentQuest) {
                    GarddenPapyrus::g_currentDialogueQuest = selected->parentQuest;
                }

                auto player = RE::PlayerCharacter::GetSingleton();
                std::string choiceText = selected->topicText.c_str();
                GarddenPapyrus::SendPlayerChoiceEvent(player, speaker, selected->parentTopicInfo, choiceText);
            }
        }

        // =========================
        // DETERMINE IF LINE ENDED
        // =========================
        bool dialogueActive = mtm->currentTopicInfo != nullptr;
        bool lineEnded = lineActive && (!dialogueActive || currentID != lastLineID);

        if (lineEnded && activeTopic) {
            std::string text = lastValidText.empty() ? GetTopicText(activeTopic, activeSpeaker) : lastValidText;

            GarddenPapyrus::SendLineEndEvent(activeSpeaker, reinterpret_cast<RE::TESForm*>(activeTopic), text);

            // reset
            GarddenPapyrus::g_currentDialogueQuest = nullptr;
            activeTopic = nullptr;
            activeSpeaker = nullptr;
            lastSpeaker = nullptr;
            lastValidText.clear();
            lineActive = false;

            // Keep lastLineID so OnLineSpoken is not send again until a new real line shows up
            // If dialogue menu was closed, lastLineID is the ID of the previous line
            // If dialogue menu is still open, updates below
        }

        // =========================
        // START NEW LINE (INCLUDES INVISIBLECONTINUE)
        // =========================
        if (currentID != 0 && currentID != lastLineID && !lineActive) {

            std::string text = GetTopicText(currentTopic, speaker);

            if (text.empty()) {
                logger::warn("Empty text on START {:08X}", currentID);
            }

            activeTopic = currentTopic;
            activeSpeaker = speaker;
            lastSpeaker = speaker;
            lastValidText = text;

            GarddenPapyrus::SendLineSpokenEvent(speaker, currentTopic, text);

            lastLineID = currentID;
            lineActive = true;


        }
    }

    // =========================
    // LOOP THREAD (SCHEDULER ONLY)
    // =========================

    void Loop() {
        logger::info("Loop thread started");

        bool inWorld = false;  // estado real de gameplay
        auto lastRun = std::chrono::steady_clock::now();

        while (running.load()) {
            auto ui = RE::UI::GetSingleton();

            // verifica se menus que realmente significam "fora do mundo"
            bool menuBlockingWorld = ui && (ui->IsApplicationMenuOpen());

            bool nowInWorld = !menuBlockingWorld;  // se não está em menu de bloqueio, está no mundo

            if (nowInWorld && !inWorld) {
                logger::info("Entered world!");

                if (!Garddens::Persistence::WasLoadedFromSave()) {
                    logger::info("New Game detected (no persistence data)");
                    Garddens::Persistence::ClearAll();
                }

                Garddens::Persistence::ReapplyAll();
                // não resetamos hooks aqui
            }

            if (!nowInWorld && inWorld) {
                logger::info("Left world (real exit)!");
                Hooks::Reset();  // reset apenas em saída real
            }

            inWorld = nowInWorld;

            // =========================
            // THROTTLED TASK DISPATCH
            // =========================
            if (inWorld) {
                auto now = std::chrono::steady_clock::now();

                if (now - lastRun > std::chrono::milliseconds(100)) {
                    lastRun = now;
                    SKSE::GetTaskInterface()->AddTask([]() { CheckDialogue(); });
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        logger::info("Loop thread stopped");
    }

}

// =========================
// PUBLIC API
// =========================

void Hooks::Install() {
    if (running.load()) {
        logger::info("Loop already running");
        return;
    }

    logger::info("Starting dialogue loop...");

    running = true;
    loopThread = std::thread(Loop);
}

void Hooks::Stop() {
    if (!running.load()) return;

    logger::info("Stopping dialogue loop...");

    running = false;

    if (loopThread.joinable()) {
        loopThread.join();
    }
}

void Hooks::Reset() {
    std::lock_guard lock(stateMutex);

    activeTopic = nullptr;
    activeSpeaker = nullptr;
    lastSpeaker = nullptr;

    lastValidText.clear();

    logger::info("Hooks reset");
}
