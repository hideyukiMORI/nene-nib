#include "EditorWindowDetail.hpp"

#include "Utf16.hpp"

#include <windows.h>

namespace nenenib::ui::win32::detail
{
namespace
{
constexpr SHORT key_down_mask = static_cast<SHORT>(0x8000);
} // namespace

[[nodiscard]] FontKey font_of(const core::EditorSettings &settings)
{
    return FontKey{settings.font_size.points(), std::string(settings.font_family.text())};
}

[[nodiscard]] bool held(int key) noexcept
{
    return (GetKeyState(key) & key_down_mask) != 0;
}

// UTF-8 の表示値を Win32 の UTF-16 へ。題名とダイアログの既定名だけが通る（CPP-014）。
// 変換そのものは core::to_utf16 ただ 1 本で、表示値は検証済みなので失敗しない（Issue #13）。
[[nodiscard]] std::wstring widen(std::string_view utf8)
{
    return core::to_utf16(utf8).value_or(std::wstring{});
}

std::int32_t low_word_of(LPARAM data) noexcept
{
    return static_cast<std::int32_t>(static_cast<short>(LOWORD(data)));
}

std::int32_t high_word_of(LPARAM data) noexcept
{
    return static_cast<std::int32_t>(static_cast<short>(HIWORD(data)));
}
} // namespace nenenib::ui::win32::detail
