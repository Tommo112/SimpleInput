#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include <windows.h>

#include "simpleinput/AutomationEngine.h"

namespace lc {

inline constexpr ULONG_PTR kInjectedExtraInfo = 0x4C434B52;

class TimerResolution {
public:
    TimerResolution();
    TimerResolution(const TimerResolution&) = delete;
    TimerResolution& operator=(const TimerResolution&) = delete;
    ~TimerResolution();

private:
    bool active_ = false;
};

class GlobalInputHook {
public:
    using Callback = std::function<void(const InputEvent&)>;

    GlobalInputHook() = default;
    GlobalInputHook(const GlobalInputHook&) = delete;
    GlobalInputHook& operator=(const GlobalInputHook&) = delete;
    ~GlobalInputHook();

    bool start(Callback callback);
    void stop();
    [[nodiscard]] bool running() const;
    [[nodiscard]] std::wstring lastError() const;

private:
    static LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK mouseProc(int code, WPARAM wParam, LPARAM lParam);
    static GlobalInputHook* activeHook_;

    void runHookThread();
    void dispatch(const InputEvent& event);
    void setLastError(std::wstring message);

    mutable std::mutex mutex_;
    std::thread thread_;
    DWORD threadId_ = 0;
    HHOOK keyboardHook_ = nullptr;
    HHOOK mouseHook_ = nullptr;
    Callback callback_;
    std::atomic_bool running_ = false;
    std::atomic_bool ready_ = false;
    std::wstring lastError_;
};

namespace NativeInput {
bool isMouseKey(KeyCode key);
KeyCode normalizeKey(KeyCode key);
void sendKeyState(KeyCode key, bool down);
void clickKey(KeyCode key);
void sendRecordedInput(const RecordedInput& input);
std::wstring keyName(KeyCode key);
} // namespace NativeInput

} // namespace lc
