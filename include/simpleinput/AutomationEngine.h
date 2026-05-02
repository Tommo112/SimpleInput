#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "simpleinput/KeyCodes.h"

namespace lc {

enum class TriggerMode {
    Toggle,
    Hold,
};

struct RecordedInput {
    KeyCode key = Key::None;
    bool down = false;
    std::uint64_t delayMs = 0;
    int x = 0;
    int y = 0;
    bool hasPosition = false;
};

struct AppSettings {
    bool globalEnabled = false;
    KeyCode clickKey = Key::LButton;
    KeyCode triggerKey = Key::LButton;
    KeyCode globalToggleKey = Key::F8;
    KeyCode recordKey = Key::F9;
    KeyCode playbackKey = Key::F10;
    double cps = 10.0;
    TriggerMode mode = TriggerMode::Toggle;
    std::vector<RecordedInput> recording;

    static AppSettings defaults();
    void sanitize();
};

struct InputEvent {
    KeyCode key = Key::None;
    bool down = false;
    bool injected = false;
    std::uint64_t timeMs = 0;
    int x = 0;
    int y = 0;
    bool hasPosition = false;
};

enum class EngineCommandType {
    GlobalEnabled,
    GlobalDisabled,
    StartClicker,
    StopClicker,
    RecordingStarted,
    RecordingSaved,
    PlayRecording,
};

struct EngineCommand {
    EngineCommandType type = EngineCommandType::GlobalDisabled;
    KeyCode clickKey = Key::None;
    double cps = 0.0;
    std::vector<RecordedInput> recording;
};

class AutomationEngine {
public:
    explicit AutomationEngine(AppSettings settings = AppSettings::defaults());

    [[nodiscard]] const AppSettings& settings() const;
    void setSettings(AppSettings settings);

    [[nodiscard]] bool isClicking() const;
    [[nodiscard]] bool isRecording() const;

    std::vector<EngineCommand> handleInput(const InputEvent& event);

private:
    AppSettings settings_;
    bool clicking_ = false;
    bool recording_ = false;
    std::uint64_t lastRecordTimeMs_ = 0;
    std::vector<RecordedInput> draftRecording_;
    std::array<bool, 256> physicalDown_{};

    [[nodiscard]] bool isPhysicalDown(KeyCode key) const;
    void setPhysicalDown(KeyCode key, bool down);
    void startClicking(std::vector<EngineCommand>& commands);
    void stopClicking(std::vector<EngineCommand>& commands);
    void startRecording(std::vector<EngineCommand>& commands);
    void stopRecording(std::vector<EngineCommand>& commands);
    void appendRecordedInput(const InputEvent& event);
    [[nodiscard]] bool isControlKey(KeyCode key) const;
};

} // namespace lc
