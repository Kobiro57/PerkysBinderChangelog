#pragma once
#include <functional>
#include <string>
#include <utility>

namespace keybinder {
// Pairs only the commands issued by this forum session. Repeated rendered
// frames and the setup-to-browser handoff must not send extra commands.
class ForumAfkSession {
public:
    using Sender = std::function<bool(const std::string&)>;
    void SetSender(Sender sender) { sender_ = std::move(sender); }
    void SetVisible(bool visible) noexcept {
        if (visible == visible_) return;
        visible_ = visible;
        if (visible) requestedAfk_ = Send("/afk");
        else {
            const bool leave = requestedAfk_;
            requestedAfk_ = false;
            if (leave) Send("/back");
        }
    }
private:
    bool Send(const char* command) noexcept {
        try { return sender_ && sender_(command); }
        catch (...) { return false; }
    }
    Sender sender_;
    bool visible_ = false;
    bool requestedAfk_ = false;
};
}
