#pragma once

#include <string_view>
#include <vector>

namespace nenenib::adapters::win32
{
// 保存の形の行に割る（session.v1 と history.v1 が共用・ADR 0059 / ADR 0060 の決定 8）。行末の CR は
// 落とし、最後の LF の後ろの空は行に数えない。行は bytes を借りる。
[[nodiscard]] std::vector<std::string_view> text_lines(std::string_view bytes);
} // namespace nenenib::adapters::win32
