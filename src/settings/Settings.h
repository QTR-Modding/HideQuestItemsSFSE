#pragma once

namespace HideQuestItems::Settings
{
    struct Values
    {
        bool containerMenu = true;
        bool playerInventoryMenu = true;
    };

    enum class OperationResult
    {
        kSuccess,
        kPathUnavailable,
        kFileNotFound,
        kDirectoryCreationFailed,
        kOpenFailed,
        kReadFailed,
        kInvalidJson,
        kInvalidSchema,
        kWriteFailed,
        kReplaceFailed
    };

    [[nodiscard]] Values Get() noexcept;
    void Set(const Values& a_values) noexcept;

    [[nodiscard]] bool GetContainerMenuEnabled() noexcept;
    [[nodiscard]] bool GetPlayerInventoryMenuEnabled() noexcept;
    void SetContainerMenuEnabled(bool a_enabled) noexcept;
    void SetPlayerInventoryMenuEnabled(bool a_enabled) noexcept;

    [[nodiscard]] OperationResult Load();
    [[nodiscard]] OperationResult Save();
    void Reset() noexcept;
}
