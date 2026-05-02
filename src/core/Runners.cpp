#include "simpleinput/Runners.h"

#include <algorithm>
#include <chrono>

namespace lc {

namespace {

bool waitOrStop(std::condition_variable& cv, std::mutex& mutex, const std::atomic_bool& stopRequested,
                std::chrono::milliseconds duration) {
    std::unique_lock lock(mutex);
    return cv.wait_for(lock, duration, [&stopRequested] { return stopRequested.load(); });
}

} // namespace

ClickRunner::~ClickRunner() {
    stop();
}

void ClickRunner::start(KeyCode key, double cps, ClickFunction click) {
    stop();
    if (!click) {
        return;
    }

    cps = std::clamp(cps, 1.0, 1000.0);
    const auto interval = std::chrono::milliseconds(std::max<int>(1, static_cast<int>(1000.0 / cps)));

    stopRequested_.store(false);
    active_.store(true);
    worker_ = std::thread([this, key, interval, click = std::move(click)] {
        while (!stopRequested_.load()) {
            click(key);
            if (waitOrStop(cv_, mutex_, stopRequested_, interval)) {
                break;
            }
        }
        active_.store(false);
    });
}

void ClickRunner::stop() {
    stopRequested_.store(true);
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    active_.store(false);
}

bool ClickRunner::active() const {
    return active_.load();
}

PlaybackRunner::~PlaybackRunner() {
    stop();
}

bool PlaybackRunner::start(std::vector<RecordedInput> recording, SendFunction send) {
    if (!send || recording.empty() || active_.load()) {
        return false;
    }

    stop();
    stopRequested_.store(false);
    active_.store(true);
    worker_ = std::thread([this, recording = std::move(recording), send = std::move(send)] {
        for (const auto& input : recording) {
            if (waitOrStop(cv_, mutex_, stopRequested_, std::chrono::milliseconds(input.delayMs))) {
                break;
            }
            if (stopRequested_.load()) {
                break;
            }
            send(input);
        }
        active_.store(false);
    });
    return true;
}

void PlaybackRunner::stop() {
    stopRequested_.store(true);
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    active_.store(false);
}

bool PlaybackRunner::active() const {
    return active_.load();
}

} // namespace lc
