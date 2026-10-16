#pragma once

#include <cstdint>

namespace nenenib::core
{
// 文書ごとの表示形式（ADR 0140 の決定 3）。text は今日どおりの文字組み、grid は等幅の升目。
// 閉じた選択肢で、設定（EditorSettings）には入れない。文書ごとの値は application が持つ。
enum class DisplayForm : std::uint8_t
{
    text,
    grid
};
} // namespace nenenib::core
