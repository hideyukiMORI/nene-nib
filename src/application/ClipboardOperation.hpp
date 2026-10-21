#pragma once

#include <cstdint>

namespace nenenib::application
{
// クリップボードに対してすることの閉じた一覧（CPP-002）。
enum class ClipboardOperation : std::uint8_t
{
    copy,
    cut,
    // Ctrl+V。文書の言語とクリップボードの HTML で貼り方が決まる（ADR 0157 の決定 1）。
    paste,
    // Ctrl+Shift+V。いつも文字だけ（決定 2）。
    paste_plain,
    // 「HTML のまま貼り付け」。どの文書でも HTML の fragment をそのまま（決定 3）。
    paste_html
};
} // namespace nenenib::application
