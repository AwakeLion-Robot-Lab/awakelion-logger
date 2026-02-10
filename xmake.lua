--[[
░█████╗░░██╗░░░░░░░██╗░░░░░░██╗░░░░░░█████╗░░██████╗░░██████╗░███████╗██████╗░
██╔══██╗░██║░░██╗░░██║░░░░░░██║░░░░░██╔══██╗██╔════╝░██╔════╝░██╔════╝██╔══██╗
███████║░╚██╗████╗██╔╝█████╗██║░░░░░██║░░██║██║░░██╗░██║░░██╗░█████╗░░██████╔╝
██╔══██║░░████╔═████║░╚════╝██║░░░░░██║░░██║██║░░╚██╗██║░░╚██╗██╔══╝░░██╔══██╗
██║░░██║░░╚██╔╝░╚██╔╝░░░░░░░███████╗╚█████╔╝╚██████╔╝╚██████╔╝███████╗██║░░██║
╚═╝░░╚═╝░░░╚═╝░░░╚═╝░░░░░░░░╚══════╝░╚════╝░░╚═════╝░░╚═════╝░╚══════╝╚═╝░░╚═╝
This project is compiled with [xmake](https://xmake.io/),
which is a lightweight, cross-platform build tool based on Lua.
--]]

set_project("Awakelion-Logger")
set_description("A low-latency, high-throughput and few-dependencies logger for `AwakeLion Robot Lab` project.")
set_version("1.0.2")
set_xmakever("2.9.8")
set_license("Apache-2.0")

set_defaultplat("linux")
set_languages("c11", "c++20")

add_rules("mode.debug", "mode.release")
if is_mode("debug") then
    set_symbols("debug")
    set_optimize("none")
    add_cxflags("-g")
elseif is_mode("release") then
    set_optimize("fastest")
    add_cxflags("-march=native")
    add_cxflags("-w")
end

option("test")
    set_default(false)
    set_showmenu(true)
    set_description("toggle on for awakelion logger unit tests with googletest.")
option_end()

if has_config("test") then
    add_requires("gtest 1.17.0", {configs = {main = true}})
end
add_requires("openssl", {system = true})
add_requires("ixwebsocket v11.4.6")

namespace("fosu-awakelion")
    -- header-only library
    target("awakelion-logger")
        set_kind("headeronly")
        add_headerfiles("include/(aw_logger/**.hpp)")
        add_headerfiles("include/3rdparty/(nlohmann/**.hpp)")
        add_includedirs("include", {public = true})
        add_includedirs("include/3rdparty", {public = true})

        -- dependencies
        add_packages("ixwebsocket", {public = true})

    -- test
    if has_config("test") then
        for _, file in ipairs(os.files("test/*.cpp")) do
            local name = path.basename(file)
            target("awakelion-logger-test-" .. name)
                set_kind("binary")
                set_default(false)
                add_files(file)
                add_deps("awakelion-logger")
                add_packages("gtest")
                set_rundir("$(projectdir)")
                add_tests("awakelion-logger-test", {runargs = {"--gtest_color=yes"}})
        end
    end
namespace_end() -- namespace fosu-awakelion
