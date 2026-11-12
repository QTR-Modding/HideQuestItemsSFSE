#include "hooks/MenuHooks.h"

#include "hooks/PlayerInventoryHook.h"
#include "runtime/QuestItemVisibility.h"

#include <cstdint>

namespace HideQuestItems::Hooks
{
    namespace
    {
        constexpr std::size_t kProcessMessageSlot = 0x08;

        class ContainerMenuHook
        {
        public:
            [[nodiscard]] static bool Install()
            {
                const auto table = SF1::ResolveID(RE::VTABLE::ContainerMenu.value);
                return SF1::Hooks::HookVTable(table, kProcessMessageSlot, Thunk, original);
            }

        private:
            static RE::UI_MESSAGE_RESULT Thunk(
                RE::IMenu* a_menu,
                RE::UIMessageData& a_message)
            {
                switch (a_message.Type()) {
                case RE::UI_MESSAGE_TYPE::kShow:
                case RE::UI_MESSAGE_TYPE::kUpdate:
                    Runtime::OpenContainerMenu();
                    break;
                case RE::UI_MESSAGE_TYPE::kHide:
                case RE::UI_MESSAGE_TYPE::kForceHide:
                    Runtime::CloseContainerMenu();
                    break;
                default:
                    break;
                }

                return original(a_menu, a_message);
            }

            using ProcessMessage = RE::UI_MESSAGE_RESULT (*)(RE::IMenu*, RE::UIMessageData&);
            static inline ProcessMessage original{};
        };
    }

    bool Install()
    {
        if (!PlayerInventory::Install() || !ContainerMenuHook::Install()) {
            return false;
        }

        logger::info("Installed inventory and container menu hooks");
        return true;
    }

    void RefreshSettings()
    {
        const auto* tasks = SFSE::GetTaskInterface();
        if (!tasks) {
            logger::error("Could not queue the settings refresh on the game thread");
            return;
        }

        tasks->AddTask([] {
            Runtime::RefreshContainerSettings();
            PlayerInventory::Refresh();
        });
    }
}
