#include "hooks/PlayerInventoryHook.h"

#include "runtime/QuestItemVisibility.h"
#include "settings/Settings.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace HideQuestItems::Hooks::PlayerInventory
{
    namespace
    {
        using PublishItem = void (*)(void*, const RE::BGSInventoryItem*);

        constexpr REL::ID kPublishItem{ 88084 };
        constexpr REL::ID kInitialPublishCaller{ 88102 };
        constexpr REL::ID kUpdatePublishCaller{ 88119 };
        constexpr std::ptrdiff_t kInitialPublishOffset = 0x1D7;
        constexpr std::ptrdiff_t kUpdatePublishOffset = 0x1B0;

        constexpr std::array<std::uint8_t, 5> kInitialPublishCall{
            0xE8, 0x64, 0xE2, 0xFF, 0xFF
        };
        constexpr std::array<std::uint8_t, 5> kUpdatePublishCall{
            0xE8, 0x6B, 0xCE, 0xFF, 0xFF
        };

        struct PublishContext
        {
            void*                          model;
            RE::InventoryInterface::Handle handle;
        };
        static_assert(offsetof(PublishContext, handle) == 0x8);

        PublishItem originalPublishItem{};

        void QueueRowRemoval(const RE::InventoryInterface::Handle& a_handle)
        {
            const auto* tasks = SFSE::GetTaskInterface();
            if (!tasks) {
                logger::error("Could not queue player inventory row removal");
                return;
            }

            tasks->AddTask([a_handle] {
                if (!Settings::GetPlayerInventoryMenuEnabled()) {
                    return;
                }

                if (!RE::GameUIModel::QueueRemovePlayerInventoryItem(a_handle)) {
                    logger::debug("Player inventory data model is not available for row removal");
                }
            });
        }

        void PublishItemThunk(void* a_context, const RE::BGSInventoryItem* a_item)
        {
            if (a_item && Settings::GetPlayerInventoryMenuEnabled() &&
                Runtime::ShouldHide(*a_item)) {
                if (a_context) {
                    QueueRowRemoval(static_cast<const PublishContext*>(a_context)->handle);
                }
                return;
            }

            originalPublishItem(a_context, a_item);
        }

        [[nodiscard]] bool HasExpectedCall(
            std::uintptr_t a_site,
            const std::array<std::uint8_t, 5>& a_bytes) noexcept
        {
            return std::memcmp(
                       reinterpret_cast<const void*>(a_site),
                       a_bytes.data(),
                       a_bytes.size()) == 0 &&
                   SF1::Hooks::CallTarget(a_site) == reinterpret_cast<std::uintptr_t>(originalPublishItem);
        }

        [[nodiscard]] bool RestoreCalls(
            std::uintptr_t a_initialSite,
            std::uintptr_t a_updateSite) noexcept
        {
            const auto initialRestored = SF1::Hooks::WriteCode(a_initialSite, kInitialPublishCall);
            const auto updateRestored = SF1::Hooks::WriteCode(a_updateSite, kUpdatePublishCall);
            return initialRestored && updateRestored &&
                   HasExpectedCall(a_initialSite, kInitialPublishCall) &&
                   HasExpectedCall(a_updateSite, kUpdatePublishCall);
        }
    }

    bool Install()
    {
        originalPublishItem = SF1::At<PublishItem>(kPublishItem);
        const auto initialSite = SF1::ResolveID(kInitialPublishCaller.value) + kInitialPublishOffset;
        const auto updateSite = SF1::ResolveID(kUpdatePublishCaller.value) + kUpdatePublishOffset;

        if (!HasExpectedCall(initialSite, kInitialPublishCall) ||
            !HasExpectedCall(updateSite, kUpdatePublishCall)) {
            logger::critical("Player inventory hook preflight failed for Starfield 1.16.244");
            return false;
        }

        const auto relay = SF1::Hooks::AllocateCallRelay(REL::GetTrampoline(), reinterpret_cast<std::uintptr_t>(&PublishItemThunk));
        if (!SF1::Hooks::CanReachCall(initialSite, relay) || !SF1::Hooks::CanReachCall(updateSite, relay)) {
            logger::critical("Player inventory hook relay is unavailable");
            return false;
        }
        if (!SF1::Hooks::WriteCall(initialSite, relay) || !SF1::Hooks::WriteCall(updateSite, relay)) {
            const auto rollbackVerified = RestoreCalls(initialSite, updateSite);
            logger::critical(
                "Player inventory hook verification failed; rollback verified: {}",
                rollbackVerified);
            return false;
        }

        logger::info("Installed player inventory data filter");
        return true;
    }

    void Refresh()
    {
        if (!RE::GameUIModel::ReconcilePlayerInventory(false)) {
            logger::debug("Player inventory data model is not available for refresh");
        }
    }
}
