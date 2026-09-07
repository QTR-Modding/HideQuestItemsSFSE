#include "hooks/MenuHooks.h"
#include "menu/Menu.h"
#include "settings/Settings.h"

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

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
    if (!a_sfse) {
        return false;
    }

    SFSE::Init(a_sfse, { .trampoline = true, .trampolineSize = 64 });
    if (a_sfse->RuntimeVersion() != SFSE::RUNTIME_SF_1_16_244) {
        logger::critical("Unsupported Starfield runtime {}", a_sfse->RuntimeVersion());
        return false;
    }

    const auto* messaging = SFSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(MessageCallback)) {
        logger::critical("Could not register the SFSE message listener");
        return false;
    }

    return true;
}
