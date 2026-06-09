#include <spdlog/sinks/basic_file_sink.h>

#include "GarddenPapyrus.h"
#include "Hooks.h"
#include "Persistence.h"
#include "SKSE/SKSE.h"

namespace logger = SKSE::log;

// =========================
// LOG
// =========================
void SetupLog() {
    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");

    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);

    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));

    spdlog::set_default_logger(std::move(loggerPtr));
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);
}

// =========================
// MESSAGES
// =========================
void MessageHandler(SKSE::MessagingInterface::Message* msg) {
    if (!msg) return;

    switch (msg->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            logger::info("DataLoaded received!");
            Hooks::Install();
            SKSE::GetTaskInterface()->AddTask([]() { GarddenCombat::Install(); });
            break;

        case SKSE::MessagingInterface::kPostLoadGame:
            logger::info("PostLoadGame received!");
            //Hooks::Install();
            Garddens::Persistence::ReapplyListenersDeferred();           
            break;

        case SKSE::MessagingInterface::kNewGame:
            logger::info("NewGame received!");
            //Hooks::Install();
            break;
    }
}



// =========================
// ENTRY POINT
// =========================
SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SetupLog();

    SKSE::Init(skse);
    SKSE::AllocTrampoline(64);

    // =========================
    // MESSAGING
    // =========================
    auto messaging = SKSE::GetMessagingInterface();
    if (messaging) {
        messaging->RegisterListener(MessageHandler);
    }

    // =========================
    // SERIALIZATION
    // =========================
    auto serialization = SKSE::GetSerializationInterface();

    serialization->SetUniqueID('GARD');  // unic ID

    serialization->SetSaveCallback([](SKSE::SerializationInterface* ser) {
        logger::info("Serialization: Save callback");
        Garddens::Persistence::Save(ser);
    });

    serialization->SetLoadCallback([](SKSE::SerializationInterface* ser) {
        logger::info("Serialization: Load callback");

        Garddens::Persistence::Load(ser);

        Garddens::Persistence::ReapplyAll();
    });

    serialization->SetRevertCallback([](SKSE::SerializationInterface*) {
        logger::info("Serialization: Revert callback");

        Hooks::Reset();
        Garddens::Persistence::Revert();
    });

    // =========================
    // PAPYRUS
    // =========================
    SKSE::GetPapyrusInterface()->Register(GarddenPapyrus::RegisterFuncs);

    logger::info("Plugin loaded");

    return true;
}