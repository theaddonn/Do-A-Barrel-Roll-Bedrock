#include "Runtime.hpp"
#include "Input.hpp"
#include "config/Settings.hpp"
#include "game/Hooks.hpp"
#include "game/Camera.hpp"
#include "ui/Hud.hpp"
#include "ui/Feedback.hpp"
#include <android/dlext.h>
#include <dlfcn.h>

namespace dabr {
namespace {
void* (*openNext)(const char*, int){};
void* (*openExtNext)(const char*, int, const android_dlextinfo*){};
thread_local bool insideLoader{};
void notifyLoad(const char* name) {
    if (insideLoader || !name) return;
    insideLoader = true;
    Runtime::get().libraryLoaded(name);
    insideLoader = false;
}
void* openHook(const char* name, int flags) {
    auto* handle = openNext(name, flags);
    if (handle) notifyLoad(name);
    return handle;
}
void* openExtHook(const char* name, int flags, const android_dlextinfo* info) {
    auto* handle = openExtNext(name, flags, info);
    if (handle) notifyLoad(name);
    return handle;
}
}
Runtime& Runtime::get() { static Runtime runtime; return runtime; }
bool Runtime::active() const { return running.load(std::memory_order_acquire) && wanted.load(std::memory_order_acquire); }
const Targets& Runtime::native() const { return targets; }
Menu& Runtime::modMenu() { return menu; }
bool Runtime::hudTop() const {
    return hudFlag && hudFlag(nullptr, nullptr) && !(menuFlag && menuFlag(nullptr, nullptr)) && !(pauseFlag && pauseFlag(nullptr, nullptr));
}

bool Runtime::load(const ModLocation& location) {
    std::lock_guard lock(mutex);
    insideLoader = true;
    const bool connected = host.connect();
    insideLoader = false;
    if (!connected) { return false; }
    if (location.root.empty() || !Settings::get().load(location.root)) { return false; }
    if (!menu.connect(location.owner)) { return false; }
    if (!Input::get().connect()) { menu.disconnect(); return false; }
    auto* handle = dlopen("libpreloader.so", RTLD_NOW | RTLD_NOLOAD);
    const auto resolve = [&](const char* name) {
        auto* symbol = handle ? dlsym(handle, name) : nullptr;
        return symbol ? symbol : dlsym(RTLD_DEFAULT, name);
    };
    hudFlag = reinterpret_cast<Flag>(resolve("Java_org_levimc_launcher_preloader_PreloaderInput_nativeIsHudScreenOpen"));
    menuFlag = reinterpret_cast<Flag>(resolve("Java_org_levimc_launcher_preloader_PreloaderInput_nativeIsShowingMenu"));
    pauseFlag = reinterpret_cast<Flag>(resolve("Java_org_levimc_launcher_preloader_PreloaderInput_nativeIsPauseMenuOpen"));
    if (handle) dlclose(handle);
    if (!hudFlag) { menu.disconnect(); return false; }
    if (image.locate()) return true;
    bool observed = false;
    if (auto* ordinary = dlsym(RTLD_DEFAULT, "dlopen")) observed |= host.install(ordinary, reinterpret_cast<void*>(openHook), reinterpret_cast<void**>(&openNext));
    if (auto* extended = dlsym(RTLD_DEFAULT, "android_dlopen_ext")) observed |= host.install(extended, reinterpret_cast<void*>(openExtHook), reinterpret_cast<void**>(&openExtNext));
    if (!observed) { menu.disconnect(); }
    return observed;
}
bool Runtime::install() {
    if (installed) { running.store(true, std::memory_order_release); return true; }
    if (!image.locate()) return true;
    if (!image.resolve(targets)) { return false; }
    if (!installGameHooks(host, targets) || !installCameraHooks(host, targets) || !installHudHooks(host, targets)) {
        running.store(false);
        return false;
    }
    installed = true;
    running.store(true, std::memory_order_release);
    return true;
}
bool Runtime::enable() {
    std::lock_guard lock(mutex);
    wanted.store(true, std::memory_order_release);
    return install();
}
bool Runtime::disable() {
    wanted.store(false, std::memory_order_release);
    running.store(false, std::memory_order_release);
    Input::get().allow(false);
    menu.frame(false, *Settings::get().snapshot());
    return true;
}
bool Runtime::unload() {
    disable();
    menu.disconnect();
    return true;
}
void Runtime::libraryLoaded(const char* path) {
    std::string_view name(path);
    const auto slash = name.find_last_of('/');
    if (slash != name.npos) name.remove_prefix(slash + 1);
    if (name != "libminecraftpe.so" || !wanted.load()) return;
    std::lock_guard lock(mutex);
    if (wanted.load()) install();
}
void Runtime::toggle() {
    const bool value = !Settings::get().snapshot()->modEnabled;
    if (Settings::get().change("mod_enabled", value ? "true" : "false")) {
        showMessage(std::string(value ? "§aEnabled §r" : "§cDisabled §r") + "Do a Barrel Roll");
        menu.enabled(value);
    }
}
}
