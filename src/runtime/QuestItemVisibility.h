#pragma once

namespace RE
{
    struct BGSInventoryItem;
}

namespace HideQuestItems::Runtime
{
    [[nodiscard]] bool ShouldHide(const RE::BGSInventoryItem& a_item) noexcept;

    void OpenContainerMenu();
    void CloseContainerMenu();
    void RefreshContainerSettings();
}
