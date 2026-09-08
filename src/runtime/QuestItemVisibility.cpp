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
        using FormSet = std::unordered_set<RE::TESFormID>;

        constexpr std::uint8_t kNotFavorited = 0xFE;

        struct HiddenForm
        {
            bool originalPlayable;
        };

        std::mutex visibilityMutex;
        std::unordered_map<RE::TESFormID, HiddenForm> hiddenForms;
        bool containerMenuOpen = false;

        [[nodiscard]] FormSet CollectEligibleForms()
        {
            FormSet result;
            const auto player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return result;
            }

            player->ForEachInventoryItem([&result](const RE::BGSInventoryItem& a_item) {
                if (ShouldHide(a_item)) {
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

        void RestoreAll()
        {
            for (const auto& [formID, hidden] : hiddenForms) {
                RestoreForm(formID, hidden);
            }
            hiddenForms.clear();
        }

        void Apply(const FormSet& a_forms)
        {
            for (auto iter = hiddenForms.begin(); iter != hiddenForms.end();) {
                if (!a_forms.contains(iter->first)) {
                    RestoreForm(iter->first, iter->second);
                    iter = hiddenForms.erase(iter);
                } else {
                    ++iter;
                }
            }

            for (const auto formID : a_forms) {
                if (hiddenForms.contains(formID)) {
                    continue;
                }

                const auto form = RE::TESForm::LookupByID(formID);
                if (!form || !form->GetPlayable()) {
                    continue;
                }

                hiddenForms.emplace(formID, HiddenForm{ form->GetPlayable() });
                form->SetPlayable(false);
            }
        }
    }

    bool ShouldHide(const RE::BGSInventoryItem& a_item) noexcept
    {
        if (!a_item.object || a_item.IsEquipped() ||
            static_cast<std::uint8_t>(a_item.unk24) != kNotFavorited) {
            return false;
        }

        bool hasPositiveCount = false;
        for (const auto& stack : a_item.stacks) {
            if (stack.count == 0) {
                continue;
            }

            hasPositiveCount = true;
            if (const auto extra = stack.extra.get();
                extra && extra->HasType(RE::ExtraDataType::kLeveledItem)) {
                return false;
            }
        }

        return hasPositiveCount && a_item.IsQuestObject();
    }

    void OpenContainerMenu()
    {
        const std::scoped_lock lock(visibilityMutex);
        containerMenuOpen = true;
        if (Settings::GetContainerMenuEnabled()) {
            Apply(CollectEligibleForms());
        } else {
            RestoreAll();
        }
    }

    void CloseContainerMenu()
    {
        const std::scoped_lock lock(visibilityMutex);
        containerMenuOpen = false;
        RestoreAll();
    }

    void RefreshContainerSettings()
    {
        const std::scoped_lock lock(visibilityMutex);
        if (containerMenuOpen && Settings::GetContainerMenuEnabled()) {
            Apply(CollectEligibleForms());
        } else {
            RestoreAll();
        }
    }
}
