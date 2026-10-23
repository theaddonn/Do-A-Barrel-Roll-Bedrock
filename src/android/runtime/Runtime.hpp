#pragma once

#include "Host.hpp"
#include "Image.hpp"
#include "Menu.hpp"
#include "Metadata.hpp"
#include <atomic>
#include <mutex>

namespace dabr {
class Runtime {
public:
    static Runtime& get();
    bool load(const ModLocation&);
    bool enable();
    bool disable();
    bool unload();
    bool active() const;
    bool hudTop() const;
    const Targets& native() const;
    void libraryLoaded(const char*);
    void toggle();
    Menu& modMenu();
private:
    bool install();
    std::mutex mutex;
    Host host;
    Image image;
    Menu menu;
    Targets targets;
    std::atomic_bool wanted{}, running{};
    bool installed{};
    using Flag = unsigned char (*)(void*, void*);
    Flag hudFlag{}, menuFlag{}, pauseFlag{};
};
}
