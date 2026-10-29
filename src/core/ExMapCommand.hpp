#pragma once

#include "DisplayText.hpp"
#include "EditorSettings.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExMapFailure.hpp"
#include "ExMapRequest.hpp"
#include "ExResult.hpp"

#include <expected>
#include <optional>
#include <span>
#include <string_view>

namespace nenenib::core
{
// `:nmap` `:imap` `:vmap` `:xmap` `:omap` `:map` と noremap 系・`:unmap` 系・`:let mapleader=` を
// 読む（ADR 0176 の決定 6）。名前が表に当たらなければ値なし（ほかの命令として扱う）。範囲の形と
// `!`（`:map!`）は受けない。答えは ExResult.map の要求で、設定を書き換えるのは controller
// （map_edited → persist_settings）。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
map_command(std::string_view text);

// Ctrl+P の `:` の候補に載せる命令の完全な形（決定 6）。
[[nodiscard]] std::span<const std::string_view> map_command_candidates() noexcept;

// 要求を設定へ写す 1 本（決定 6・追補 14・15）。add は同じ左辺の行から要求のモードを外してから
// 末尾に足し、remove はモードを外して空の行を消す（`v` と `x` は同じ VISUAL なのでどちらも外す）。
// 行は map_row_of を通し、保存で同じ鍵になる行は後の行だけを残す。leader は parse_map_leader を
// 通す。list は設定を変えない。
[[nodiscard]] std::expected<EditorSettings, ExMapFailure> map_edited(const EditorSettings &settings,
                                                                     const ExMapRequest &request);

[[nodiscard]] DisplayText ex_map_failure_message(ExMapFailure failure);
} // namespace nenenib::core
