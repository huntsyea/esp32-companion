#ifndef CONVERSATION_SESSION_H
#define CONVERSATION_SESSION_H

#include <cstdint>
#include <mutex>
#include <string>

class ConversationSession {
public:
    void Open(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        session_id_ = session_id;
        active_ = false;
        abort_sent_ = false;
        ++epoch_;
    }

    void Close() {
        std::lock_guard<std::mutex> lock(mutex_);
        session_id_.clear();
        active_ = false;
        abort_sent_ = false;
        ++epoch_;
    }

    uint32_t StartInteraction() {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ = !session_id_.empty();
        abort_sent_ = false;
        return ++epoch_;
    }

    void Complete(uint32_t epoch) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (epoch_ == epoch) {
            active_ = false;
        }
    }

    bool Cancel() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) {
            return false;
        }
        const bool send_abort = active_ && !abort_sent_ && !session_id_.empty();
        abort_sent_ = abort_sent_ || send_abort;
        active_ = false;
        ++epoch_;
        return send_abort;
    }

    bool IsActive() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_;
    }

    bool Matches(const std::string& session_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_ && !session_id.empty() && session_id == session_id_;
    }

    bool Capture(const std::string& session_id, uint32_t& epoch) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_ || session_id.empty() || session_id != session_id_) {
            return false;
        }
        epoch = epoch_;
        return true;
    }

    bool CaptureCurrent(std::string& session_id, uint32_t& epoch) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_ || session_id_.empty()) {
            return false;
        }
        session_id = session_id_;
        epoch = epoch_;
        return true;
    }

    bool Matches(const std::string& session_id, uint32_t epoch) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_ && epoch == epoch_ && !session_id.empty() && session_id == session_id_;
    }

    bool IsCurrent(uint32_t epoch) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_ && epoch == epoch_;
    }

    uint32_t epoch() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return epoch_;
    }

private:
    mutable std::mutex mutex_;
    std::string session_id_;
    uint32_t epoch_ = 0;
    bool active_ = false;
    bool abort_sent_ = false;
};

#endif  // CONVERSATION_SESSION_H
