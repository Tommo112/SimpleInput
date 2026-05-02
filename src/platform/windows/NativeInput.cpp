#include "simpleinput/NativeInput.h"

#include <chrono>

#include <mmsystem.h>

namespace lc {

GlobalInputHook* GlobalInputHook::activeHook_ = nullptr;

namespace {

bool isInjected(ULONG_PTR extraInfo) {
    return extraInfo == kInjectedExtraInfo;
}

bool isKeyMessage(WPARAM message, bool* down) {
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
        *down = true;
        return true;
    }
    if (message == WM_KEYUP || message == WM_SYSKEYUP) {
        *down = false;
        return true;
    }
    return false;
}

bool isExtendedKey(KeyCode key) {
    switch (key) {
    case VK_RCONTROL:
    case VK_RMENU:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_NUMLOCK:
    case VK_DIVIDE:
        return true;
    default:
        return false;
    }
}

void fillMouseInput(INPUT& input, KeyCode key, bool down) {
    input.type = INPUT_MOUSE;
    input.mi.dwExtraInfo = kInjectedExtraInfo;

    switch (key) {
    case VK_LBUTTON:
        input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        break;
    case VK_RBUTTON:
        input.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
        break;
    case VK_MBUTTON:
        input.mi.dwFlags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
        break;
    case VK_XBUTTON1:
        input.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
        input.mi.mouseData = XBUTTON1;
        break;
    case VK_XBUTTON2:
        input.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
        input.mi.mouseData = XBUTTON2;
        break;
    default:
        break;
    }
}

std::uint64_t nowMs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

} // namespace

TimerResolution::TimerResolution() {
    active_ = timeBeginPeriod(1) == TIMERR_NOERROR;
}

TimerResolution::~TimerResolution() {
    if (active_) {
        timeEndPeriod(1);
    }
}

GlobalInputHook::~GlobalInputHook() {
    stop();
}

bool GlobalInputHook::start(Callback callback) {
    stop();
    {
        std::lock_guard lock(mutex_);
        callback_ = std::move(callback);
        lastError_.clear();
        ready_.store(false);
        activeHook_ = this;
    }

    thread_ = std::thread(&GlobalInputHook::runHookThread, this);
    for (int i = 0; i < 200 && !ready_.load(); ++i) {
        Sleep(5);
    }
    return running_.load();
}

void GlobalInputHook::stop() {
    if (thread_.joinable()) {
        if (threadId_ != 0) {
            PostThreadMessageW(threadId_, WM_QUIT, 0, 0);
        }
        thread_.join();
    }

    std::lock_guard lock(mutex_);
    if (activeHook_ == this) {
        activeHook_ = nullptr;
    }
    callback_ = nullptr;
    threadId_ = 0;
    ready_.store(false);
    running_.store(false);
}

bool GlobalInputHook::running() const {
    return running_.load();
}

std::wstring GlobalInputHook::lastError() const {
    std::lock_guard lock(mutex_);
    return lastError_;
}

LRESULT CALLBACK GlobalInputHook::keyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    auto* self = activeHook_;
    if (code < 0 || self == nullptr) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    bool down = false;
    if (!isKeyMessage(wParam, &down)) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    InputEvent event;
    event.key = NativeInput::normalizeKey(static_cast<KeyCode>(kb->vkCode));
    event.down = down;
    event.injected = isInjected(kb->dwExtraInfo);
    event.timeMs = nowMs();
    self->dispatch(event);

    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK GlobalInputHook::mouseProc(int code, WPARAM wParam, LPARAM lParam) {
    auto* self = activeHook_;
    if (code < 0 || self == nullptr) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const auto* mouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    InputEvent event;
    event.timeMs = nowMs();
    event.x = mouse->pt.x;
    event.y = mouse->pt.y;
    event.hasPosition = true;
    event.injected = isInjected(mouse->dwExtraInfo);

    switch (wParam) {
    case WM_LBUTTONDOWN:
        event.key = VK_LBUTTON;
        event.down = true;
        break;
    case WM_LBUTTONUP:
        event.key = VK_LBUTTON;
        event.down = false;
        break;
    case WM_RBUTTONDOWN:
        event.key = VK_RBUTTON;
        event.down = true;
        break;
    case WM_RBUTTONUP:
        event.key = VK_RBUTTON;
        event.down = false;
        break;
    case WM_MBUTTONDOWN:
        event.key = VK_MBUTTON;
        event.down = true;
        break;
    case WM_MBUTTONUP:
        event.key = VK_MBUTTON;
        event.down = false;
        break;
    case WM_XBUTTONDOWN:
        event.key = HIWORD(mouse->mouseData) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
        event.down = true;
        break;
    case WM_XBUTTONUP:
        event.key = HIWORD(mouse->mouseData) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
        event.down = false;
        break;
    default:
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    self->dispatch(event);
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void GlobalInputHook::runHookThread() {
    threadId_ = GetCurrentThreadId();
    keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardProc, GetModuleHandleW(nullptr), 0);
    mouseHook_ = SetWindowsHookExW(WH_MOUSE_LL, mouseProc, GetModuleHandleW(nullptr), 0);

    if (!keyboardHook_ || !mouseHook_) {
        setLastError(L"创建全局键鼠监听失败。如果安全软件拦截，可以尝试以管理员身份运行。");
        if (keyboardHook_) {
            UnhookWindowsHookEx(keyboardHook_);
            keyboardHook_ = nullptr;
        }
        if (mouseHook_) {
            UnhookWindowsHookEx(mouseHook_);
            mouseHook_ = nullptr;
        }
        running_.store(false);
        ready_.store(true);
        return;
    }

    running_.store(true);
    ready_.store(true);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (keyboardHook_) {
        UnhookWindowsHookEx(keyboardHook_);
        keyboardHook_ = nullptr;
    }
    if (mouseHook_) {
        UnhookWindowsHookEx(mouseHook_);
        mouseHook_ = nullptr;
    }
    running_.store(false);
}

void GlobalInputHook::dispatch(const InputEvent& event) {
    Callback callback;
    {
        std::lock_guard lock(mutex_);
        callback = callback_;
    }
    if (callback) {
        callback(event);
    }
}

void GlobalInputHook::setLastError(std::wstring message) {
    std::lock_guard lock(mutex_);
    lastError_ = std::move(message);
}

namespace NativeInput {

bool isMouseKey(KeyCode key) {
    return key == VK_LBUTTON || key == VK_RBUTTON || key == VK_MBUTTON || key == VK_XBUTTON1 || key == VK_XBUTTON2;
}

KeyCode normalizeKey(KeyCode key) {
    switch (key) {
    case VK_LCONTROL:
    case VK_RCONTROL:
        return VK_CONTROL;
    case VK_LSHIFT:
    case VK_RSHIFT:
        return VK_SHIFT;
    case VK_LMENU:
    case VK_RMENU:
        return VK_MENU;
    default:
        return key;
    }
}

void sendKeyState(KeyCode key, bool down) {
    if (key == Key::None) {
        return;
    }

    INPUT input{};
    if (isMouseKey(key)) {
        fillMouseInput(input, key, down);
    } else {
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = static_cast<WORD>(key);
        input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(static_cast<UINT>(key), MAPVK_VK_TO_VSC));
        input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
        if (isExtendedKey(key)) {
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        }
        input.ki.dwExtraInfo = kInjectedExtraInfo;
    }

    SendInput(1, &input, sizeof(INPUT));
}

void clickKey(KeyCode key) {
    sendKeyState(key, true);
    Sleep(2);
    sendKeyState(key, false);
}

void sendRecordedInput(const RecordedInput& input) {
    if (input.hasPosition && isMouseKey(input.key)) {
        SetCursorPos(input.x, input.y);
    }
    sendKeyState(input.key, input.down);
}

std::wstring keyName(KeyCode key) {
    switch (key) {
    case VK_LBUTTON:
        return L"鼠标左键";
    case VK_RBUTTON:
        return L"鼠标右键";
    case VK_MBUTTON:
        return L"鼠标中键";
    case VK_XBUTTON1:
        return L"鼠标侧键1";
    case VK_XBUTTON2:
        return L"鼠标侧键2";
    case VK_CONTROL:
        return L"Ctrl";
    case VK_SHIFT:
        return L"Shift";
    case VK_MENU:
        return L"Alt";
    case VK_ESCAPE:
        return L"Esc";
    case VK_SPACE:
        return L"空格";
    default:
        break;
    }

    if (key >= VK_F1 && key <= VK_F24) {
        return L"F" + std::to_wstring(key - VK_F1 + 1);
    }
    if (key >= '0' && key <= '9') {
        return std::wstring(1, static_cast<wchar_t>(key));
    }
    if (key >= 'A' && key <= 'Z') {
        return std::wstring(1, static_cast<wchar_t>(key));
    }

    const UINT scan = MapVirtualKeyW(static_cast<UINT>(key), MAPVK_VK_TO_VSC);
    wchar_t name[64] = {};
    const LONG lParam = static_cast<LONG>(scan << 16);
    if (GetKeyNameTextW(lParam, name, 64) > 0) {
        return name;
    }
    return L"VK " + std::to_wstring(key);
}

} // namespace NativeInput

} // namespace lc
