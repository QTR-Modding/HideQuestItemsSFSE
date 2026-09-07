#include "hooks/PlayerInventoryHook.h"

#include "runtime/QuestItemVisibility.h"
#include "settings/Settings.h"

#include "REL/ASM.h"
#include "REL/Utility.h"

#include <array>
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
        constexpr std::array<std::uint8_t, 6> kAbsoluteJump{
            0xFF, 0x25, 0x00, 0x00, 0x00, 0x00
        };

        REL::Relocation<PublishItem> originalPublishItem;

        void PublishItemThunk(void* a_context, const RE::BGSInventoryItem* a_item)
        {
            if (a_item && Settings::GetPlayerInventoryMenuEnabled() &&
                Runtime::ShouldHide(*a_item)) {
                return;
            }

            originalPublishItem(a_context, a_item);
        }

        [[nodiscard]] bool HasExpectedCall(
            const REL::Relocation<std::uintptr_t>& a_site,
            const std::array<std::uint8_t, 5>& a_bytes) noexcept
        {
            return std::memcmp(
                       reinterpret_cast<const void*>(a_site.address()),
                       a_bytes.data(),
                       a_bytes.size()) == 0 &&
                   REL::ASM::CALL5::TARGET(a_site.address()) == originalPublishItem.address();
        }

        [[nodiscard]] bool CallsThunk(
            const REL::Relocation<std::uintptr_t>& a_site) noexcept
        {
            const auto stubAddress = REL::ASM::CALL5::TARGET(a_site.address());
            const auto* stub = reinterpret_cast<const REL::ASM::JMP14*>(stubAddress);
            return std::memcmp(stub, kAbsoluteJump.data(), kAbsoluteJump.size()) == 0 &&
                   stub->addr == reinterpret_cast<std::uintptr_t>(&PublishItemThunk);
        }

        [[nodiscard]] bool RestoreCalls(
            const REL::Relocation<std::uintptr_t>& a_initialSite,
            const REL::Relocation<std::uintptr_t>& a_updateSite) noexcept
        {
            const auto initialRestored = REL::WriteSafe(
                a_initialSite.address(),
                kInitialPublishCall.data(),
                kInitialPublishCall.size());
            const auto updateRestored = REL::WriteSafe(
                a_updateSite.address(),
                kUpdatePublishCall.data(),
                kUpdatePublishCall.size());
            return initialRestored && updateRestored &&
                   HasExpectedCall(a_initialSite, kInitialPublishCall) &&
                   HasExpectedCall(a_updateSite, kUpdatePublishCall);
        }
    }

    bool Install()
    {
        originalPublishItem = kPublishItem;
        if (!originalPublishItem) {
            logger::critical("Could not resolve the player inventory row publisher");
            return false;
        }

        // Starfield 1.16.244 reaches the player row publisher through these two
        // sites: bulk reconciliation and retained/single-item updates.
        REL::Relocation<std::uintptr_t> initialSite{
            kInitialPublishCaller,
            kInitialPublishOffset
        };
        REL::Relocation<std::uintptr_t> updateSite{
            kUpdatePublishCaller,
            kUpdatePublishOffset
        };

        if (!HasExpectedCall(initialSite, kInitialPublishCall) ||
            !HasExpectedCall(updateSite, kUpdatePublishCall)) {
            logger::critical("Player inventory hook preflight failed for Starfield 1.16.244");
            return false;
        }

        const auto initialOriginal = initialSite.write_call<5>(PublishItemThunk);
        const auto updateOriginal = updateSite.write_call<5>(PublishItemThunk);
        if (initialOriginal != originalPublishItem.address() ||
            updateOriginal != originalPublishItem.address() ||
            !CallsThunk(initialSite) || !CallsThunk(updateSite)) {
            const auto rollbackVerified = RestoreCalls(initialSite, updateSite);
            logger::critical(
                "Player inventory hook verification failed; rollback verified: {}",
                rollbackVerified);
            return false;
        }

        logger::info("Installed player inventory data filter");
        return true;
    }
}
