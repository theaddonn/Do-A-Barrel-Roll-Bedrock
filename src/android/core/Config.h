#pragma once

#include <filesystem>
#include <array>
#include <nlohmann/json_fwd.hpp>
#include <string>

namespace do_a_barrel_roll::core {

enum class ActivationBehaviour {
    Vanilla,
    TripleJump,
    Hybrid,
    HybridToggle,
};

struct Sensitivity {
    double pitch = 1.0;
    double yaw   = 0.4;
    double roll  = 1.0;
};

struct Config {
    int formatVersion = 2;

    bool modEnabled = true;

    bool                switchRollAndYaw      = false;
    bool                invertPitch           = false;
    bool                momentumBasedMouse    = false;
    double              momentumMouseDeadzone = 0.2;
    bool                showMomentumWidget    = true;
    ActivationBehaviour activationBehaviour   = ActivationBehaviour::Vanilla;
    bool                disableWhenSubmerged  = true;

    bool   enableBanking                  = true;
    double bankingStrength                = 20.0;
    bool   simulateControlSurfaceEfficacy = false;
    bool   automaticRighting              = false;
    double rightingStrength               = 50.0;

    bool   enableGamepad     = true;
    double gamepadDeadzone   = 0.15;

    Sensitivity smoothing{1.0, 2.5, 1.0};

    Sensitivity desktop;
    Sensitivity controller;
    Sensitivity touch;
    bool touchButtons = true;
    double buttonOpacity = 70.0;
    std::array<int, 7> bindings{37, 0, 0, 29, 32, 0, 0};
    nlohmann::json toJson() const;
    void readJson(const nlohmann::json& root);

    bool load(const std::filesystem::path& path);

    bool save(const std::filesystem::path& path) const;
};

[[nodiscard]] const char* toString(ActivationBehaviour b);

}
