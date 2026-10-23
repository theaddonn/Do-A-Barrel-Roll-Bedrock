#pragma once

#include "runtime/Host.hpp"
namespace dabr {
bool installGameHooks(Host&, const Targets&);
void restoreCamera();
void clearHud();
}
