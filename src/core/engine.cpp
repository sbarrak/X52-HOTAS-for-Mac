#include "engine.h"

#include <algorithm>

namespace x52 {

Engine::Engine(Host& host) : host_(host) {}

bool Engine::isDown(int button) const {
    return button >= 1 && button <= kMaxButton && buttons_[button].down;
}

void Engine::endHeld(Button& b) {
    if (b.hasHeld) {
        if (b.held.kind == StepKind::Command) host_.commandEnd(b.held.text);
        else if (b.held.kind == StepKind::Key) host_.keyUp(b.held.text);
        b.hasHeld = false;
    }
    b.state = State::Idle;
}

void Engine::releaseAll() {
    for (Button& b : buttons_) endHeld(b);
    running_.clear();
}

void Engine::setProfile(const Profile& p) {
    releaseAll();
    profile_ = p;
    // The switches keep their physical position across a profile change.
    shift_ = profile_.pinkieShift && isDown(profile_.shiftButton);
    // A latched clutch stays as it was; a held one follows the button.
    clutch_ = profile_.clutchSafe && (profile_.clutchLatched ? clutch_ : isDown(profile_.clutchButton));
}

// A lone command or key follows the button (down = begin, up = end); anything longer is a macro.
void Engine::startHeld(Button& b, const Action& a, double now) {
    if (a.size() == 1 && (a[0].kind == StepKind::Command || a[0].kind == StepKind::Key)) {
        b.held = a[0];
        b.hasHeld = true;
        if (a[0].kind == StepKind::Command) host_.commandBegin(a[0].text);
        else host_.keyDown(a[0].text);
    } else {
        runOnce(a, now);
    }
    b.state = State::Held;
}

void Engine::advance(Running& r, double now) {
    while (r.next < r.steps.size() && now >= r.resumeAt) {
        const Step& s = r.steps[r.next++];
        switch (s.kind) {
            case StepKind::Command: host_.commandOnce(s.text); break;
            case StepKind::Key:
                host_.keyDown(s.text);
                host_.keyUp(s.text);
                break;
            case StepKind::Wait: r.resumeAt = now + s.ms / 1000.0; break;
            case StepKind::Dataref: host_.datarefStep(s); break;
        }
    }
}

void Engine::runOnce(const Action& a, double now) {
    if (a.empty()) return;
    Running r;
    r.steps = a;
    r.resumeAt = now;
    advance(r, now);
    if (r.next < r.steps.size()) running_.push_back(r);
}

void Engine::buttonEvent(int button, bool down, double now) {
    if (button < 1 || button > kMaxButton) return;
    Button& b = buttons_[button];
    if (b.down == down) return;
    b.down = down;

    for (int m = 0; m < 3; ++m) {
        if (button != profile_.modeButtons[m]) continue;
        // A worn dial flickers between positions; the mode only changes once the dial
        // has rested on the new position (see tick).
        if (down) {
            pendingMode_ = m + 1 != mode_ ? m + 1 : 0;
            pendingSince_ = now;
        } else if (pendingMode_ == m + 1) {
            pendingMode_ = 0;
        }
        return;
    }
    if (profile_.pinkieShift && button == profile_.shiftButton) {
        shift_ = down;
        host_.layerChanged(mode_, shift_);
        return;
    }
    if (profile_.clutchSafe && button == profile_.clutchButton) {
        if (!profile_.clutchLatched) clutch_ = down;
        else if (down) clutch_ = !clutch_;
        return;
    }

    if (!down) {
        if (b.state == State::PendingHold) {
            b.state = State::Idle;
            runOnce(b.binding.press, now);   // released early: it was a tap
        } else {
            endHeld(b);
        }
        return;
    }

    const Binding* found = profile_.find(button, layer());
    if (clutch_) {
        // Safe mode: say what the button would do, but do not do it.
        host_.showLabel(found && !found->label.empty() ? found->label : buttonName(button));
        return;
    }
    if (!found) {
        host_.showLabel(buttonName(button));   // unprogrammed: its standard name
        return;
    }
    b.binding = *found;
    b.downTime = now;
    host_.showLabel(!b.binding.label.empty() ? b.binding.label
                    : !b.binding.press.empty() || b.binding.holdLabel.empty() ? buttonName(button)
                    : b.binding.holdLabel);
    if (b.binding.hold.empty()) startHeld(b, b.binding.press, now);
    else b.state = State::PendingHold;
}

void Engine::tick(double now) {
    if (pendingMode_ && now - pendingSince_ >= profile_.modeSettleMs / 1000.0) {
        mode_ = pendingMode_;
        pendingMode_ = 0;
        host_.layerChanged(mode_, shift_);
    }
    for (Button& b : buttons_) {
        if (b.state == State::PendingHold && now - b.downTime >= profile_.holdMs / 1000.0) {
            if (!b.binding.holdLabel.empty()) host_.showLabel(b.binding.holdLabel);
            startHeld(b, b.binding.hold, now);
        }
    }
    for (Running& r : running_) advance(r, now);
    running_.erase(std::remove_if(running_.begin(), running_.end(),
                                  [](const Running& r) { return r.next >= r.steps.size(); }),
                   running_.end());
}

}  // namespace x52
