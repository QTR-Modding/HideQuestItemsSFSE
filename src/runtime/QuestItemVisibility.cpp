#include "runtime/QuestItemVisibility.h"

#include "settings/Settings.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace HideQuestItems::Runtime
{
    namespace
    {
        using OwnerBits = std::uint8_t;
        using FormSet = std::unordered_set<RE::TESFormID>;

        struct HiddenForm
        {
            bool originalPlayable;
            OwnerBits owners;
        };

        std::mutex visibilityMutex;
        std::unordered_map<RE::TESFormID, HiddenForm> hiddenForms;
        OwnerBits openOwners = 0;

        [[nodiscard]] constexpr OwnerBits ToBits(MenuOwner a_owner) noexcept
        {
            return static_cast<OwnerBits>(a_owner);
        }

        [[nodiscard]] bool IsEnabled(MenuOwner a_owner) noexcept
        {
            switch (a_owner) {
            case MenuOwner::kContainer:
                return Settings::GetContainerMenuEnabled();
            case MenuOwner::kPlayerInventory:
                return Settings::GetPlayerInventoryMenuEnabled();
            }

            return false;
        }

        [[nodiscard]] FormSet CollectEligibleForms()
        {
            FormSet result;
            const auto player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return result;
            }

            player->ForEachInventoryItem([&result](const RE::BGSInventoryItem& a_item) {
                if (!a_item.object || a_item.IsEquipped()) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                bool hasPositiveCount = false;
                bool allPositiveStacksAreQuestItems = true;
                bool hasExcludedStack = false;

                for (const auto& stack : a_item.stacks) {
                    if (stack.count == 0) {
                        continue;
                    }

                    hasPositiveCount = true;
                    const auto extra = stack.extra.get();
                    if (!extra) {
                        allPositiveStacksAreQuestItems = false;
                        continue;
                    }

                    allPositiveStacksAreQuestItems =
                        allPositiveStacksAreQuestItems && extra->HasQuestObjectAlias();
                    hasExcludedStack = hasExcludedStack ||
                                       extra->HasType(RE::ExtraDataType::kFavorite) ||
                                       extra->HasType(RE::ExtraDataType::kLeveledItem);
                }

                if (hasPositiveCount && allPositiveStacksAreQuestItems && !hasExcludedStack) {
                    result.insert(a_item.object->GetFormID());
                }

                return RE::BSContainer::ForEachResult::kContinue;
            });

            return result;
        }

        void RestoreForm(RE::TESFormID a_formID, const HiddenForm& a_hidden)
        {
            if (const auto form = RE::TESForm::LookupByID(a_formID);
                form && form->GetPlayable() != a_hidden.originalPlayable) {
                form->SetPlayable(a_hidden.originalPlayable);
            }
        }

        void ReleaseOwner(MenuOwner a_owner)
        {
            const auto owner = ToBits(a_owner);
            for (auto iter = hiddenForms.begin(); iter != hiddenForms.end();) {
                iter->second.owners &= static_cast<OwnerBits>(~owner);
                if (iter->second.owners == 0) {
                    RestoreForm(iter->first, iter->second);
                    iter = hiddenForms.erase(iter);
                } else {
                    ++iter;
                }
            }
        }

        void ApplyOwner(MenuOwner a_owner, const FormSet& a_forms)
        {
            const auto owner = ToBits(a_owner);

            for (auto iter = hiddenForms.begin(); iter != hiddenForms.end();) {
                if ((iter->second.owners & owner) != 0 && !a_forms.contains(iter->first)) {
                    iter->second.owners &= static_cast<OwnerBits>(~owner);
                    if (iter->second.owners == 0) {
                        RestoreForm(iter->first, iter->second);
                        iter = hiddenForms.erase(iter);
                        continue;
                    }
                }
                ++iter;
            }

            for (const auto formID : a_forms) {
                if (const auto existing = hiddenForms.find(formID); existing != hiddenForms.end()) {
                    existing->second.owners |= owner;
                    continue;
                }

                const auto form = RE::TESForm::LookupByID(formID);
                if (!form || !form->GetPlayable()) {
                    continue;
                }

                hiddenForms.emplace(formID, HiddenForm{ true, owner });
                form->SetPlayable(false);
            }
        }

    }

    void OpenOrRefresh(MenuOwner a_owner)
    {
        const std::scoped_lock lock(visibilityMutex);
        openOwners |= ToBits(a_owner);

        if (IsEnabled(a_owner)) {
            ApplyOwner(a_owner, CollectEligibleForms());
        } else {
            ReleaseOwner(a_owner);
        }
    }

    void Close(MenuOwner a_owner)
    {
        const std::scoped_lock lock(visibilityMutex);
        openOwners &= static_cast<OwnerBits>(~ToBits(a_owner));
        ReleaseOwner(a_owner);
    }

    void RefreshSettings()
    {
        const std::scoped_lock lock(visibilityMutex);
        const auto containerOpen = (openOwners & ToBits(MenuOwner::kContainer)) != 0;
        const auto inventoryOpen = (openOwners & ToBits(MenuOwner::kPlayerInventory)) != 0;
        const auto containerEnabled = containerOpen && IsEnabled(MenuOwner::kContainer);
        const auto inventoryEnabled = inventoryOpen && IsEnabled(MenuOwner::kPlayerInventory);

        FormSet eligible;
        if (containerEnabled || inventoryEnabled) {
            eligible = CollectEligibleForms();
        }

        if (containerEnabled) {
            ApplyOwner(MenuOwner::kContainer, eligible);
        } else {
            ReleaseOwner(MenuOwner::kContainer);
        }

        if (inventoryEnabled) {
            ApplyOwner(MenuOwner::kPlayerInventory, eligible);
        } else {
            ReleaseOwner(MenuOwner::kPlayerInventory);
        }
    }

}
