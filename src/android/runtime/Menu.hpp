#pragma once

#include "MenuAbi.hpp"
#include "config/Settings.hpp"
#include <mutex>

namespace dabr {
class Menu {
public:
    bool connect(std::string_view owner);
    void disconnect();
    void frame(bool flying, const core::Config&);
    void enabled(bool);
private:
    void buttons(double opacity);
    void refresh();
    std::mutex schemaMutex;
    void* handle{};
    std::string owner;
    bool registered{}, controlsVisible{};
    double opacity{-1};
    bool (*registerModule)(const pl::modmenu::ModuleInfo&){};
    void (*unregisterModule)(std::string_view){};
    void (*setEnabled)(std::string_view, bool){};
    bool (*setSchema)(std::string_view, std::string_view){};
    bool (*registerButton)(const pl::modmenu::ButtonInfo&){};
    void (*unregisterButton)(std::string_view){};
};
}
