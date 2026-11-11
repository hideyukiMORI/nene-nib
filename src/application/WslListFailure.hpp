#pragma once

namespace nenenib::application
{
// WSL のディストリの一覧が読めなかった（ADR 0162 の追補 #1450）。WSL が入っていないのは失敗では
// なく空の一覧。読めないときもフォルダ選びの箱は場所を足さずに出す（知らせない）。
enum class WslListFailure
{
    unreadable,
};
} // namespace nenenib::application
