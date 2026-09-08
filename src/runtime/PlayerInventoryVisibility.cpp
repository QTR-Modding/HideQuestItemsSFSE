#include "runtime/PlayerInventoryVisibility.h"

#include "runtime/QuestItemVisibility.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

namespace HideQuestItems::Runtime
{
    namespace
    {
        static_assert(
            offsetof(RE::TESAmmo, data) + offsetof(RE::AMMO_DATA, flags) == 0x210);
        static_assert(offsetof(RE::TESObjectWEAPInstanceData, WeaponFlags) == 0x48);
        static_assert(offsetof(RE::WeaponDataFlags, NonPlayable) == 0x10);

        struct Candidate
        {
            RE::TESBoundObject* object;
            RE::BSTSmartPointer<RE::TBO_InstanceData> instanceData;
        };

        struct FormState
        {
            void Restore() const noexcept
            {
                form->SetPlayable(originalPlayable);
                form->formFlags = originalFlags;
            }

            RE::TESForm* form;
            decltype(RE::TESForm::formFlags) originalFlags;
            bool originalPlayable;
        };

        struct AmmoState
        {
            void Restore() const noexcept { ammo->data.flags = originalFlags; }

            RE::TESAmmo* ammo;
            decltype(RE::AMMO_DATA::flags) originalFlags;
        };

        struct WeaponState
        {
            void Restore() const noexcept { *nonPlayable = originalValue; }

            std::uint8_t* nonPlayable;
            std::uint8_t originalValue;
        };

        using PlayableState = std::variant<FormState, AmmoState, WeaponState>;

        class ScopedPlayableOverride
        {
        public:
            ScopedPlayableOverride()
            {
                const auto player = RE::PlayerCharacter::GetSingleton();
                if (!player) {
                    return;
                }

                player->ForEachInventoryItem([this](const RE::BGSInventoryItem& a_item) {
                    if (ShouldHide(a_item) &&
                        a_item.object->GetPlayable(a_item.instanceData.get())) {
                        candidates.push_back({ a_item.object, a_item.instanceData });
                    }
                    return RE::BSContainer::ForEachResult::kContinue;
                });

                states.reserve(candidates.size());
                for (const auto& candidate : candidates) {
                    Hide(candidate);
                }
            }

            ScopedPlayableOverride(const ScopedPlayableOverride&) = delete;
            ScopedPlayableOverride(ScopedPlayableOverride&&) = delete;
            ScopedPlayableOverride& operator=(const ScopedPlayableOverride&) = delete;
            ScopedPlayableOverride& operator=(ScopedPlayableOverride&&) = delete;

            ~ScopedPlayableOverride()
            {
                for (auto iter = states.rbegin(); iter != states.rend(); ++iter) {
                    std::visit([](const auto& a_state) { a_state.Restore(); }, *iter);
                }
            }

        private:
            void Hide(const Candidate& a_candidate)
            {
                if (const auto ammo = a_candidate.object->As<RE::TESAmmo>()) {
                    HideAmmo(*ammo, a_candidate.instanceData.get());
                } else if (const auto weapon = a_candidate.object->As<RE::TESObjectWEAP>()) {
                    HideWeapon(*weapon, a_candidate.instanceData);
                } else {
                    HideForm(*a_candidate.object, a_candidate.instanceData.get());
                }
            }

            void HideForm(RE::TESForm& a_form, const RE::TBO_InstanceData* a_instanceData)
            {
                states.emplace_back(FormState{
                    .form = std::addressof(a_form),
                    .originalFlags = a_form.formFlags,
                    .originalPlayable = a_form.GetPlayable()
                });

                a_form.SetPlayable(false);
                if (a_form.GetPlayable(a_instanceData)) {
                    std::get<FormState>(states.back()).Restore();
                    states.pop_back();
                }
            }

            void HideAmmo(RE::TESAmmo& a_ammo, const RE::TBO_InstanceData* a_instanceData)
            {
                states.emplace_back(AmmoState{
                    .ammo = std::addressof(a_ammo),
                    .originalFlags = a_ammo.data.flags
                });

                a_ammo.data.flags.set(RE::AMMO_DATA::Flag::kNonPlayable);
                if (a_ammo.GetPlayable(a_instanceData)) {
                    std::get<AmmoState>(states.back()).Restore();
                    states.pop_back();
                }
            }

            void HideWeapon(
                RE::TESObjectWEAP& a_weapon,
                const RE::BSTSmartPointer<RE::TBO_InstanceData>& a_instanceData)
            {
                const auto weaponData = a_instanceData ?
                                            reinterpret_cast<RE::TESObjectWEAPInstanceData*>(
                                                a_instanceData.get()) :
                                            a_weapon.weaponData.get();
                if (!weaponData || !weaponData->WeaponFlags) {
                    return;
                }

                auto& nonPlayable = weaponData->WeaponFlags->NonPlayable;
                states.emplace_back(WeaponState{
                    .nonPlayable = std::addressof(nonPlayable),
                    .originalValue = nonPlayable
                });

                nonPlayable = 1;
                if (a_weapon.GetPlayable(a_instanceData.get())) {
                    std::get<WeaponState>(states.back()).Restore();
                    states.pop_back();
                }
            }

            std::vector<Candidate> candidates;
            std::vector<PlayableState> states;
        };
    }

    void ReconcileWithHiddenQuestItems(
        RE::PlayerInventoryDataModel* a_model,
        bool a_incremental,
        PlayerInventoryReconcile a_reconcile)
    {
        const ScopedPlayableOverride playableOverride;
        a_reconcile(a_model, a_incremental);
    }
}
