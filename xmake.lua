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

-- CommonLibSF supplies the plugin rule; deployment remains an explicit step.
rule("commonlib.plugin", function()
    after_build(function() end)
end)

local plugin = {
    name = "HideQuestItemsSFSE",
    version = {
        major = 0,
        minor = 1,
        patch = 0,
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

local commonlibsf = os.getenv("COMMONLIBSF_PATH") or "lib/commonlibsf"
local sfsemcp = os.getenv("SFSEMCP_PATH") or "lib/sfse-mcp"
local staging_dir = path.join(project_root, "build", "staging")

includes(commonlibsf)
add_requires("rapidjson 1.1.0")

set_project(plugin.name)
set_version(plugin_version)
set_license("GPL-3.0-or-later")
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")

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
    add_rules("commonlibsf.plugin", {
        name = plugin.name,
        author = plugin.author,
        description = plugin.description,
        options = {
            address_library = true,
            sig_scanning = false,
            no_struct_use = false,
            layout_dependent = true
        }
    })

    add_packages("rapidjson")
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src", path.join(sfsemcp, "include"))
    add_installfiles("COPYING", "EXCEPTIONS", "THIRD_PARTY_NOTICES.md", "LICENSES/*.txt")
    set_pcxxheader("src/PCH.h")

    on_config(function(target)
        target:set("installdir", staging_dir)
        target:remove("installfiles", target:symbolfile())
    end)

    before_build(function(target)
        assert(path.absolute(target:installdir()) == path.absolute(staging_dir),
            "build output must remain in the project staging directory")
    end)
