#include "runtime/Runtime.hpp"
#include <jni.h>

namespace pl::mod { class ModContext; }

extern "C" __attribute__((visibility("default")))
bool PLMod_Load(JavaVM*, const PLModInfo* info) {
    return dabr::Runtime::get().load(dabr::legacyLocation(info));
}

extern "C" __attribute__((visibility("default")))
bool PLMod_Enable() { return dabr::Runtime::get().enable(); }

extern "C" __attribute__((visibility("default")))
bool PLMod_Disable() { return dabr::Runtime::get().disable(); }

extern "C" __attribute__((visibility("default")))
bool PLMod_Unload() { return dabr::Runtime::get().unload(); }

namespace {
struct Registration {
    void* instance;
    bool (*load)(void*, pl::mod::ModContext&);
    bool (*enable)(void*, pl::mod::ModContext&);
    bool (*disable)(void*, pl::mod::ModContext&);
    bool (*unload)(void*, pl::mod::ModContext&);
};
bool cppLoad(void*, pl::mod::ModContext& context) {
    return dabr::Runtime::get().load(dabr::contextLocation(&context));
}
bool cppEnable(void*, pl::mod::ModContext&) { return PLMod_Enable(); }
bool cppDisable(void*, pl::mod::ModContext&) { return PLMod_Disable(); }
bool cppUnload(void*, pl::mod::ModContext&) { return PLMod_Unload(); }
int identity;
Registration registration{&identity, cppLoad, cppEnable, cppDisable, cppUnload};
}

extern "C" __attribute__((visibility("default")))
Registration* PLGetModRegistration() { return &registration; }
