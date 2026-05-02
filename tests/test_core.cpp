#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "simpleinput/AutomationEngine.h"
#include "simpleinput/KeyCodes.h"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool hasCommand(const std::vector<lc::EngineCommand>& commands, lc::EngineCommandType type) {
    for (const auto& command : commands) {
        if (command.type == type) {
            return true;
        }
    }
    return false;
}

void defaultSettingsAreSafe() {
    const auto settings = lc::AppSettings::defaults();
    require(!settings.globalEnabled, "global trigger must be disabled by default");
    require(settings.globalToggleKey == lc::Key::F8, "global toggle must default to F8");
    require(settings.recordKey == lc::Key::F9, "record key must default to F9");
    require(settings.playbackKey == lc::Key::F10, "playback key must default to F10");
    require(settings.clickKey == lc::Key::LButton, "click key must default to left button");
    require(settings.triggerKey == lc::Key::LButton, "trigger key must default to left button");
    require(settings.cps == 10.0, "cps must default to 10");
}

void globalToggleStopsClickerWhenDisabled() {
    lc::AutomationEngine engine(lc::AppSettings::defaults());

    auto commands = engine.handleInput({lc::Key::F8, true, false, 0});
    require(engine.settings().globalEnabled, "F8 down should enable global trigger");
    require(hasCommand(commands, lc::EngineCommandType::GlobalEnabled), "enable command expected");
    engine.handleInput({lc::Key::F8, false, false, 1});

    commands = engine.handleInput({lc::Key::LButton, true, false, 10});
    require(engine.isClicking(), "trigger should start clicker after global enable");
    require(hasCommand(commands, lc::EngineCommandType::StartClicker), "start clicker command expected");

    commands = engine.handleInput({lc::Key::F8, true, false, 20});
    require(!engine.settings().globalEnabled, "second F8 down should disable global trigger");
    require(!engine.isClicking(), "disabling global trigger must stop active clicker");
    require(hasCommand(commands, lc::EngineCommandType::StopClicker), "stop clicker command expected");
}

void toggleModeIgnoresInjectedSameKeyClicks() {
    auto settings = lc::AppSettings::defaults();
    settings.globalEnabled = true;
    settings.mode = lc::TriggerMode::Toggle;
    settings.clickKey = lc::Key::LButton;
    settings.triggerKey = lc::Key::LButton;
    lc::AutomationEngine engine(settings);

    auto commands = engine.handleInput({lc::Key::LButton, true, false, 0});
    require(engine.isClicking(), "physical same-key trigger should start clicker");
    require(hasCommand(commands, lc::EngineCommandType::StartClicker), "start command expected");

    commands = engine.handleInput({lc::Key::LButton, true, true, 1});
    require(engine.isClicking(), "injected down from clicker must not toggle off");
    require(commands.empty(), "injected down should produce no commands");

    commands = engine.handleInput({lc::Key::LButton, false, true, 2});
    require(engine.isClicking(), "injected up from clicker must not toggle off");
    require(commands.empty(), "injected up should produce no commands");

    commands = engine.handleInput({lc::Key::LButton, false, false, 3});
    require(engine.isClicking(), "toggle mode should not stop on physical release");

    commands = engine.handleInput({lc::Key::LButton, true, false, 4});
    require(!engine.isClicking(), "next physical press should stop clicker");
    require(hasCommand(commands, lc::EngineCommandType::StopClicker), "stop command expected");
}

void holdModeStartsAndStopsOnPhysicalEdges() {
    auto settings = lc::AppSettings::defaults();
    settings.globalEnabled = true;
    settings.mode = lc::TriggerMode::Hold;
    settings.clickKey = lc::Key::LButton;
    settings.triggerKey = lc::Key::LButton;
    lc::AutomationEngine engine(settings);

    auto commands = engine.handleInput({lc::Key::LButton, true, false, 0});
    require(engine.isClicking(), "hold mode should start on physical press");
    require(hasCommand(commands, lc::EngineCommandType::StartClicker), "start command expected");

    commands = engine.handleInput({lc::Key::LButton, false, true, 1});
    require(engine.isClicking(), "injected up must not stop hold mode");
    require(commands.empty(), "injected up should produce no commands");

    commands = engine.handleInput({lc::Key::LButton, false, false, 2});
    require(!engine.isClicking(), "hold mode should stop on physical release");
    require(hasCommand(commands, lc::EngineCommandType::StopClicker), "stop command expected");
}

void repeatedPhysicalDownDoesNotDoubleToggle() {
    auto settings = lc::AppSettings::defaults();
    settings.globalEnabled = true;
    settings.mode = lc::TriggerMode::Toggle;
    settings.triggerKey = lc::Key::F6;
    lc::AutomationEngine engine(settings);

    engine.handleInput({lc::Key::F6, true, false, 0});
    require(engine.isClicking(), "first down should start");

    const auto commands = engine.handleInput({lc::Key::F6, true, false, 1});
    require(engine.isClicking(), "repeated down should not toggle");
    require(commands.empty(), "repeated down should not emit commands");

    engine.handleInput({lc::Key::F6, false, false, 2});
    engine.handleInput({lc::Key::F6, true, false, 3});
    require(!engine.isClicking(), "new down after release should toggle off");
}

void recordingKeyTogglesAndSavesEvents() {
    lc::AutomationEngine engine(lc::AppSettings::defaults());

    auto commands = engine.handleInput({lc::Key::F9, true, false, 100});
    require(engine.isRecording(), "F9 should start recording");
    require(hasCommand(commands, lc::EngineCommandType::RecordingStarted), "recording started command expected");
    engine.handleInput({lc::Key::F9, false, false, 110});

    commands = engine.handleInput({lc::Key::LButton, true, false, 150, 10, 20, true});
    require(commands.empty(), "normal recorded input should not emit commands");

    commands = engine.handleInput({lc::Key::LButton, false, false, 190, 10, 20, true});
    require(commands.empty(), "normal recorded input should not emit commands");

    commands = engine.handleInput({lc::Key::F9, true, false, 250});
    require(!engine.isRecording(), "second F9 should stop recording");
    require(hasCommand(commands, lc::EngineCommandType::RecordingSaved), "recording saved command expected");
    require(engine.settings().recording.size() == 2, "recording should contain two captured events");
    require(engine.settings().recording[0].delayMs == 0, "first event delay should be zero");
    require(engine.settings().recording[1].delayMs == 40, "second event delay should preserve timing");
    require(engine.settings().recording[0].hasPosition, "mouse button event should keep cursor position");
}

void playbackKeyEmitsSavedRecording() {
    auto settings = lc::AppSettings::defaults();
    settings.recording.push_back({lc::Key::LButton, true, 0, 1, 2, true});
    settings.recording.push_back({lc::Key::LButton, false, 30, 1, 2, true});
    lc::AutomationEngine engine(settings);

    const auto commands = engine.handleInput({lc::Key::F10, true, false, 0});
    require(hasCommand(commands, lc::EngineCommandType::PlayRecording), "playback command expected");
    require(commands[0].recording.size() == 2, "playback command should carry saved recording");
}

} // namespace

int main() {
    const std::vector<std::pair<std::string, void (*)()>> tests = {
        {"defaultSettingsAreSafe", defaultSettingsAreSafe},
        {"globalToggleStopsClickerWhenDisabled", globalToggleStopsClickerWhenDisabled},
        {"toggleModeIgnoresInjectedSameKeyClicks", toggleModeIgnoresInjectedSameKeyClicks},
        {"holdModeStartsAndStopsOnPhysicalEdges", holdModeStartsAndStopsOnPhysicalEdges},
        {"repeatedPhysicalDownDoesNotDoubleToggle", repeatedPhysicalDownDoesNotDoubleToggle},
        {"recordingKeyTogglesAndSavesEvents", recordingKeyTogglesAndSavesEvents},
        {"playbackKeyEmitsSavedRecording", playbackKeyEmitsSavedRecording},
    };

    for (const auto& test : tests) {
        try {
            test.second();
            std::cout << "[PASS] " << test.first << '\n';
        } catch (const std::exception& ex) {
            std::cerr << "[FAIL] " << test.first << ": " << ex.what() << '\n';
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
