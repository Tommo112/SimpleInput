#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "simpleinput/AutomationEngine.h"

namespace lc {

class ClickRunner {
public:
    using ClickFunction = std::function<void(KeyCode)>;

    ClickRunner() = default;
    ClickRunner(const ClickRunner&) = delete;
    ClickRunner& operator=(const ClickRunner&) = delete;
    ~ClickRunner();

    void start(KeyCode key, double cps, ClickFunction click);
    void stop();
    [[nodiscard]] bool active() const;

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::atomic_bool stopRequested_ = false;
    std::atomic_bool active_ = false;
};

class PlaybackRunner {
public:
    using SendFunction = std::function<void(const RecordedInput&)>;

    PlaybackRunner() = default;
    PlaybackRunner(const PlaybackRunner&) = delete;
    PlaybackRunner& operator=(const PlaybackRunner&) = delete;
    ~PlaybackRunner();

    bool start(std::vector<RecordedInput> recording, SendFunction send);
    void stop();
    [[nodiscard]] bool active() const;

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::atomic_bool stopRequested_ = false;
    std::atomic_bool active_ = false;
};

} // namespace lc
