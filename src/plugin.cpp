#include "hooks/MenuHooks.h"
#include "menu/Menu.h"
#include "settings/Settings.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

namespace HideQuestItems
{
    void InitializeLog();
}

void HideQuestItems::InitializeLog()
{
    try {
        if (const auto directory = SFSE::GetLogDirectory()) {
            const auto logFile = *directory / (PLUGIN_NAME ".log");
            const auto log = logger::basic_logger_mt(PLUGIN_NAME, logFile.string(), true);
            log->sinks().push_back(std::make_shared<logger::sinks::msvc_sink_mt>());
#ifdef NDEBUG
            constexpr auto level = logger::level::info;
#else
            constexpr auto level = logger::level::debug;
#endif
            log->set_level(level);
            log->flush_on(level);
            log->set_pattern("[%T.%e] [%=5t] [%L] %v");
            logger::set_default_logger(log);
        }
    } catch (const logger::spdlog_ex& error) {
        logger::warn("Could not start the file log: {}", error.what());
    }
}

namespace
{
    void LoadSettings()
    {
        const auto result = HideQuestItems::Settings::Load();
        if (result == HideQuestItems::Settings::OperationResult::kFileNotFound) {
            if (HideQuestItems::Settings::Save() != HideQuestItems::Settings::OperationResult::kSuccess) {
                logger::warn("Could not create the default settings file");
            }
        } else if (result != HideQuestItems::Settings::OperationResult::kSuccess) {
            logger::warn("Could not load settings; defaults remain active");
        }
    }

    void MessageCallback(SFSE::MessagingInterface::Message* a_message)
    {
        switch (a_message->type) {
        case SFSE::MessagingInterface::kPostLoad:
            LoadSettings();
            HideQuestItems::Menu::Register();
            break;
        case SFSE::MessagingInterface::kPostDataLoad:
            if (!HideQuestItems::Hooks::Install()) {
                logger::critical("Could not install menu hooks");
            }
            break;
        default:
            break;
        }
    }
}

extern "C"
{
    __declspec(dllexport) constinit SFSE::PluginVersionData SFSEPlugin_Version{
        1,
        PLUGIN_VERSION_PACKED,
        PLUGIN_NAME,
        PLUGIN_AUTHOR,
        1U << 2,
        1U << 3,
        { SFSE::RUNTIME_SF_1_16_244 },
        0,
        0,
        0
    };
}

extern "C" __declspec(dllexport) bool SFSEPlugin_Load(const SFSE::LoadInterface* a_sfse)
{
    if (!a_sfse) {
        return false;
    }

    HideQuestItems::InitializeLog();
    if (a_sfse->RuntimeVersion() != SFSE::RUNTIME_SF_1_16_244) {
        logger::critical("Unsupported Starfield runtime {}", a_sfse->RuntimeVersion());
        return false;
    }

    if (!SFSE::Init(*a_sfse, 64)) {
        logger::critical("Could not initialize SFSE");
        return false;
    }
    const auto messaging = SFSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(MessageCallback)) {
        logger::critical("Could not register the SFSE message listener");
        return false;
    }

    return true;
}
