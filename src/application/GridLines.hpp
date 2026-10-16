#pragma once

#include "GridLine.hpp"
#include "LineNumber.hpp"
#include "TextBuffer.hpp"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace nenenib::application
{
// 格子の行の保持（ADR 0140 の決定 4・試作）。状態が本文を入れ替えるたびに新しい入れ物を作るので、
// 中身はいつも同じ本文の行を指す。上限を超えたら古いものから捨てる。
class GridLines final
{
  public:
    [[nodiscard]] std::shared_ptr<const core::GridLine>
    line(const core::TextBuffer &text, core::LineNumber number, std::size_t columns);

  private:
    std::vector<std::pair<std::size_t, std::shared_ptr<const core::GridLine>>> lines_;
};
} // namespace nenenib::application
