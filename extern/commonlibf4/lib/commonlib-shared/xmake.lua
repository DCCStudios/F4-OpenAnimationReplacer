-- set minimum xmake version
set_xmakever("3.0.0")

-- set project constants
set_project("commonlib-shared")
set_arch("x64")
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- add options
option("commonlib_ini", function()
    set_default(false)
    set_description("enable REX::INI settings support")
end)

option("commonlib_json", function()
    set_default(false)
    set_description("enable REX::JSON settings support")
end)

option("commonlib_toml", function()
    set_default(false)
    set_description("enable REX::TOML settings support")
end)

option("commonlib_xbyak", function()
    set_default(false)
    set_description("enable xbyak support for Trampoline")
end)

-- add packages
add_requires("spdlog v1.16.0", { configs = { header_only = false, wchar = true, std_format = true } })

-- add config packages
if has_config("commonlib_ini") then add_requires("simpleini v4.25") end
if has_config("commonlib_json") then add_requires("glaze v7.0.0") end
if has_config("commonlib_toml") then add_requires("toml11 v4.4.0") end
if has_config("commonlib_xbyak") then add_requires("xbyak v7.06") end

target("commonlib-shared", function()
    -- set target kind
    set_kind("static")

    -- set build by default
    set_default(os.scriptdir() == os.projectdir())

    -- add packages
    add_packages("spdlog", { public = true })

    -- add config packages
    if has_config("commonlib_ini") then
        add_packages("simpleini", { public = true })
        add_defines("COMMONLIB_OPTION_INI=1", { public = true })
    end

    if has_config("commonlib_json") then
        add_packages("glaze", { public = true })
        add_defines("COMMONLIB_OPTION_JSON=1", { public = true })
    end

    if has_config("commonlib_toml") then
        add_packages("toml11", { public = true })
        add_defines("COMMONLIB_OPTION_TOML=1", { public = true })
    end

    if has_config("commonlib_xbyak") then
        add_packages("xbyak", { public = true })
        add_defines("COMMONLIB_OPTION_XBYAK=1", { public = true })
    end

    -- add options
    add_options("commonlib_ini", "commonlib_json", "commonlib_toml", "commonlib_xbyak", { public = true })

    -- add system links
    add_syslinks("advapi32", "bcrypt", "d3d11", "d3dcompiler", "dbghelp", "dxgi", "ole32", "shell32", "user32", "version", "ws2_32")

    -- add source files
    add_files("src/**.cpp")

    -- add header files
    add_includedirs("include", { public = true })
    add_headerfiles(
        "include/(REL/**.h)",
        "include/(REX/**.h)"
    )

    -- set precompiled header
    set_pcxxheader("src/REX/PCH.h")

    -- add flags (public)
    add_cxxflags(
        "/EHsc",
        "/permissive-",
        { public = true }
    )

    -- add flags (cl) (public)
    add_cxxflags(
        "cl::/bigobj",
        "cl::/cgthreads8",
        "cl::/diagnostics:caret",
        "cl::/external:W0",
        "cl::/fp:contract",
        "cl::/fp:except-",
        "cl::/guard:cf-",
        "cl::/Zc:enumTypes",
        "cl::/Zc:preprocessor",
        "cl::/Zc:templateScope",
        { public = true }
    )

    -- add flags (cl: disable warnings) (public)
    add_cxxflags(
        "cl::/wd4200", -- nonstandard extension used : zero-sized array in struct/union
        "cl::/wd4201", -- nonstandard extension used : nameless struct/union
        "cl::/wd4324", -- structure was padded due to alignment specifier
        { public = true }
    )

    -- add flags (cl: warnings -> errors) (public)
    add_cxxflags(
        "cl::/we4715", -- not all control paths return a value
        { public = true }
    )

    -- add flags (clang-cl) (public)
    add_cxxflags(
        "clang_cl::-fms-compatibility",
        "clang_cl::-fms-extensions",
        { public = true }
    )

    -- add flags (clang-cl: disable warnings) (public)
    add_cxxflags(
        "clang_cl::-Wno-delete-non-abstract-non-virtual-dtor",
        "clang_cl::-Wno-deprecated-volatile",
        "clang_cl::-Wno-ignored-qualifiers",
        "clang_cl::-Wno-inconsistent-missing-override",
        "clang_cl::-Wno-invalid-offsetof",
        "clang_cl::-Wno-microsoft-include",
        "clang_cl::-Wno-overloaded-virtual",
        "clang_cl::-Wno-pragma-system-header-outside-header",
        "clang_cl::-Wno-reinterpret-base-class",
        "clang_cl::-Wno-switch",
        "clang_cl::-Wno-unused-local-typedef",
        "clang_cl::-Wno-unused-private-field",
        { public = true }
    )
end)

rule("commonlib.plugin", function()
    add_deps("win.sdk.resource")

    on_config(function(target)
        import("core.project.project")
        import("core.base.semver")

        target:set("arch", "x64")
        target:set("kind", "shared")

        target:set("configdir", target:autogendir())
        target:add("configfiles", path.join(os.scriptdir(), "res/commonlib-plugin.rc.in"))
        target:add("files", path.join(target:configdir(), "commonlib-plugin.rc"))

        local data = target:data("commonlib.plugin.config") or {}
        target:set("configvar", "COMMONLIB_PLUGIN_AUTHOR", data.author or "")
        target:set("configvar", "COMMONLIB_PLUGIN_CONTACT", data.contact or "")
        target:set("configvar", "COMMONLIB_PLUGIN_DESCRIPTION", data.description or "")
        target:set("configvar", "COMMONLIB_PLUGIN_LICENSE", (target:license() or "Unknown") .. " License")
        target:set("configvar", "COMMONLIB_PLUGIN_NAME", data.name or target:name())
        target:set("configvar", "COMMONLIB_PLUGIN_VERSION", target:version() or "0.0.0")
        target:set("configvar", "COMMONLIB_PLUGIN_VERSION_MAJOR", semver.new(target:version() or "0.0.0"):major())
        target:set("configvar", "COMMONLIB_PLUGIN_VERSION_MINOR", semver.new(target:version() or "0.0.0"):minor())
        target:set("configvar", "COMMONLIB_PLUGIN_VERSION_PATCH", semver.new(target:version() or "0.0.0"):patch())
        target:set("configvar", "COMMONLIB_PROJECT_NAME", project.name() or "")
        target:set("configvar", "COMMONLIB_PROJECT_VERSION", project.version() or "0.0.0")
        target:set("configvar", "COMMONLIB_PROJECT_VERSION_MAJOR", semver.new(project.version() or "0.0.0"):major())
        target:set("configvar", "COMMONLIB_PROJECT_VERSION_MINOR", semver.new(project.version() or "0.0.0"):minor())
        target:set("configvar", "COMMONLIB_PROJECT_VERSION_PATCH", semver.new(project.version() or "0.0.0"):patch())
    end)

    on_install(function(target)
        import("target.action.install")(target, { binaries = false, headers = false, libraries = false, packages = false })
    end)

    on_package(function(target)
        import("core.project.config")
        import("core.project.project")

        local archivename = target:name() .. "-" .. (target:version() or "0.0.0") .. ".zip"
        cprint("${dim}packaging %s .. ", archivename)

        local rootdir = path.join(os.tmpdir(), "packages", project.name() or "", target:name())
        os.tryrm(rootdir)

        local data = target:data("commonlib.plugin.package") or {}
        local installdir = path.join(rootdir, data.prefixdir or "")
        os.mkdir(installdir)

        local srcfiles, dstfiles = target:installfiles(installdir)
        if srcfiles and #srcfiles > 0 and dstfiles and #dstfiles > 0 then
            for idx, srcfile in ipairs(srcfiles) do
                os.trycp(srcfile, dstfiles[idx])
            end
        else
            return
        end

        local archivedir = path.absolute(path.join(config.builddir(), "packages"))
        local archivefile = path.join(archivedir, archivename)
        os.tryrm(archivefile)

        local olddir = os.cd(rootdir)
        local archivefiles = os.files("**")
        os.cd(olddir)

        import("utils.archive").archive(archivefile, archivefiles, { curdir = rootdir })

        cprint("${dim}packaging %s to %s ... ${color.success}${text.success}", archivename, archivedir)
    end)

    after_build(function(target)
        import("core.project.depend")
        import("core.project.task")

        depend.on_changed(function()
            local srcfiles, dstfiles = target:installfiles()
            if srcfiles and #srcfiles > 0 and dstfiles and #dstfiles > 0 then
                task.run("install")
            end
        end, { changed = target:is_rebuilt(), files = { target:targetfile() } })
    end)
end)
