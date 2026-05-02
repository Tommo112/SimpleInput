#pragma once

#include <filesystem>

#include "simpleinput/AutomationEngine.h"

namespace lc {

[[nodiscard]] AppSettings loadSettings(const std::filesystem::path& path);
bool saveSettings(const std::filesystem::path& path, const AppSettings& settings);

} // namespace lc
