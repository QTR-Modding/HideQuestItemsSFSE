#include "hooks/MenuHooks.h"

#include "runtime/QuestItemVisibility.h"

#include <cstdint>

namespace HideQuestItems::Hooks
{
    namespace
    {
        constexpr std::uint32_t kForceHideMessage = 4;
        constexpr std::size_t kProcessMessageSlot = 0x08;

        template <Runtime::MenuOwner Owner>
        class ProcessMessageHook
        {
        public:
            [[nodiscard]] static bool Install(REL::ID a_vtable)
            {
                REL::Relocation<std::uintptr_t> vtable{ a_vtable };
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
            static RE::UI_MESSAGE_RESULT Thunk(RE::IMenu* a_menu, RE::UIMessageData& a_message)
            {
                const auto type = static_cast<std::uint32_t>(a_message.type);
                switch (type) {
                case static_cast<std::uint32_t>(RE::UI_MESSAGE_TYPE::kShow):
                case static_cast<std::uint32_t>(RE::UI_MESSAGE_TYPE::kUpdate):
                    Runtime::OpenOrRefresh(Owner);
                    break;
                case static_cast<std::uint32_t>(RE::UI_MESSAGE_TYPE::kHide):
                case kForceHideMessage:
                    Runtime::Close(Owner);
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
        if (!ProcessMessageHook<Runtime::MenuOwner::kContainer>::Install(RE::VTABLE::ContainerMenu[0]) ||
            !ProcessMessageHook<Runtime::MenuOwner::kPlayerInventory>::Install(RE::VTABLE::InventoryMenu[11])) {
            return false;
        }

        logger::info("Installed ContainerMenu and InventoryMenu hooks");
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
            Runtime::RefreshSettings();
        });
    }
}
