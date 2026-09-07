#pragma once

#include <cstdint>

namespace HideQuestItems::Runtime
{
    enum class MenuOwner : std::uint8_t
    {
        kContainer = 1U << 0U,
        kPlayerInventory = 1U << 1U
    };

    void OpenOrRefresh(MenuOwner a_owner);
    void Close(MenuOwner a_owner);

    void RefreshSettings();
}
