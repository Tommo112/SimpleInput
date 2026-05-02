# SimpleInput

SimpleInput 是一个 Windows 简易连点器，支持连点、按键录制和回放。程序为原生 Win32 界面.

## 使用方式

1. 解压发布包，双击 `SimpleInput.exe`。
2. 勾选 `允许全局触发` 后，触发键才会生效。
3. 默认热键：
   - 全局开关：`F8`
   - 开始/停止录制：`F9`
   - 回放录制：`F10`
4. 连点键是自动点击的按键，触发键是启动或停止连点的按键；两个键可以相同。
5. 模式支持点击触发和按住触发，CPS 用来设置每秒点击次数。

配置会保存在程序同目录的 `config.json`。

## 手动编译

需要 Windows、CMake、Ninja 和 GCC/MinGW。

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```
