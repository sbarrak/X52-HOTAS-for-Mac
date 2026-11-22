// Undo / redo for a profile: remembers what the profile file looked like (as text) before each edit.
// No UI and no files here; the app and the plugin each keep one and feed it what they see.
#pragma once
#include <string>
#include <vector>

namespace x52 {

class History {
public:
    // Edits closer together than this belong to one step: a slider drag or typing is one undo, not hundreds.
    static constexpr double kJoin = 1.0;
    static constexpr size_t kMax = 50;

    // A different profile is in use (or none): nothing to undo or redo, `text` is where it stands.
    void reset(const std::string& text) {
        undo_.clear();
        redo_.clear();
        current_ = text;
        lastEdit_ = -1e9;
    }
    // The profile now reads `text`, at time `now` (seconds). A change adds an undo step unless it follows the
    // previous one within kJoin; any change drops the redo steps.
    void record(const std::string& text, double now) {
        if (text == current_) return;
        if (undo_.empty() || now - lastEdit_ > kJoin) {
            undo_.push_back(current_);
            if (undo_.size() > kMax) undo_.erase(undo_.begin());
        }
        current_ = text;
        redo_.clear();
        lastEdit_ = now;
    }
    // Replaces what the profile is taken to read now without making a step (after undo / redo wrote the file and
    // the profile was read back, whose text may differ in form only).
    void adopt(const std::string& text) { current_ = text; }

    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    // The text to go back to / forward to ("" and false when there is none). The step stands on its own: the
    // next edit starts a new one even if it comes straight after.
    bool undo(std::string& out) { return step(undo_, redo_, out); }
    bool redo(std::string& out) { return step(redo_, undo_, out); }
    const std::string& current() const { return current_; }

private:
    bool step(std::vector<std::string>& from, std::vector<std::string>& to, std::string& out) {
        if (from.empty()) return false;
        to.push_back(current_);
        current_ = from.back();
        from.pop_back();
        lastEdit_ = -1e9;
        out = current_;
        return true;
    }
    std::vector<std::string> undo_, redo_;
    std::string current_;
    double lastEdit_ = -1e9;
};

}  // namespace x52
