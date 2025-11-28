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
set_version("1.2.3")
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
        add_includedirs("$(builddir)", {public = true})

        -- dependencies
        add_packages("ixwebsocket", {public = true})

        -- configuration
        if has_config("test") then
        -- local test
            set_configvar("SETTINGS_FILE_PATH", path.absolute("config/aw_logger_settings.json"))
        else
        -- integration install
            set_configvar("SETTINGS_FILE_PATH", path.join("$(prefix)", "share/aw_logger/aw_logger_settings.json"))
        end
        add_configfiles("config/settings_path.h.in", {
            filename = "aw_logger/settings_path.h",
            pattern = "@(.-)@"
        })
        add_headerfiles("$(builddir)/aw_logger/settings_path.h", {prefixdir = "aw_logger"})
        add_installfiles("config/aw_logger_settings.json", {prefixdir = "share/aw_logger"})

        -- check config file
        before_build(function (target)
            local config = path.absolute("config/aw_logger_settings.json")
            if not os.isfile(config) then
                raise("can not find aw_logger_settings.json: " .. config)
            end
        end)

    -- cpp server
    target("awakelion-logger-cpp-server")
        set_kind("binary")
        set_default(false)
        add_includedirs("server/cpp")
        add_includedirs("include/3rdparty", {public = true})
        add_files("server/cpp/*.cpp")

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

                -- enable the following on_test function to print test results, but it will increase spent time significantly
                -- you can check test logs in `build/.gens` instead while use test command `xmake test -vD`

--[[
                on_test(function (target, opt) -- for printing test results
                    cprint("${bright cyan}========================================${clear}")
                    cprint("${bright cyan}[Running]${clear} %s", target:name())
                    cprint("${bright cyan}========================================${clear}")

                    local targetfile = target:targetfile()
                    local runargs = opt.runargs

                    local outdata, errdata = os.iorunv(targetfile, runargs)

                    if outdata then
                        print(outdata)
                    end
                    if errdata and #errdata > 0 then
                        cprint("${bright yellow}%s${clear}", errdata)
                    end

                    local ok = os.execv(targetfile, runargs)

                    cprint("${bright cyan}========================================${clear}")
                    if ok == 0 then
                        cprint("${bright green}✓ [PASSED]${clear} %s", target:name())
                        cprint("${bright cyan}========================================${clear}\n")
                        return true
                    else
                        cprint("${bright red}✗ [FAILED]${clear} %s (exit code: %d)", target:name(), ok)
                        cprint("${bright cyan}========================================${clear}\n")
                        return false, error
                    end
                end)
--]]
        end
    end
namespace_end() -- namespace fosu-awakelion
