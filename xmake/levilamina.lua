set_version("0.2.0-mc26.51")
local root = os.projectdir()

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

add_requires("levilamina 26.51.6", {configs = {target_type = get_config("target_type")}})

add_requires("levibuildscript 0.6.1")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("DoABarrelRoll")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker", {modName = "DoABarrelRoll", modVersion = "0.2.0-mc26.51", modPlatform = get_config("target_type")})
    if is_plat("windows") then
        add_defines("NOMINMAX", "UNICODE", "_HAS_CXX23=1")
        set_exceptions("none")
        add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
        add_cxflags(
            "/EHs",
            "-Wno-microsoft-cast",
            "-Wno-invalid-offsetof",
            "-Wno-c++2b-extensions",
            "-Wno-microsoft-include",
            "-Wno-overloaded-virtual",
            "-Wno-ignored-qualifiers",
            "-Wno-missing-field-initializers",
            "-Wno-potentially-evaluated-expression",
            "-Wno-pragma-system-header-outside-header",
            {tools = {"clang_cl"}}
        )
        set_toolchains("clang-cl")
    end
    add_packages("levilamina")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("none")
    add_headerfiles(path.join(root, "src/common/*.h"), path.join(root, "src/levilamina/**.h"))
    add_includedirs(path.join(root, "src"))
    add_files(
        path.join(root, "src/common/*.cpp"),
        path.join(root, "src/levilamina/*.cpp"),
        path.join(root, "src/levilamina/core/*.cpp"),
        path.join(root, "src/levilamina/net/*.cpp")
    )
    if get_config("target_type") == "server" then
        add_defines("DABR_SERVER")
        add_files(path.join(root, "src/levilamina/server/*.cpp"))
    else
        add_defines("DABR_CLIENT")
        add_files(path.join(root, "src/levilamina/hooks/*.cpp"))
    end
target_end()
