#pragma once

#include <cstdint>

namespace nenenib::core
{
// `:set root=.`（ADR 0162 の決定 7）。アクティブな文書のフォルダをルートにする
// 要求で、経路を解くのは文書を持つ controller（無題なら失敗）。解いた値を設定に
// 載せて保存するのも controller なので、Ex の答えの settings は空のまま。値が
// 増えたら写し先の switch がコンパイルで落ちる（CPP-002）。
enum class ExRootRequest : std::uint8_t
{
    active_document_folder
};
} // namespace nenenib::core
