#include "hooks/PlayerInventoryHook.h"

#include "runtime/PlayerInventoryVisibility.h"
#include "settings/Settings.h"

#include "REL/ASM.h"
#include "REL/Utility.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace HideQuestItems::Hooks::PlayerInventory
{
    namespace
    {
        using Reconcile = Runtime::PlayerInventoryReconcile;
        using ProcessMessage = RE::UI_MESSAGE_RESULT (*)(RE::IMenu*, RE::UIMessageData&);

        constexpr std::uint32_t kForceHideMessage = 4;
        constexpr std::size_t kProcessMessageSlot = 0x08;
        constexpr std::size_t kReconcilePrologueSize = 5;

        constexpr std::array<std::uint8_t, kReconcilePrologueSize> kReconcilePrologue{
            0x48, 0x89, 0x5C, 0x24, 0x10
        };
        constexpr std::array<std::uint8_t, 6> kAbsoluteJump{
            0xFF, 0x25, 0x00, 0x00, 0x00, 0x00
        };

        std::atomic_bool inventoryMenuOpen = false;
        REL::Relocation<Reconcile> originalReconcile;
        REL::Relocation<ProcessMessage> originalProcessMessage;
        std::uintptr_t reconcileAddress = 0;
        std::uintptr_t processMessageSlotAddress = 0;
        std::uintptr_t processMessageAddress = 0;

        void QueueRefresh()
        {
            const auto* tasks = SFSE::GetTaskInterface();
            if (!tasks) {
                logger::error("Could not queue the player inventory refresh");
                return;
            }

            tasks->AddTask([] { Refresh(); });
        }

        void ReconcileThunk(RE::PlayerInventoryDataModel* a_model, bool a_incremental)
        {
            if (inventoryMenuOpen.load(std::memory_order_relaxed) &&
                Settings::GetPlayerInventoryMenuEnabled()) {
                Runtime::ReconcileWithHiddenQuestItems(
                    a_model,
                    a_incremental,
                    originalReconcile.get());
            } else {
                originalReconcile(a_model, a_incremental);
            }
        }

        RE::UI_MESSAGE_RESULT ProcessMessageThunk(
            RE::IMenu* a_menu,
            RE::UIMessageData& a_message)
        {
            bool refresh = false;
            switch (static_cast<std::uint32_t>(a_message.type)) {
            case static_cast<std::uint32_t>(RE::UI_MESSAGE_TYPE::kShow):
            case static_cast<std::uint32_t>(RE::UI_MESSAGE_TYPE::kUpdate):
                refresh = !inventoryMenuOpen.exchange(true, std::memory_order_relaxed);
                break;
            case static_cast<std::uint32_t>(RE::UI_MESSAGE_TYPE::kHide):
            case kForceHideMessage:
                refresh = inventoryMenuOpen.exchange(false, std::memory_order_relaxed);
                break;
            default:
                break;
            }

            const auto result = originalProcessMessage(a_menu, a_message);
            if (refresh) {
                QueueRefresh();
            }
            return result;
        }

        [[nodiscard]] bool HasExpectedReconcilePrologue() noexcept
        {
            return reconcileAddress != 0 &&
                   std::memcmp(
                       reinterpret_cast<const void*>(reconcileAddress),
                       kReconcilePrologue.data(),
                       kReconcilePrologue.size()) == 0;
        }

        [[nodiscard]] bool ReconcileCallsThunk() noexcept
        {
            if (reconcileAddress == 0 ||
                *reinterpret_cast<const std::uint8_t*>(reconcileAddress) != 0xE9) {
                return false;
            }

            const auto stubAddress = REL::ASM::JMP5::TARGET(reconcileAddress);
            const auto* stub = reinterpret_cast<const REL::ASM::JMP14*>(stubAddress);
            return std::memcmp(stub, kAbsoluteJump.data(), kAbsoluteJump.size()) == 0 &&
                   stub->addr == reinterpret_cast<std::uintptr_t>(&ReconcileThunk);
        }

        [[nodiscard]] bool RestoreReconcile() noexcept
        {
            return reconcileAddress != 0 &&
                   REL::WriteSafe(
                       reconcileAddress,
                       kReconcilePrologue.data(),
                       kReconcilePrologue.size()) &&
                   HasExpectedReconcilePrologue();
        }

        [[nodiscard]] bool InstallReconcile()
        {
            REL::Relocation<std::uintptr_t> target{
                RE::ID::PlayerInventoryDataModel::Reconcile
            };
            reconcileAddress = target.address();
            if (!HasExpectedReconcilePrologue()) {
                logger::critical("Player inventory reconcile preflight failed for Starfield 1.16.244");
                return false;
            }

            auto& trampoline = REL::GetTrampoline();
            auto* gateway = static_cast<std::uint8_t*>(
                trampoline.allocate(kReconcilePrologueSize + sizeof(REL::ASM::JMP14)));
            std::memcpy(gateway, kReconcilePrologue.data(), kReconcilePrologue.size());
            const REL::ASM::JMP14 returnJump{ reconcileAddress + kReconcilePrologueSize };
            std::memcpy(
                gateway + kReconcilePrologueSize,
                std::addressof(returnJump),
                sizeof(returnJump));
            originalReconcile = reinterpret_cast<std::uintptr_t>(gateway);

            target.write_jmp<kReconcilePrologueSize>(ReconcileThunk);
            if (!ReconcileCallsThunk()) {
                const auto rollbackVerified = RestoreReconcile();
                logger::critical(
                    "Player inventory reconcile hook verification failed; rollback verified: {}",
                    rollbackVerified);
                return false;
            }

            return true;
        }

        [[nodiscard]] bool ProcessMessageCallsThunk() noexcept
        {
            return processMessageSlotAddress != 0 &&
                   *reinterpret_cast<const std::uintptr_t*>(processMessageSlotAddress) ==
                       reinterpret_cast<std::uintptr_t>(&ProcessMessageThunk);
        }

        [[nodiscard]] bool RestoreProcessMessage() noexcept
        {
            return processMessageSlotAddress != 0 && processMessageAddress != 0 &&
                   REL::WriteSafeData(processMessageSlotAddress, processMessageAddress) &&
                   *reinterpret_cast<const std::uintptr_t*>(processMessageSlotAddress) ==
                       processMessageAddress;
        }

        [[nodiscard]] bool InstallProcessMessage()
        {
            REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE::InventoryMenu[11] };
            if (!vtable) {
                return false;
            }

            processMessageSlotAddress =
                vtable.address() + sizeof(void*) * kProcessMessageSlot;
            processMessageAddress =
                *reinterpret_cast<const std::uintptr_t*>(processMessageSlotAddress);
            if (processMessageAddress == 0) {
                return false;
            }

            originalProcessMessage = processMessageAddress;
            const auto thunkAddress = reinterpret_cast<std::uintptr_t>(&ProcessMessageThunk);
            if (!REL::WriteSafeData(processMessageSlotAddress, thunkAddress) ||
                !ProcessMessageCallsThunk()) {
                const auto rollbackVerified = RestoreProcessMessage();
                logger::critical(
                    "InventoryMenu lifecycle hook verification failed; rollback verified: {}",
                    rollbackVerified);
                return false;
            }

            return true;
        }
    }

    bool Install()
    {
        if (!InstallProcessMessage()) {
            logger::critical("Could not install the InventoryMenu lifecycle hook");
            return false;
        }

        if (!InstallReconcile()) {
            const auto rollbackVerified = RestoreProcessMessage();
            logger::critical(
                "Could not install the player inventory reconcile hook; lifecycle rollback verified: {}",
                rollbackVerified);
            return false;
        }

        logger::info("Installed player inventory visibility hooks");
        return true;
    }

    void Refresh()
    {
        if (!RE::GameUIModel::ReconcilePlayerInventory(false)) {
            logger::debug("Player inventory data model is not available for refresh");
        }
    }
}
