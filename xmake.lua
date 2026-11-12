set_xmakever("3.0.9")
set_policy("package.requires_lock", true)

local project_root = os.projectdir()

if is_plat("windows") then
    add_cxflags(
        "/Brepro",
        "/experimental:deterministic",
        '/d1trimfile:"' .. project_root .. '"',
        '/pathmap:"' .. project_root .. '"=.',
        {
            tools = "cl",
            force = true
        })
    add_shflags("/Brepro", "/PDBALTPATH:%_PDB%", {
        force = true
    })
end

local plugin = {
    name = "HideQuestItemsSFSE",
    version = {
        major = 1,
        minor = 0,
        patch = 1,
        build = 0
    },
    author = "Quantumyilmaz",
    description = "Hides quest items from the player inventory and container menus"
}

local plugin_version = string.format(
    "%d.%d.%d",
    plugin.version.major,
    plugin.version.minor,
    plugin.version.patch)

local sfsemcp = os.getenv("SFSEMCP_PATH") or "lib/sfse-mcp"
local staging_dir = path.join(project_root, "build", "staging")

option("sf1sdk_dir")
    set_default("../SF1SDK")
    set_showmenu(true)
    set_description("Directory containing the SF1SDK checkout")
option_end()

includes(get_config("sf1sdk_dir") or "../SF1SDK")
add_requires("spdlog v1.16.0", { configs = { header_only = false, wchar = true, std_format = true } })
add_requires("rapidjson 1.1.0")

set_project(plugin.name)
set_version(plugin_version)
set_license("GPL-3.0-or-later")
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")

local plugin_description = plugin.description
local major, minor, patch = plugin_version:match("^(%d+)%.(%d+)%.(%d+)$")
local packed_version = tonumber(major) * 16777216 + tonumber(minor) * 65536 + tonumber(patch) * 16

add_rules("mode.debug", "mode.releasedbg", "mode.release")
add_rules("plugin.vsxmake.autoupdate")

target(plugin.name)
    add_defines(
        "_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING",
        "NOMINMAX",
        "WIN32_LEAN_AND_MEAN",
        "HIDEQUESTITEMS_VERSION_MAJOR=" .. plugin.version.major,
        "HIDEQUESTITEMS_VERSION_MINOR=" .. plugin.version.minor,
        "HIDEQUESTITEMS_VERSION_PATCH=" .. plugin.version.patch,
        "HIDEQUESTITEMS_VERSION_BUILD=" .. plugin.version.build)
    set_kind("shared")
    add_rules("sf1sdk.plugin", { stage = true, symbols = false })
    add_deps("SF1SDK")
    add_packages("spdlog")
    add_files("src/Version.rc")
    add_syslinks("user32")
    add_defines(
        'PLUGIN_NAME="' .. plugin.name .. '"',
        'PLUGIN_AUTHOR="' .. plugin.author .. '"',
        'PLUGIN_DESCRIPTION="' .. plugin_description .. '"',
        'PLUGIN_VERSION_TEXT="' .. plugin_version .. '"',
        "PLUGIN_VERSION_MAJOR=" .. major,
        "PLUGIN_VERSION_MINOR=" .. minor,
        "PLUGIN_VERSION_PATCH=" .. patch,
        "PLUGIN_VERSION_PACKED=" .. packed_version
    )

    add_packages("rapidjson")
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src", path.join(sfsemcp, "include"), path.join(sfsemcp, "lib/clib-utils-qtr/include"))
    add_installfiles("COPYING", "EXCEPTIONS", "THIRD_PARTY_NOTICES.md", "LICENSES/*.txt")
    set_pcxxheader("src/PCH.h")

    set_installdir(staging_dir)
