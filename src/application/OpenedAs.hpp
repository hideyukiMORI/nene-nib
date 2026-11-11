#pragma once

#include <cstdint>

namespace nenenib::application
{
// 開き方（ADR 0179 の決定 3）。エクスプローラーの面のクリック 1 回でファイルを開く道だけが
// transient（プレビューのタブ）で、ほかの開く道（面のダブルクリックと Enter・Ctrl+P の面・
// Ctrl+O・起動引数・ドロップ・`:e`）は pinned。読みが裏で終わっても同じ開き方で開くので、開く
// 要求に添えて覚える。閉じた集合（CPP-002）。
enum class OpenedAs : std::uint8_t
{
    pinned,
    transient
};
} // namespace nenenib::application
