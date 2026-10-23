#include "levilamina/DoABarrelRoll.h"

#include "ll/api/mod/RegisterHelper.h"
#ifdef DABR_SERVER
#include "levilamina/server/ServerState.h"
#else
#include "levilamina/hooks/Net.h"
#include "levilamina/hooks/State.h"
#endif

namespace do_a_barrel_roll {

DoABarrelRoll& DoABarrelRoll::getInstance() {
    static DoABarrelRoll instance;
    return instance;
}

bool DoABarrelRoll::load() {
    auto& logger = getSelf().getLogger();
#ifdef DABR_SERVER
    server::configPath = getSelf().getConfigDir() / "config.json";
    if (std::filesystem::exists(server::configPath)) {
        if (!server::config.load(server::configPath)) {
            logger.warn("Could not parse {}, using defaults", server::configPath.string());
        }
    } else if (!server::config.save(server::configPath)) {
        logger.warn("Could not write default config to {}", server::configPath.string());
    }
#else
    state::configPath = getSelf().getConfigDir() / "config.json";
    if (std::filesystem::exists(state::configPath)) {
        if (!state::config.load(state::configPath)) {
            logger.warn("Could not parse {}, using defaults", state::configPath.string());
        }
    } else if (!state::config.save(state::configPath)) {
        logger.warn("Could not write default config to {}", state::configPath.string());
    }
#endif
    return true;
}

bool DoABarrelRoll::enable() {
#ifdef DABR_SERVER
    server::installNet();
    server::installThrust();
#else
    hooks::installInput();
    hooks::installCamera();
    hooks::installPlayer();
    hooks::installPlayerModel();
    hooks::installServer();
    hooks::installHud();
    hooks::installKeys();
    hooks::installGamepad();
    hooks::installNet();
#endif
    return true;
}

bool DoABarrelRoll::disable() {
#ifdef DABR_SERVER
    server::uninstallThrust();
    server::uninstallNet();
#else
    hooks::uninstallNet();
    hooks::uninstallGamepad();
    hooks::uninstallKeys();
    hooks::uninstallHud();
    hooks::uninstallServer();
    hooks::uninstallPlayerModel();
    hooks::uninstallPlayer();
    hooks::uninstallCamera();
    hooks::uninstallInput();
#endif
    return true;
}

} // namespace do_a_barrel_roll

LL_REGISTER_MOD(do_a_barrel_roll::DoABarrelRoll, do_a_barrel_roll::DoABarrelRoll::getInstance());
