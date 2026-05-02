#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

#include "simpleinput/KeyCodes.h"
#include "simpleinput/SettingsStore.h"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path tempConfigPath() {
    return std::filesystem::temp_directory_path() / "simpleinput-settings-test.json";
}

void savingAndLoadingKeepsUserSettingsAndRecording() {
    auto settings = lc::AppSettings::defaults();
    settings.globalEnabled = true;
    settings.clickKey = lc::Key::RButton;
    settings.triggerKey = lc::Key::F6;
    settings.globalToggleKey = lc::Key::F7;
    settings.recordKey = lc::Key::F11;
    settings.playbackKey = lc::Key::F12;
    settings.cps = 25.5;
    settings.mode = lc::TriggerMode::Hold;
    settings.recording.push_back({lc::Key::LButton, true, 0, 10, 20, true});
    settings.recording.push_back({lc::Key::LButton, false, 35, 10, 20, true});

    const auto path = tempConfigPath();
    std::filesystem::remove(path);
    require(lc::saveSettings(path, settings), "saveSettings should write config file");

    const auto loaded = lc::loadSettings(path);
    require(loaded.globalEnabled, "globalEnabled should round-trip");
    require(loaded.clickKey == lc::Key::RButton, "clickKey should round-trip");
    require(loaded.triggerKey == lc::Key::F6, "triggerKey should round-trip");
    require(loaded.globalToggleKey == lc::Key::F7, "globalToggleKey should round-trip");
    require(loaded.recordKey == lc::Key::F11, "recordKey should round-trip");
    require(loaded.playbackKey == lc::Key::F12, "playbackKey should round-trip");
    require(loaded.cps == 25.5, "cps should round-trip");
    require(loaded.mode == lc::TriggerMode::Hold, "mode should round-trip");
    require(loaded.recording.size() == 2, "recording event count should round-trip");
    require(loaded.recording[1].delayMs == 35, "recording delay should round-trip");
    require(loaded.recording[0].hasPosition, "recording position flag should round-trip");

    const std::string content = [&] {
        std::ifstream file(path);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }();
    require(content.find('{') != std::string::npos, "config should be saved as a JSON object");
    require(content.find("\"globalEnabled\"") != std::string::npos, "JSON should contain quoted setting keys");
    require(content.find("\"recording\"") != std::string::npos, "JSON should contain recording array");
    std::filesystem::remove(path);
}

void loadingJsonConfigReadsSettings() {
    const auto path = tempConfigPath();
    std::filesystem::remove(path);
    {
        std::ofstream file(path);
        file << "{\n"
             << "  \"globalEnabled\": true,\n"
             << "  \"clickKey\": 2,\n"
             << "  \"triggerKey\": 117,\n"
             << "  \"globalToggleKey\": 118,\n"
             << "  \"recordKey\": 122,\n"
             << "  \"playbackKey\": 123,\n"
             << "  \"cps\": 30.5,\n"
             << "  \"mode\": \"hold\",\n"
             << "  \"recording\": [\n"
             << "    {\"key\": 1, \"down\": true, \"delayMs\": 0, \"x\": 11, \"y\": 22, \"hasPosition\": true},\n"
             << "    {\"key\": 1, \"down\": false, \"delayMs\": 44, \"x\": 11, \"y\": 22, \"hasPosition\": true}\n"
             << "  ]\n"
             << "}\n";
    }

    const auto loaded = lc::loadSettings(path);
    require(loaded.globalEnabled, "JSON globalEnabled should load");
    require(loaded.clickKey == lc::Key::RButton, "JSON clickKey should load");
    require(loaded.triggerKey == lc::Key::F6, "JSON triggerKey should load");
    require(loaded.mode == lc::TriggerMode::Hold, "JSON mode should load");
    require(loaded.cps == 30.5, "JSON cps should load");
    require(loaded.recording.size() == 2, "JSON recording should load");
    require(loaded.recording[1].delayMs == 44, "JSON recording delay should load");
    std::filesystem::remove(path);
}

void missingConfigLoadsDefaults() {
    const auto path = tempConfigPath();
    std::filesystem::remove(path);

    const auto loaded = lc::loadSettings(path);
    require(!loaded.globalEnabled, "missing config should keep global trigger disabled");
    require(loaded.globalToggleKey == lc::Key::F8, "missing config should keep F8 global toggle");
    require(loaded.recordKey == lc::Key::F9, "missing config should keep F9 record key");
    require(loaded.playbackKey == lc::Key::F10, "missing config should keep F10 playback key");
}

} // namespace

int main() {
    const std::pair<const char*, void (*)()> tests[] = {
        {"savingAndLoadingKeepsUserSettingsAndRecording", savingAndLoadingKeepsUserSettingsAndRecording},
        {"loadingJsonConfigReadsSettings", loadingJsonConfigReadsSettings},
        {"missingConfigLoadsDefaults", missingConfigLoadsDefaults},
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
