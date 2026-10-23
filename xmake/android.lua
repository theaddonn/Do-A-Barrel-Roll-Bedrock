set_version("1.0.0")
add_requires("nlohmann_json v3.12.0")
local root = os.projectdir()

target("DoABarrelRoll")
    set_kind("shared")
    set_toolchains("ndk")
    set_basename("DoABarrelRoll")
    set_prefixname("")
    set_targetdir(path.join(root, "bin", "android"))
    set_runtimes("c++_shared")
    set_symbols("none")
    set_strip("all")
    add_files(
        path.join(root, "src/android/main.cpp"),
        path.join(root, "src/common/*.cpp"),
        path.join(root, "src/android/config/*.cpp"),
        path.join(root, "src/android/core/*.cpp"),
        path.join(root, "src/android/game/*.cpp"),
        path.join(root, "src/android/runtime/*.cpp"),
        path.join(root, "src/android/ui/*.cpp")
    )
    add_includedirs(path.join(root, "include/android"), path.join(root, "src"), path.join(root, "src/android"))
    add_packages("nlohmann_json")
    add_cxxflags("-g0", "-fPIC", "-fno-rtti", "-Wno-invalid-offsetof", "-fvisibility=hidden", "-fvisibility-inlines-hidden")
    add_links("android", "dl", "m")
    add_shflags("-Wl,-z,max-page-size=16384", "-Wl,--version-script=" .. path.join(os.projectdir(), "src", "android", "exports.map"), {force = true})
    if is_mode("release") then
        set_optimize("fast")
    end
    on_load(function(target)
        import("core.project.config")
        import("detect.sdks.find_ndk")
        assert((config.get("target_type") or "client") == "client", "Android supports the client target only")
        local api = tonumber(config.get("ndk_sdkver") or 28)
        local ndk = find_ndk(config.get("ndk"), {force = true, arch = target:arch(), sdkver = api, compiler = "clang"})
        assert(ndk and ndk.bindir and ndk.sysroot, "A complete Android LLVM NDK is required")
        local suffix = os.host() == "windows" and ".exe" or ""
        local tools = {
            cc = {"clang", "clang"},
            cxx = {"clangxx", "clang++"},
            as = {"clang", "clang"},
            ld = {"clangxx", "clang++"},
            sh = {"clangxx", "clang++"},
            ar = {"ar", "llvm-ar"},
            strip = {"strip", "llvm-strip"}
        }
        for kind, tool in pairs(tools) do
            local program = path.join(ndk.bindir, tool[2] .. suffix)
            assert(os.isfile(program), "Missing NDK tool: %s", program)
            target:set("toolset", kind, tool[1] .. "@" .. program)
        end
        local triple = "--target=aarch64-linux-android" .. api
        local sysroot = "--sysroot=" .. os.args(ndk.sysroot)
        target:add("cxflags", triple, sysroot, {force = true})
        target:add("asflags", triple, sysroot, {force = true})
        target:add("ldflags", triple, sysroot, "--rtlib=compiler-rt", "-stdlib=libc++", {force = true})
        target:add("shflags", triple, sysroot, "--rtlib=compiler-rt", "-stdlib=libc++", {force = true})
    end)
    after_build(function(target)
        import("lib.detect.find_tool")
        os.tryrm(path.join(target:targetdir(), "DoABarrelRoll.sym"))
        local python = find_tool("python3") or find_tool("python")
        local args = {}
        if not python then
            python = find_tool("py")
            if python then table.insert(args, "-3") end
        end
        assert(python, "Python 3 is required to package DoABarrelRoll.levipack")
        table.insert(args, path.join(os.projectdir(), "scripts", "package_levipack.py"))
        table.insert(args, "--library")
        table.insert(args, target:targetfile())
        table.insert(args, "--output")
        table.insert(args, path.join(target:targetdir(), "DoABarrelRoll.levipack"))
        os.vrunv(python.program, args)
    end)
target_end()
