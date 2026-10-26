#pragma once
#include <cstddef>
#include <string_view>
#include <utility>
#include <windows.h>

namespace nenenib::ui::win32
{
// unit の位置の code point と、その UTF-16 の単位数。対になっていないサロゲートはそのまま 1 単位。
[[nodiscard]] std::pair<char32_t, UINT32> wide_code_point_at(std::wstring_view wide,
                                                             std::size_t unit);
// unit の位置で前の文字が続く（サロゲートの対の後半か、幅 0 の code point・ADR 0053）。
[[nodiscard]] bool wide_continues_at(std::wstring_view wide, std::size_t unit);
// 区切りの start から字体選択へ 1 回に渡す長さ（ADR 0142 の決定 3 の追補・#875）。切り目は内容
// だけで決まる: 歩いた長さが fallback_piece_minimum 以上で、code point の hash の下位 6 ビットが 0
// で、次の単位が前の文字に続かない所。fallback_key_text_limit まで当たりが無ければ、最小以上・
// 上限未満の切ってよい所のうち hash が最小の所（同点は後ろ）で切る。remaining が最小以下か
// 上限以内で当たりが無ければ remaining の全部。
[[nodiscard]] UINT32 fallback_piece_length(std::wstring_view wide, UINT32 start, UINT32 remaining);
} // namespace nenenib::ui::win32
