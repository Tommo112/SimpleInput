#include "simpleinput/AutomationEngine.h"

#include <algorithm>

namespace lc {

namespace {

bool canTrackKey(KeyCode key) {
    return key >= 0 && key < 256;
}

EngineCommand makeClickCommand(EngineCommandType type, KeyCode clickKey, double cps) {
    EngineCommand command;
    command.type = type;
    command.clickKey = clickKey;
    command.cps = cps;
    return command;
}

EngineCommand makeSimpleCommand(EngineCommandType type) {
    EngineCommand command;
    command.type = type;
    return command;
}

} // namespace

AppSettings AppSettings::defaults() {
    return {};
}

void AppSettings::sanitize() {
    if (clickKey == Key::None) {
        clickKey = Key::LButton;
    }
    if (triggerKey == Key::None) {
        triggerKey = clickKey;
    }
    if (globalToggleKey == Key::None) {
        globalToggleKey = Key::F8;
    }
    if (recordKey == Key::None) {
        recordKey = Key::F9;
    }
    if (playbackKey == Key::None) {
        playbackKey = Key::F10;
    }
    cps = std::clamp(cps, 1.0, 1000.0);
}

AutomationEngine::AutomationEngine(AppSettings settings) {
    setSettings(std::move(settings));
}

const AppSettings& AutomationEngine::settings() const {
    return settings_;
}

void AutomationEngine::setSettings(AppSettings settings) {
    settings.sanitize();
    settings_ = std::move(settings);
    if (!settings_.globalEnabled) {
        clicking_ = false;
    }
}

bool AutomationEngine::isClicking() const {
    return clicking_;
}

bool AutomationEngine::isRecording() const {
    return recording_;
}

std::vector<EngineCommand> AutomationEngine::handleInput(const InputEvent& event) {
    std::vector<EngineCommand> commands;
    if (event.key == Key::None || event.injected) {
        return commands;
    }

    const bool wasDown = isPhysicalDown(event.key);
    if (event.down && wasDown) {
        return commands;
    }
    setPhysicalDown(event.key, event.down);

    if (event.down && event.key == settings_.globalToggleKey) {
        settings_.globalEnabled = !settings_.globalEnabled;
        commands.push_back(makeSimpleCommand(settings_.globalEnabled ? EngineCommandType::GlobalEnabled
                                                                     : EngineCommandType::GlobalDisabled));
        if (!settings_.globalEnabled) {
            stopClicking(commands);
        }
        return commands;
    }

    if (event.down && event.key == settings_.recordKey) {
        if (recording_) {
            stopRecording(commands);
        } else {
            startRecording(commands);
        }
        return commands;
    }

    if (recording_) {
        if (!isControlKey(event.key)) {
            appendRecordedInput(event);
        }
        return commands;
    }

    if (event.down && event.key == settings_.playbackKey && !settings_.recording.empty()) {
        EngineCommand command;
        command.type = EngineCommandType::PlayRecording;
        command.recording = settings_.recording;
        commands.push_back(std::move(command));
        return commands;
    }

    if (!settings_.globalEnabled || event.key != settings_.triggerKey) {
        return commands;
    }

    if (settings_.mode == TriggerMode::Toggle) {
        if (event.down) {
            if (clicking_) {
                stopClicking(commands);
            } else {
                startClicking(commands);
            }
        }
    } else {
        if (event.down) {
            startClicking(commands);
        } else {
            stopClicking(commands);
        }
    }

    return commands;
}

bool AutomationEngine::isPhysicalDown(KeyCode key) const {
    return canTrackKey(key) && physicalDown_[static_cast<std::size_t>(key)];
}

void AutomationEngine::setPhysicalDown(KeyCode key, bool down) {
    if (canTrackKey(key)) {
        physicalDown_[static_cast<std::size_t>(key)] = down;
    }
}

void AutomationEngine::startClicking(std::vector<EngineCommand>& commands) {
    if (clicking_) {
        return;
    }
    clicking_ = true;
    commands.push_back(makeClickCommand(EngineCommandType::StartClicker, settings_.clickKey, settings_.cps));
}

void AutomationEngine::stopClicking(std::vector<EngineCommand>& commands) {
    if (!clicking_) {
        return;
    }
    clicking_ = false;
    commands.push_back(makeClickCommand(EngineCommandType::StopClicker, settings_.clickKey, settings_.cps));
}

void AutomationEngine::startRecording(std::vector<EngineCommand>& commands) {
    stopClicking(commands);
    draftRecording_.clear();
    lastRecordTimeMs_ = 0;
    recording_ = true;
    commands.push_back(makeSimpleCommand(EngineCommandType::RecordingStarted));
}

void AutomationEngine::stopRecording(std::vector<EngineCommand>& commands) {
    recording_ = false;
    settings_.recording = draftRecording_;
    draftRecording_.clear();
    lastRecordTimeMs_ = 0;
    commands.push_back(makeSimpleCommand(EngineCommandType::RecordingSaved));
}

void AutomationEngine::appendRecordedInput(const InputEvent& event) {
    RecordedInput input;
    input.key = event.key;
    input.down = event.down;
    input.delayMs = draftRecording_.empty() ? 0 : event.timeMs - lastRecordTimeMs_;
    input.x = event.x;
    input.y = event.y;
    input.hasPosition = event.hasPosition;

    draftRecording_.push_back(input);
    lastRecordTimeMs_ = event.timeMs;
}

bool AutomationEngine::isControlKey(KeyCode key) const {
    return key == settings_.globalToggleKey || key == settings_.recordKey || key == settings_.playbackKey;
}

} // namespace lc
