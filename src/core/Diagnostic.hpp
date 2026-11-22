#pragma once

#include "DiagnosticSeverity.hpp"
#include "OffsetRange.hpp"

#include <string>

namespace nenenib::core
{
// 文書の値としての診断 1 件（ADR 0161 の決定 2）。range は本文のバイト位置の半開区間、message は
// サーバーの文の全部（改行を含む・lsp_diagnostic_message_limit バイトまで・#1939）、source と
// code はサーバーが添えた出どころと番号（無ければ空・番号が整数なら 10 進の文字列）。1 行の知らせと
// 一覧の行は message の最初の行だけを見せ、全部は札（diagnostic_tip_text）が見せる。どの欄も
// 互いに独立して妥当なので公開 aggregate。比較は非メンバー（CPP-003）。
struct Diagnostic
{
    OffsetRange range;
    DiagnosticSeverity severity;
    std::string message;
    std::string source{};
    std::string code{};
};

[[nodiscard]] inline bool operator==(const Diagnostic &left, const Diagnostic &right)
{
    return left.range == right.range && left.severity == right.severity &&
           left.message == right.message && left.source == right.source && left.code == right.code;
}
} // namespace nenenib::core
