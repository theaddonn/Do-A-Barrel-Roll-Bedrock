#include "levilamina/hooks/Feedback.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/GuiData.h"

#include <mutex>
#include <optional>
#include <vector>

namespace do_a_barrel_roll::hooks {

namespace {
std::mutex               mutex;
std::vector<std::string> pending;
} // namespace

void showMessage(std::string const& message) {
    std::lock_guard lock(mutex);
    pending.push_back(message);
}

void flushMessages(ClientInstance& client) {
    std::vector<std::string> messages;
    {
        std::lock_guard lock(mutex);
        if (pending.empty()) return;
        messages.swap(pending);
    }
    auto gui = client.getGuiData();
    // Only the latest message matters for the action bar.
    gui->setActionBarMessage(messages.back(), std::nullopt);
}

} // namespace do_a_barrel_roll::hooks
