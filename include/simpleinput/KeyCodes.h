#pragma once

namespace lc {

using KeyCode = int;

namespace Key {
inline constexpr KeyCode None = 0x00;
inline constexpr KeyCode LButton = 0x01;
inline constexpr KeyCode RButton = 0x02;
inline constexpr KeyCode MButton = 0x04;
inline constexpr KeyCode XButton1 = 0x05;
inline constexpr KeyCode XButton2 = 0x06;
inline constexpr KeyCode Backspace = 0x08;
inline constexpr KeyCode Tab = 0x09;
inline constexpr KeyCode Enter = 0x0D;
inline constexpr KeyCode Shift = 0x10;
inline constexpr KeyCode Control = 0x11;
inline constexpr KeyCode Alt = 0x12;
inline constexpr KeyCode Escape = 0x1B;
inline constexpr KeyCode Space = 0x20;
inline constexpr KeyCode F6 = 0x75;
inline constexpr KeyCode F7 = 0x76;
inline constexpr KeyCode F8 = 0x77;
inline constexpr KeyCode F9 = 0x78;
inline constexpr KeyCode F10 = 0x79;
inline constexpr KeyCode F11 = 0x7A;
inline constexpr KeyCode F12 = 0x7B;
} // namespace Key

} // namespace lc
