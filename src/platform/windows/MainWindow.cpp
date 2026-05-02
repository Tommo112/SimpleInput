#include "platform/windows/MainWindow.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "simpleinput/NativeInput.h"
#include "simpleinput/Runners.h"
#include "simpleinput/SettingsStore.h"

namespace lc {

namespace {

constexpr UINT kInputMessage = WM_APP + 1;
constexpr UINT_PTR kStatusTimer = 1;

enum ControlId {
    IdGlobalEnabled = 100,
    IdGlobalToggleKey,
    IdClickKey,
    IdTriggerKey,
    IdMode,
    IdCps,
    IdRecordKey,
    IdPlaybackKey,
};

enum class CaptureTarget {
    None,
    GlobalToggleKey,
    ClickKey,
    TriggerKey,
    RecordKey,
    PlaybackKey,
};

std::filesystem::path executableSettingsPath() {
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = 0;
    for (;;) {
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return std::filesystem::path("config.json");
        }
        if (length < buffer.size() - 1) {
            buffer.resize(length);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer).parent_path() / "config.json";
}

std::wstring formatCps(double cps) {
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(1) << cps;
    return stream.str();
}

std::wstring controlText(HWND handle) {
    const int length = GetWindowTextLengthW(handle);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(handle, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

std::wstring statusWord(bool enabled) {
    return enabled ? L"开启" : L"关闭";
}

} // namespace

class MainWindow {
public:
    explicit MainWindow(HINSTANCE instance) : instance_(instance), configPath_(executableSettingsPath()),
                                              engine_(loadSettings(configPath_)) {}
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    ~MainWindow() {
        shutdown();
    }

    bool create();
    void show(int showCommand);

private:
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND globalEnabledCheck_ = nullptr;
    HWND globalToggleButton_ = nullptr;
    HWND clickKeyButton_ = nullptr;
    HWND triggerKeyButton_ = nullptr;
    HWND modeCombo_ = nullptr;
    HWND cpsEdit_ = nullptr;
    HWND recordKeyButton_ = nullptr;
    HWND playbackKeyButton_ = nullptr;
    HWND hookStatusLabel_ = nullptr;
    HWND runStatusLabel_ = nullptr;
    HWND recordingStatusLabel_ = nullptr;
    HWND configPathLabel_ = nullptr;
    HFONT font_ = nullptr;
    CaptureTarget capture_ = CaptureTarget::None;
    std::filesystem::path configPath_;
    AutomationEngine engine_;
    ClickRunner clickRunner_;
    PlaybackRunner playbackRunner_;
    GlobalInputHook inputHook_;
    bool updatingUi_ = false;
    bool shutdown_ = false;

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    HWND createControl(const wchar_t* className, const wchar_t* text, DWORD style, int x, int y, int width,
                       int height, int id = 0, DWORD exStyle = 0);
    void buildUi();
    void startInputHook();
    void handleCommand(int id, int code);
    void beginCapture(CaptureTarget target);
    void finishCapture(KeyCode key);
    HWND buttonForCapture(CaptureTarget target) const;
    void handleInputEvent(const InputEvent& event);
    void applyCommands(const std::vector<EngineCommand>& commands);
    void applyUiSettings(bool syncAfterApply);
    void syncUiFromSettings();
    void refreshStatus();
    void saveCurrentSettings();
    void shutdown();
};

bool MainWindow::create() {
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = MainWindow::windowProc;
    windowClass.hInstance = instance_;
    windowClass.lpszClassName = L"SimpleInputWindow";
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);

    RegisterClassW(&windowClass);

    hwnd_ = CreateWindowExW(0, windowClass.lpszClassName, L"SimpleInput",
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
                            520, 455, nullptr, nullptr, instance_, this);
    if (!hwnd_) {
        return false;
    }

    buildUi();
    syncUiFromSettings();
    startInputHook();
    refreshStatus();
    SetTimer(hwnd_, kStatusTimer, 250, nullptr);
    return true;
}

void MainWindow::show(int showCommand) {
    ShowWindow(hwnd_, showCommand);
    UpdateWindow(hwnd_);
}

LRESULT CALLBACK MainWindow::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->handleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT MainWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND:
        handleCommand(LOWORD(wParam), HIWORD(wParam));
        return 0;
    case WM_TIMER:
        if (wParam == kStatusTimer) {
            refreshStatus();
        }
        return 0;
    case kInputMessage: {
        std::unique_ptr<InputEvent> event(reinterpret_cast<InputEvent*>(lParam));
        if (event) {
            handleInputEvent(*event);
        }
        return 0;
    }
    case WM_DESTROY:
        shutdown();
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

HWND MainWindow::createControl(const wchar_t* className, const wchar_t* text, DWORD style, int x, int y, int width,
                               int height, int id, DWORD exStyle) {
    HWND control = CreateWindowExW(exStyle, className, text, WS_CHILD | WS_VISIBLE | style, x, y, width, height, hwnd_,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    return control;
}

void MainWindow::buildUi() {
    font_ = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    createControl(L"BUTTON", L"连点", BS_GROUPBOX, 12, 10, 480, 212);
    globalEnabledCheck_ = createControl(L"BUTTON", L"允许全局触发", BS_AUTOCHECKBOX, 32, 34, 200, 24,
                                        IdGlobalEnabled);
    createControl(L"STATIC", L"全局开关键", 0, 32, 68, 130, 22);
    globalToggleButton_ = createControl(L"BUTTON", L"", BS_PUSHBUTTON, 170, 64, 140, 26, IdGlobalToggleKey);
    createControl(L"STATIC", L"连点键", 0, 32, 100, 130, 22);
    clickKeyButton_ = createControl(L"BUTTON", L"", BS_PUSHBUTTON, 170, 96, 140, 26, IdClickKey);
    createControl(L"STATIC", L"触发键", 0, 32, 132, 130, 22);
    triggerKeyButton_ = createControl(L"BUTTON", L"", BS_PUSHBUTTON, 170, 128, 140, 26, IdTriggerKey);
    createControl(L"STATIC", L"触发模式", 0, 32, 164, 130, 22);
    modeCombo_ = createControl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 170, 160, 160, 120, IdMode);
    SendMessageW(modeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"点击触发"));
    SendMessageW(modeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"按住触发"));
    createControl(L"STATIC", L"CPS", 0, 32, 196, 130, 22);
    cpsEdit_ = createControl(L"EDIT", L"", ES_AUTOHSCROLL, 170, 192, 80, 24, IdCps, WS_EX_CLIENTEDGE);

    createControl(L"BUTTON", L"录制与回放", BS_GROUPBOX, 12, 230, 480, 104);
    createControl(L"STATIC", L"录制键", 0, 32, 264, 130, 22);
    recordKeyButton_ = createControl(L"BUTTON", L"", BS_PUSHBUTTON, 170, 260, 140, 26, IdRecordKey);
    createControl(L"STATIC", L"回放键", 0, 32, 296, 130, 22);
    playbackKeyButton_ = createControl(L"BUTTON", L"", BS_PUSHBUTTON, 170, 292, 140, 26, IdPlaybackKey);
    recordingStatusLabel_ = createControl(L"STATIC", L"", 0, 324, 264, 150, 44);

    createControl(L"BUTTON", L"状态", BS_GROUPBOX, 12, 342, 480, 74);
    hookStatusLabel_ = createControl(L"STATIC", L"", 0, 32, 370, 170, 20);
    runStatusLabel_ = createControl(L"STATIC", L"", 0, 210, 370, 260, 20);
    configPathLabel_ = createControl(L"STATIC", L"", SS_PATHELLIPSIS, 32, 394, 438, 18);
}

void MainWindow::startInputHook() {
    const bool started = inputHook_.start([this](const InputEvent& event) {
        auto* copy = new InputEvent(event);
        if (!PostMessageW(hwnd_, kInputMessage, 0, reinterpret_cast<LPARAM>(copy))) {
            delete copy;
        }
    });

    if (!started) {
        const auto message = inputHook_.lastError().empty()
                                 ? L"全局键鼠监听启动失败。如果安全软件拦截，可以尝试以管理员身份运行。"
                                 : inputHook_.lastError();
        MessageBoxW(hwnd_, message.c_str(), L"SimpleInput", MB_ICONWARNING | MB_OK);
    }
}

void MainWindow::handleCommand(int id, int code) {
    if (updatingUi_) {
        return;
    }

    if (code == BN_CLICKED) {
        switch (id) {
        case IdGlobalEnabled:
            applyUiSettings(false);
            return;
        case IdGlobalToggleKey:
            beginCapture(CaptureTarget::GlobalToggleKey);
            return;
        case IdClickKey:
            beginCapture(CaptureTarget::ClickKey);
            return;
        case IdTriggerKey:
            beginCapture(CaptureTarget::TriggerKey);
            return;
        case IdRecordKey:
            beginCapture(CaptureTarget::RecordKey);
            return;
        case IdPlaybackKey:
            beginCapture(CaptureTarget::PlaybackKey);
            return;
        default:
            break;
        }
    }

    if ((id == IdMode && code == CBN_SELCHANGE) || (id == IdCps && (code == EN_CHANGE || code == EN_KILLFOCUS))) {
        applyUiSettings(code == EN_KILLFOCUS);
    }
}

void MainWindow::beginCapture(CaptureTarget target) {
    capture_ = target;
    if (HWND button = buttonForCapture(target)) {
        SetWindowTextW(button, L"按任意键...");
    }
    SetFocus(hwnd_);
}

void MainWindow::finishCapture(KeyCode key) {
    auto settings = engine_.settings();
    key = NativeInput::normalizeKey(key);
    switch (capture_) {
    case CaptureTarget::GlobalToggleKey:
        settings.globalToggleKey = key;
        break;
    case CaptureTarget::ClickKey:
        settings.clickKey = key;
        break;
    case CaptureTarget::TriggerKey:
        settings.triggerKey = key;
        break;
    case CaptureTarget::RecordKey:
        settings.recordKey = key;
        break;
    case CaptureTarget::PlaybackKey:
        settings.playbackKey = key;
        break;
    case CaptureTarget::None:
        return;
    }

    capture_ = CaptureTarget::None;
    engine_.setSettings(settings);
    saveCurrentSettings();
    syncUiFromSettings();
    refreshStatus();
}

HWND MainWindow::buttonForCapture(CaptureTarget target) const {
    switch (target) {
    case CaptureTarget::GlobalToggleKey:
        return globalToggleButton_;
    case CaptureTarget::ClickKey:
        return clickKeyButton_;
    case CaptureTarget::TriggerKey:
        return triggerKeyButton_;
    case CaptureTarget::RecordKey:
        return recordKeyButton_;
    case CaptureTarget::PlaybackKey:
        return playbackKeyButton_;
    case CaptureTarget::None:
        return nullptr;
    }
    return nullptr;
}

void MainWindow::handleInputEvent(const InputEvent& event) {
    if (capture_ != CaptureTarget::None && event.down && !event.injected) {
        finishCapture(event.key);
        return;
    }

    const auto commands = engine_.handleInput(event);
    applyCommands(commands);
    if (!commands.empty()) {
        saveCurrentSettings();
        syncUiFromSettings();
        refreshStatus();
    }
}

void MainWindow::applyCommands(const std::vector<EngineCommand>& commands) {
    for (const auto& command : commands) {
        switch (command.type) {
        case EngineCommandType::StartClicker:
            clickRunner_.start(command.clickKey, command.cps, [](KeyCode key) { NativeInput::clickKey(key); });
            break;
        case EngineCommandType::StopClicker:
            clickRunner_.stop();
            break;
        case EngineCommandType::PlayRecording:
            playbackRunner_.start(command.recording, [](const RecordedInput& input) {
                NativeInput::sendRecordedInput(input);
            });
            break;
        case EngineCommandType::GlobalEnabled:
        case EngineCommandType::GlobalDisabled:
        case EngineCommandType::RecordingStarted:
        case EngineCommandType::RecordingSaved:
            break;
        }
    }
}

void MainWindow::applyUiSettings(bool syncAfterApply) {
    auto settings = engine_.settings();
    const bool wasClicking = engine_.isClicking();
    settings.globalEnabled = SendMessageW(globalEnabledCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    settings.mode = SendMessageW(modeCombo_, CB_GETCURSEL, 0, 0) == 1 ? TriggerMode::Hold : TriggerMode::Toggle;

    const auto cpsText = controlText(cpsEdit_);
    wchar_t* end = nullptr;
    const double cps = std::wcstod(cpsText.c_str(), &end);
    if (end != cpsText.c_str() && std::isfinite(cps)) {
        settings.cps = cps;
    }

    engine_.setSettings(settings);
    if (!engine_.settings().globalEnabled && wasClicking) {
        clickRunner_.stop();
    }
    saveCurrentSettings();
    if (syncAfterApply) {
        syncUiFromSettings();
    }
    refreshStatus();
}

void MainWindow::syncUiFromSettings() {
    updatingUi_ = true;
    const auto& settings = engine_.settings();
    SendMessageW(globalEnabledCheck_, BM_SETCHECK, settings.globalEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(globalToggleButton_, NativeInput::keyName(settings.globalToggleKey).c_str());
    SetWindowTextW(clickKeyButton_, NativeInput::keyName(settings.clickKey).c_str());
    SetWindowTextW(triggerKeyButton_, NativeInput::keyName(settings.triggerKey).c_str());
    SetWindowTextW(recordKeyButton_, NativeInput::keyName(settings.recordKey).c_str());
    SetWindowTextW(playbackKeyButton_, NativeInput::keyName(settings.playbackKey).c_str());
    SendMessageW(modeCombo_, CB_SETCURSEL, settings.mode == TriggerMode::Hold ? 1 : 0, 0);
    SetWindowTextW(cpsEdit_, formatCps(settings.cps).c_str());
    updatingUi_ = false;
}

void MainWindow::refreshStatus() {
    SetWindowTextW(hookStatusLabel_, (L"监听: " + statusWord(inputHook_.running())).c_str());
    const std::wstring runStatus = L"全局: " + statusWord(engine_.settings().globalEnabled) + L"   连点: " +
                                   statusWord(clickRunner_.active()) + L"   回放: " +
                                   statusWord(playbackRunner_.active());
    SetWindowTextW(runStatusLabel_, runStatus.c_str());

    const std::wstring recordStatus =
        std::wstring(engine_.isRecording() ? L"录制中" : L"未录制") + L"\r\n已保存: " +
        std::to_wstring(engine_.settings().recording.size()) + L" 个事件";
    SetWindowTextW(recordingStatusLabel_, recordStatus.c_str());
    SetWindowTextW(configPathLabel_, (L"配置: " + configPath_.wstring()).c_str());
}

void MainWindow::saveCurrentSettings() {
    saveSettings(configPath_, engine_.settings());
}

void MainWindow::shutdown() {
    if (shutdown_) {
        return;
    }
    shutdown_ = true;
    if (hwnd_) {
        KillTimer(hwnd_, kStatusTimer);
    }
    saveCurrentSettings();
    clickRunner_.stop();
    playbackRunner_.stop();
    inputHook_.stop();
}

int runWindowsApplication(HINSTANCE instance, int showCommand) {
    MainWindow window(instance);
    if (!window.create()) {
        MessageBoxW(nullptr, L"创建 SimpleInput 窗口失败。", L"SimpleInput", MB_ICONERROR | MB_OK);
        return EXIT_FAILURE;
    }
    window.show(showCommand);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return EXIT_SUCCESS;
}

} // namespace lc
