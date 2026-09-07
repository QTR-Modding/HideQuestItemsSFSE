#include "Settings.h"

#include <Windows.h>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>

namespace HideQuestItems::Settings
{
    namespace
    {
        constexpr std::uint8_t kContainerMenu = 1U << 0U;
        constexpr std::uint8_t kPlayerInventoryMenu = 1U << 1U;
        constexpr std::uint8_t kAllMenus = kContainerMenu | kPlayerInventoryMenu;

        std::atomic_uint8_t menuFlags{kAllMenus};
        std::mutex settingsFileMutex;

        [[nodiscard]] std::optional<std::filesystem::path> GetSettingsPath()
        {
            std::array<wchar_t, 32768> executable{};
            const auto length = ::GetModuleFileNameW(
                nullptr,
                executable.data(),
                static_cast<DWORD>(executable.size()));
            if (length == 0 || length >= executable.size()) {
                return std::nullopt;
            }

            auto gameRoot = std::filesystem::path(executable.data(), executable.data() + length).parent_path();
            if (!gameRoot.is_absolute()) {
                return std::nullopt;
            }

            return gameRoot / L"Data" / L"SFSE" / L"Plugins" / L"HideQuestItemsSFSE" / L"Settings.json";
        }

        [[nodiscard]] bool ReadFeature(
            const rapidjson::Value& a_menus,
            const char* a_name,
            bool& a_enabled)
        {
            const auto feature = a_menus.FindMember(a_name);
            if (feature == a_menus.MemberEnd() || !feature->value.IsObject()) {
                return false;
            }

            const auto enabled = feature->value.FindMember("enabled");
            if (enabled == feature->value.MemberEnd() || !enabled->value.IsBool()) {
                return false;
            }

            a_enabled = enabled->value.GetBool();
            return true;
        }

        void AddFeature(
            rapidjson::Value& a_menus,
            const char* a_name,
            bool a_enabled,
            rapidjson::Document::AllocatorType& a_allocator)
        {
            rapidjson::Value feature(rapidjson::kObjectType);
            feature.AddMember("enabled", a_enabled, a_allocator);
            a_menus.AddMember(rapidjson::StringRef(a_name), feature, a_allocator);
        }

        void RemoveTemporaryFile(const std::filesystem::path& a_path) noexcept
        {
            std::error_code ignored;
            std::filesystem::remove(a_path, ignored);
        }
    }

    Values Get() noexcept
    {
        const auto flags = menuFlags.load(std::memory_order_relaxed);
        return {
            .containerMenu = (flags & kContainerMenu) != 0,
            .playerInventoryMenu = (flags & kPlayerInventoryMenu) != 0
        };
    }

    void Set(const Values& a_values) noexcept
    {
        std::uint8_t flags = 0;
        if (a_values.containerMenu) {
            flags |= kContainerMenu;
        }
        if (a_values.playerInventoryMenu) {
            flags |= kPlayerInventoryMenu;
        }
        menuFlags.store(flags, std::memory_order_relaxed);
    }

    bool GetContainerMenuEnabled() noexcept
    {
        return (menuFlags.load(std::memory_order_relaxed) & kContainerMenu) != 0;
    }

    bool GetPlayerInventoryMenuEnabled() noexcept
    {
        return (menuFlags.load(std::memory_order_relaxed) & kPlayerInventoryMenu) != 0;
    }

    void SetContainerMenuEnabled(bool a_enabled) noexcept
    {
        if (a_enabled) {
            menuFlags.fetch_or(kContainerMenu, std::memory_order_relaxed);
        } else {
            menuFlags.fetch_and(static_cast<std::uint8_t>(~kContainerMenu), std::memory_order_relaxed);
        }
    }

    void SetPlayerInventoryMenuEnabled(bool a_enabled) noexcept
    {
        if (a_enabled) {
            menuFlags.fetch_or(kPlayerInventoryMenu, std::memory_order_relaxed);
        } else {
            menuFlags.fetch_and(static_cast<std::uint8_t>(~kPlayerInventoryMenu), std::memory_order_relaxed);
        }
    }

    OperationResult Load()
    {
        const std::scoped_lock lock(settingsFileMutex);
        const auto path = GetSettingsPath();
        if (!path) {
            logger::error("Could not resolve the settings path");
            return OperationResult::kPathUnavailable;
        }

        std::error_code fileError;
        const auto exists = std::filesystem::exists(*path, fileError);
        if (fileError) {
            logger::error("Could not inspect {}: {}", path->string(), fileError.message());
            return OperationResult::kReadFailed;
        }
        if (!exists) {
            return OperationResult::kFileNotFound;
        }

        std::ifstream input(*path, std::ios::binary);
        if (!input) {
            logger::error("Could not open {} for reading", path->string());
            return OperationResult::kOpenFailed;
        }

        rapidjson::IStreamWrapper stream(input);
        rapidjson::Document document;
        document.ParseStream(stream);
        if (input.bad()) {
            logger::error("Could not read {}", path->string());
            return OperationResult::kReadFailed;
        }
        if (document.HasParseError() || !document.IsObject()) {
            logger::error("Invalid JSON in {}", path->string());
            return OperationResult::kInvalidJson;
        }

        const auto menus = document.FindMember("menus");
        if (menus == document.MemberEnd() || !menus->value.IsObject()) {
            logger::error("Missing or invalid menus object in {}", path->string());
            return OperationResult::kInvalidSchema;
        }

        Values loaded;
        if (!ReadFeature(menus->value, "container_menu", loaded.containerMenu) ||
            !ReadFeature(menus->value, "player_inventory_menu", loaded.playerInventoryMenu)) {
            logger::error("Missing or invalid menu settings in {}", path->string());
            return OperationResult::kInvalidSchema;
        }

        Set(loaded);
        return OperationResult::kSuccess;
    }

    OperationResult Save()
    {
        const std::scoped_lock lock(settingsFileMutex);
        const auto path = GetSettingsPath();
        if (!path) {
            logger::error("Could not resolve the settings path");
            return OperationResult::kPathUnavailable;
        }

        std::error_code directoryError;
        std::filesystem::create_directories(path->parent_path(), directoryError);
        if (directoryError) {
            logger::error("Could not create {}: {}", path->parent_path().string(), directoryError.message());
            return OperationResult::kDirectoryCreationFailed;
        }

        const auto values = Get();
        rapidjson::Document document(rapidjson::kObjectType);
        auto& allocator = document.GetAllocator();

        rapidjson::Value version(rapidjson::kObjectType);
        version.AddMember("major", HIDEQUESTITEMS_VERSION_MAJOR, allocator);
        version.AddMember("minor", HIDEQUESTITEMS_VERSION_MINOR, allocator);
        version.AddMember("patch", HIDEQUESTITEMS_VERSION_PATCH, allocator);
        version.AddMember("build", HIDEQUESTITEMS_VERSION_BUILD, allocator);
        document.AddMember("plugin_version", version, allocator);

        rapidjson::Value menus(rapidjson::kObjectType);
        AddFeature(menus, "container_menu", values.containerMenu, allocator);
        AddFeature(menus, "player_inventory_menu", values.playerInventoryMenu, allocator);
        document.AddMember("menus", menus, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        if (!document.Accept(writer)) {
            logger::error("Could not serialize settings");
            return OperationResult::kWriteFailed;
        }

        auto temporary = *path;
        temporary += L".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            logger::error("Could not open {} for writing", temporary.string());
            return OperationResult::kOpenFailed;
        }

        output.write(buffer.GetString(), static_cast<std::streamsize>(buffer.GetSize()));
        output.put('\n');
        output.close();
        if (!output) {
            RemoveTemporaryFile(temporary);
            logger::error("Could not write {}", temporary.string());
            return OperationResult::kWriteFailed;
        }

        if (!::MoveFileExW(
                temporary.c_str(),
                path->c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            const auto error = ::GetLastError();
            RemoveTemporaryFile(temporary);
            logger::error("Could not replace {} (Win32 error {})", path->string(), error);
            return OperationResult::kReplaceFailed;
        }

        return OperationResult::kSuccess;
    }

    void Reset() noexcept
    {
        menuFlags.store(kAllMenus, std::memory_order_relaxed);
    }
}
