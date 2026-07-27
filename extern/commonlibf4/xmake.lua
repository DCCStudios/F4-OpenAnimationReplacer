-- include subprojects
includes("lib/commonlibf4")

-- set project constants
set_project("commonlibf4-template")
set_version("0.1.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

add_defines("COMMONLIB_RUNTIMECOUNT=3")

-- define targets
target("commonlibf4-template")
    add_rules("commonlibf4.plugin", {
        name = "commonlibf4-template",
        author = "",
        description = "",
        plugin_template = path.join(os.projectdir(), "res/commonlibf4-plugin.cpp.in"),
    })

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
