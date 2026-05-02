#include <windows.h>

#include "simpleinput/NativeInput.h"
#include "platform/windows/MainWindow.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    lc::TimerResolution timerResolution;
    return lc::runWindowsApplication(instance, showCommand);
}
