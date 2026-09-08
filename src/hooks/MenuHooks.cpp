#include "hooks/MenuHooks.h"

#include "hooks/PlayerInventoryHook.h"
#include "runtime/QuestItemVisibility.h"

#include <cstdint>

namespace HideQuestItems::Hooks
{
    namespace
    {
        constexpr std::uint32_t kForceHideMessage = 4;
        constexpr std::size_t kProcessMessageSlot = 0x08;

        class ContainerMenuHook
        {
        public:
            [[nodiscard]] static bool Install()
            {
                REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE::ContainerMenu[0] };
                if (!vtable) {
                    return false;
                }

                const auto current = *reinterpret_cast<const std::uintptr_t*>(
                    vtable.address() + sizeof(void*) * kProcessMessageSlot);
                if (current == 0) {
                    return false;
                }

                original = vtable.write_vfunc(kProcessMessageSlot, Thunk);
                return static_cast<bool>(original);
            }

        private:
            static RE::UI_MESSAGE_RESULT Thunk(
                RE::IMenu* a_menu,
                RE::UIMessageData& a_message)
            {
                switch (static_cast<std::uint32_t>(a_message.type)) {
                case static_cast<std::uint32_t>(RE::UI_MESSAGE_TYPE::kShow):
                case static_cast<std::uint32_t>(RE::UI_MESSAGE_TYPE::kUpdate):
                    Runtime::OpenContainerMenu();
                    break;
                case static_cast<std::uint32_t>(RE::UI_MESSAGE_TYPE::kHide):
                case kForceHideMessage:
                    Runtime::CloseContainerMenu();
                    break;
                default:
                    break;
                }

                return original(a_menu, a_message);
            }

            using ProcessMessage = RE::UI_MESSAGE_RESULT (*)(RE::IMenu*, RE::UIMessageData&);
            static inline REL::Relocation<ProcessMessage> original;
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
