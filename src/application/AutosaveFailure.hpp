#pragma once

namespace nenenib::application
{
// 自動退避の期待される失敗（ADR 0137 の決定 4・ARC-010）。閉じた enum（CPP-002）。
enum class AutosaveFailure
{
    // 置き場（%LOCALAPPDATA% の下）が見つからない・作れない。
    location_unavailable,
    // 本文か一覧を書けなかった。
    unwritable,
    // 残骸の一覧か本文を読めなかった。
    unreadable,
};
} // namespace nenenib::application
