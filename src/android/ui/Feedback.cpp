#include "Feedback.hpp"
#include <mutex>

namespace dabr {
namespace {
std::mutex mutex;
std::string pending;
}
void showMessage(std::string message) {
    std::lock_guard lock(mutex);
    pending = std::move(message);
}
void flushMessages(void* client, const Targets& targets) {
    if (!client || !targets.actionBar) return;
    auto* gui = read<void*>(client, build::field::ClientGuiData);
    const auto* validity = gui ? read<const void*>(gui, build::field::GuiValidity) : nullptr;
    if (!validity || !read<bool>(validity)) return;
    std::string message;
    {
        std::lock_guard lock(mutex);
        message.swap(pending);
    }
    if (!message.empty()) targets.actionBar(gui, message, std::nullopt);
}
}
