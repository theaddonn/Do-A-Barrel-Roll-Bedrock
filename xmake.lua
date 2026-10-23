add_rules("mode.debug", "mode.release")
set_project("DoABarrelRoll")
set_languages("c++20")
set_allowedplats("windows", "android")
set_allowedarchs("windows|x64", "android|arm64-v8a")

option("target_type")
    set_default("client")
    set_showmenu(true)
    set_values("client", "server")
option_end()

if is_plat("android") then
    includes("xmake/android.lua")
elseif is_plat("windows") then
    includes("xmake/levilamina.lua")
end
