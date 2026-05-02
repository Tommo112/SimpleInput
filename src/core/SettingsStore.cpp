#include "simpleinput/SettingsStore.h"

#include <charconv>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace lc {

namespace {

std::string trim(std::string_view value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(begin, end - begin + 1));
}

std::string readAll(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string quotedKey(std::string_view key) {
    return "\"" + std::string(key) + "\"";
}

std::string_view valueAfterKey(std::string_view json, std::string_view key) {
    const auto keyPos = json.find(quotedKey(key));
    if (keyPos == std::string_view::npos) {
        return {};
    }
    const auto colon = json.find(':', keyPos);
    if (colon == std::string_view::npos) {
        return {};
    }
    const auto valueStart = json.find_first_not_of(" \t\r\n", colon + 1);
    if (valueStart == std::string_view::npos) {
        return {};
    }
    return json.substr(valueStart);
}

bool parseInt(std::string_view value, int& out) {
    const auto text = trim(value);
    const auto* first = text.data();
    const auto* last = first;
    while (last < first + text.size() && (*last == '-' || (*last >= '0' && *last <= '9'))) {
        ++last;
    }
    int parsed = 0;
    const auto result = std::from_chars(first, last, parsed);
    if (first == last || result.ec != std::errc{} || result.ptr != last) {
        return false;
    }
    out = parsed;
    return true;
}

bool parseUInt64(std::string_view value, std::uint64_t& out) {
    const auto text = trim(value);
    const auto* first = text.data();
    const auto* last = first;
    while (last < first + text.size() && *last >= '0' && *last <= '9') {
        ++last;
    }
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(first, last, parsed);
    if (first == last || result.ec != std::errc{} || result.ptr != last) {
        return false;
    }
    out = parsed;
    return true;
}

bool parseDouble(std::string_view value, double& out) {
    const auto text = trim(value);
    std::size_t length = 0;
    while (length < text.size()) {
        const char ch = text[length];
        if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E') {
            ++length;
        } else {
            break;
        }
    }
    std::istringstream stream(std::string(text.substr(0, length)));
    double parsed = 0.0;
    stream >> parsed;
    if (!stream || !stream.eof()) {
        return false;
    }
    out = parsed;
    return true;
}

bool parseBool(std::string_view value, bool& out) {
    const auto text = trim(value);
    if (text.starts_with("1") || text.starts_with("true")) {
        out = true;
        return true;
    }
    if (text.starts_with("0") || text.starts_with("false")) {
        out = false;
        return true;
    }
    return false;
}

bool parseString(std::string_view value, std::string& out) {
    const auto text = trim(value);
    if (text.empty() || text.front() != '"') {
        return false;
    }
    const auto end = text.find('"', 1);
    if (end == std::string_view::npos) {
        return false;
    }
    out = std::string(text.substr(1, end - 1));
    return true;
}

void parseJsonValue(std::string_view json, std::string_view key, bool& out) {
    bool parsed = false;
    if (parseBool(valueAfterKey(json, key), parsed)) {
        out = parsed;
    }
}

void parseJsonValue(std::string_view json, std::string_view key, int& out) {
    int parsed = 0;
    if (parseInt(valueAfterKey(json, key), parsed)) {
        out = parsed;
    }
}

void parseJsonValue(std::string_view json, std::string_view key, std::uint64_t& out) {
    std::uint64_t parsed = 0;
    if (parseUInt64(valueAfterKey(json, key), parsed)) {
        out = parsed;
    }
}

void parseJsonValue(std::string_view json, std::string_view key, double& out) {
    double parsed = 0.0;
    if (parseDouble(valueAfterKey(json, key), parsed)) {
        out = parsed;
    }
}

std::string parseJsonString(std::string_view json, std::string_view key, std::string fallback) {
    std::string parsed;
    if (parseString(valueAfterKey(json, key), parsed)) {
        return parsed;
    }
    return fallback;
}

RecordedInput parseRecordedInput(std::string_view object) {
    RecordedInput input;
    parseJsonValue(object, "key", input.key);
    parseJsonValue(object, "down", input.down);
    parseJsonValue(object, "delayMs", input.delayMs);
    parseJsonValue(object, "x", input.x);
    parseJsonValue(object, "y", input.y);
    parseJsonValue(object, "hasPosition", input.hasPosition);
    return input;
}

std::vector<RecordedInput> parseRecording(std::string_view json) {
    std::vector<RecordedInput> recording;
    const auto recordingValue = valueAfterKey(json, "recording");
    const auto arrayStart = recordingValue.find('[');
    if (arrayStart == std::string_view::npos) {
        return recording;
    }

    auto cursor = arrayStart + 1;
    while (cursor < recordingValue.size()) {
        const auto objectStart = recordingValue.find('{', cursor);
        if (objectStart == std::string_view::npos) {
            break;
        }
        const auto objectEnd = recordingValue.find('}', objectStart + 1);
        if (objectEnd == std::string_view::npos) {
            break;
        }
        recording.push_back(parseRecordedInput(recordingValue.substr(objectStart, objectEnd - objectStart + 1)));
        cursor = objectEnd + 1;
    }
    return recording;
}

} // namespace

AppSettings loadSettings(const std::filesystem::path& path) {
    auto settings = AppSettings::defaults();
    const auto json = readAll(path);
    if (json.empty()) {
        return settings;
    }

    parseJsonValue(json, "globalEnabled", settings.globalEnabled);
    parseJsonValue(json, "clickKey", settings.clickKey);
    parseJsonValue(json, "triggerKey", settings.triggerKey);
    parseJsonValue(json, "globalToggleKey", settings.globalToggleKey);
    parseJsonValue(json, "recordKey", settings.recordKey);
    parseJsonValue(json, "playbackKey", settings.playbackKey);
    parseJsonValue(json, "cps", settings.cps);
    settings.mode = parseJsonString(json, "mode", "toggle") == "hold" ? TriggerMode::Hold : TriggerMode::Toggle;
    settings.recording = parseRecording(json);
    settings.sanitize();
    return settings;
}

bool saveSettings(const std::filesystem::path& path, const AppSettings& settings) {
    std::error_code error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return false;
        }
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return false;
    }

    file << "{\n";
    file << "  \"globalEnabled\": " << (settings.globalEnabled ? "true" : "false") << ",\n";
    file << "  \"clickKey\": " << settings.clickKey << ",\n";
    file << "  \"triggerKey\": " << settings.triggerKey << ",\n";
    file << "  \"globalToggleKey\": " << settings.globalToggleKey << ",\n";
    file << "  \"recordKey\": " << settings.recordKey << ",\n";
    file << "  \"playbackKey\": " << settings.playbackKey << ",\n";
    file << "  \"cps\": " << settings.cps << ",\n";
    file << "  \"mode\": \"" << (settings.mode == TriggerMode::Hold ? "hold" : "toggle") << "\",\n";
    file << "  \"recording\": [\n";

    for (std::size_t i = 0; i < settings.recording.size(); ++i) {
        const auto& input = settings.recording[i];
        file << "    {\"key\": " << input.key << ", \"down\": " << (input.down ? "true" : "false")
             << ", \"delayMs\": " << input.delayMs << ", \"x\": " << input.x << ", \"y\": " << input.y
             << ", \"hasPosition\": " << (input.hasPosition ? "true" : "false") << "}";
        if (i + 1 < settings.recording.size()) {
            file << ',';
        }
        file << '\n';
    }
    file << "  ]\n";
    file << "}\n";
    return static_cast<bool>(file);
}

} // namespace lc
