#pragma once

// Port of the client-side parts of nl.enjarai.doabarrelroll.config.ModConfig. Banking and control-surface
// efficacy formulas are the hardcoded Java defaults (see FlightController).

#include <filesystem>
#include <string>

namespace do_a_barrel_roll::core {

enum class ActivationBehaviour {
    Vanilla,      // normal glide activation
    TripleJump,   // vanilla activation cancelled until the third jump press
    Hybrid,       // vanilla activation, mod takes over after a third jump press
    HybridToggle, // like Hybrid, the third jump press toggles mod control
};

/// Java defaults: pitch 1, yaw 0.4, roll 1.
struct Sensitivity {
    double pitch = 1.0;
    double yaw   = 0.4;
    double roll  = 1.0;
};

struct Config {
    int formatVersion = 2;

    // general
    bool modEnabled = true;

    // general.controls
    bool                switchRollAndYaw      = false; // false: mouse X rolls, true: mouse X yaws
    bool                invertPitch           = false;
    bool                momentumBasedMouse    = false;
    double              momentumMouseDeadzone = 0.2;
    bool                showMomentumWidget    = true;
    ActivationBehaviour activationBehaviour   = ActivationBehaviour::Vanilla;
    bool                disableWhenSubmerged  = true;

    // general.hud
    bool showHorizon = false;

    // general.banking
    bool   enableBanking                  = true;
    double bankingStrength                = 20.0;
    bool   simulateControlSurfaceEfficacy = false;
    bool   automaticRighting              = false;
    double rightingStrength               = 50.0;

    // general.thrust
    bool   enableThrust       = false;
    double maxThrust          = 2.0;
    double thrustAcceleration = 0.1;
    bool   thrustParticles    = true;

    // general.misc (the last three are Bedrock-only)
    bool   enableEasterEggs  = true;
    bool   thrustAllowRemote = false; // apply thrust without an in-process server (rubber-bands)
    bool   enableGamepad     = true;  // XInput: right stick pitch/roll, bumpers yaw, triggers thrust
    double gamepadDeadzone   = 0.15;  // radial stick deadzone, 0..1

    // sensitivity.camera_smoothing: 0 disables smoothing for that axis; larger is smoother (more lag).
    Sensitivity smoothing{1.0, 2.5, 1.0};
    // sensitivity.desktop / sensitivity.controller
    Sensitivity desktop;
    Sensitivity controller;

    /// Server overrides, never saved: they shadow the user's values while connected to a server mod.
    bool serverForceEnabled = false;
    bool serverBlockThrust  = false;

    [[nodiscard]] bool effectiveEnabled() const { return modEnabled || serverForceEnabled; }
    [[nodiscard]] bool effectiveThrust() const { return enableThrust && !serverBlockThrust; }

    /// Loads the Java mod's nested JSON layout. Missing file, missing or unknown keys keep defaults.
    /// Returns false only if the file exists but is unparsable.
    bool load(const std::filesystem::path& path);
    /// Writes pretty-printed JSON, creating parent dirs. Returns false on I/O failure.
    bool save(const std::filesystem::path& path) const;
};

[[nodiscard]] const char* toString(ActivationBehaviour b);

} // namespace do_a_barrel_roll::core
