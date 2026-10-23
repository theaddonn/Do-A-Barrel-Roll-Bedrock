#pragma once

// Local-only on-screen feedback (action bar above the hotbar). Nothing is sent to the server.

#include <string>

class ClientInstance;

namespace do_a_barrel_roll::hooks {

/// Queues an action bar message (may contain section-sign color codes). Safe to call from key handlers.
void showMessage(std::string const& message);

/// Shows queued messages once the GUI is available. Call once per frame from a ClientInstance hook.
void flushMessages(ClientInstance& client);

} // namespace do_a_barrel_roll::hooks
