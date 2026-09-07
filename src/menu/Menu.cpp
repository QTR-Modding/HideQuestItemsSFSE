#include "Menu.h"

#include "hooks/MenuHooks.h"
#include "settings/Settings.h"

#include <SFSEMCP/SFSEMenuFramework.hpp>

namespace HideQuestItems::Menu
{
    namespace
    {
        bool dirty = false;
        const char* status = nullptr;

        [[nodiscard]] const char* FailureMessage(Settings::OperationResult a_result)
        {
            switch (a_result) {
            case Settings::OperationResult::kPathUnavailable:
                return "Could not resolve the settings path.";
            case Settings::OperationResult::kFileNotFound:
                return "The settings file does not exist.";
            case Settings::OperationResult::kDirectoryCreationFailed:
                return "Could not create the settings directory.";
            case Settings::OperationResult::kOpenFailed:
                return "Could not open the settings file.";
            case Settings::OperationResult::kReadFailed:
                return "Could not read the settings file.";
            case Settings::OperationResult::kInvalidJson:
                return "The settings file is not valid JSON.";
            case Settings::OperationResult::kInvalidSchema:
                return "The settings file has invalid menu settings.";
            case Settings::OperationResult::kWriteFailed:
                return "Could not write the settings file.";
            case Settings::OperationResult::kReplaceFailed:
                return "Could not replace the settings file.";
            case Settings::OperationResult::kSuccess:
            default:
                return nullptr;
            }
        }

        void __stdcall RenderSettings()
        {
            if (ImGuiMCP::Button("Save Settings")) {
                const auto result = Settings::Save();
                status = result == Settings::OperationResult::kSuccess ?
                             "Settings saved." :
                             FailureMessage(result);
                if (result == Settings::OperationResult::kSuccess) {
                    dirty = false;
                }
            }

            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button("Load Settings")) {
                const auto result = Settings::Load();
                status = result == Settings::OperationResult::kSuccess ?
                             "Settings reloaded." :
                             FailureMessage(result);
                if (result == Settings::OperationResult::kSuccess) {
                    dirty = false;
                    Hooks::RefreshSettings();
                }
            }

            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button("Reset to defaults")) {
                Settings::Reset();
                Hooks::RefreshSettings();
                status = "Defaults restored.";
                dirty = true;
            }

            auto playerInventoryMenu = Settings::GetPlayerInventoryMenuEnabled();
            if (ImGuiMCP::Checkbox("Player Inventory Menu", &playerInventoryMenu)) {
                Settings::SetPlayerInventoryMenuEnabled(playerInventoryMenu);
                Hooks::RefreshSettings();
                dirty = true;
                status = nullptr;
            }

            auto containerMenu = Settings::GetContainerMenuEnabled();
            if (ImGuiMCP::Checkbox("Container Menus", &containerMenu)) {
                Settings::SetContainerMenuEnabled(containerMenu);
                Hooks::RefreshSettings();
                dirty = true;
                status = nullptr;
            }

            if (status) {
                ImGuiMCP::TextUnformatted(status);
            }
            if (dirty) {
                ImGuiMCP::TextDisabled("Changes are live but not saved.");
            }
        }
    }

    void Register()
    {
        if (!SFSEMenuFramework::IsInstalled()) {
            logger::warn("SFSE Menu Framework is not installed; the settings menu was not registered");
            return;
        }

        SFSEMenuFramework::SetSection("Hide Quest Items in Menus");
        SFSEMenuFramework::AddSectionItem("Settings", RenderSettings);
    }
}
