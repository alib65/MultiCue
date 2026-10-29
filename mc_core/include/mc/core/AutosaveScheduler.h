#pragma once

namespace mc::core {

// Debounces "save N seconds after the last edit", matching the reference's
// "~1 s after the last edit" autosave cadence. Pure logic, driven by a
// caller-supplied clock reading (seconds, any monotonic origin) rather than
// a real timer, so it is unit-testable without waiting on a clock and
// reusable from any UI toolkit's timer.
class AutosaveScheduler {
public:
    explicit AutosaveScheduler(double debounceSeconds = 1.0) : debounceSeconds_(debounceSeconds) {}

    void markDirty(double nowSeconds) {
        dirty_ = true;
        lastEditSeconds_ = nowSeconds;
    }

    // Call periodically (e.g. from a ~200 ms UI timer). Returns true at
    // most once per edit, the first time it is called after the debounce
    // window has elapsed; the caller should perform the actual save then.
    bool shouldSaveNow(double nowSeconds) {
        if (dirty_ && (nowSeconds - lastEditSeconds_) >= debounceSeconds_) {
            dirty_ = false;
            return true;
        }
        return false;
    }

    bool isDirty() const noexcept { return dirty_; }

private:
    double debounceSeconds_;
    bool dirty_ = false;
    double lastEditSeconds_ = 0.0;
};

} // namespace mc::core
