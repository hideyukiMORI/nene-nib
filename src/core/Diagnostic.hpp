#pragma once

#include "DiagnosticCodeForm.hpp"
#include "DiagnosticSeverity.hpp"
#include "OffsetRange.hpp"

#include <string>

namespace nenenib::core
{
// 文書の値としての診断 1 件（ADR 0161 の決定 2）。range は本文のバイト位置の半開区間、message は
// サーバーの文の全部（改行を含む・lsp_diagnostic_message_limit バイトまで・#1939）、source と
// code はサーバーが添えた出どころと番号（無ければ空・番号が整数なら 10 進の文字列）。1 行の知らせと
// 一覧の行は message の最初の行だけを見せ、全部は札（diagnostic_tip_text）が見せる。code_form は
// code の JSON の型、data はサーバーが添えた `data` を write_json で書いた本文（無ければ空・
// lsp_diagnostic_data_limit を超えたら持たない・打鍵ごとの診断の列の写しで JSON の木を写さない・
// #2022 のレビュー R2）で、どちらもクイックフィックスの `context.diagnostics` へ戻すため（#2005・
// ADR 0194 の決定 8）。どの欄も互いに独立して妥当なので公開
// aggregate。比較は非メンバー（CPP-003）。
struct Diagnostic
{
    OffsetRange range;
    DiagnosticSeverity severity;
    std::string message;
    std::string source{};
    std::string code{};
    DiagnosticCodeForm code_form = DiagnosticCodeForm::none;
    std::string data{};
};

[[nodiscard]] inline bool operator==(const Diagnostic &left, const Diagnostic &right)
{
    return left.range == right.range && left.severity == right.severity &&
           left.message == right.message && left.source == right.source &&
           left.code == right.code && left.code_form == right.code_form && left.data == right.data;
}
} // namespace nenenib::core
