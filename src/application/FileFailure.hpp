#pragma once

#include <cstdint>

namespace nenenib::application
{
// ファイルの失敗は期待される結果である（ARC-010 / CPP-005 / ADR 0010 の決定 9）。
// 開く側と保存する側で同じ閉じた集合を使い、UI は 1 行の説明に写すだけ。
enum class FileFailure : std::uint8_t
{
    not_found,
    access_denied,
    unreadable,
    unwritable,
    too_large,
    undecodable,
    unencodable
};
} // namespace nenenib::application
