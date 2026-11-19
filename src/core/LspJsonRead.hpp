#pragma once

#include "JsonValue.hpp"
#include "LspPosition.hpp"
#include "LspRange.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
// サーバーの JSON の欄を読む唯一の口（ADR 0190 の決定 4・ARC-012）。診断（LspDiagnostics）と答え
// （LspAnswers）が同じ読みを使う。どれも欄が無い・形が違えば nullptr か値なしで、例外を投げない。

// 値が object ならその object。
[[nodiscard]] const JsonObject *json_object_of(const JsonValue &value) noexcept;

// object の鍵 key の object。
[[nodiscard]] const JsonObject *json_object_member(const JsonObject &object, std::string_view key);

// object の鍵 key の文字列。
[[nodiscard]] const std::string *json_text_member(const JsonObject &object, std::string_view key);

// 0 以上の整数（Position の line / character・severity・version・kind）。
[[nodiscard]] std::optional<std::uint64_t> json_count_of(const JsonValue &value) noexcept;

[[nodiscard]] std::optional<std::uint64_t> json_count_member(const JsonObject &object,
                                                             std::string_view key);

// object の鍵 key の LSP の Position（line と character の両方が 0 以上の整数のときだけ）。
[[nodiscard]] std::optional<LspPosition> lsp_position_member(const JsonObject &object,
                                                             std::string_view key);

// object の鍵 key の LSP の Range（start と end の両方が読めるときだけ）。
[[nodiscard]] std::optional<LspRange> lsp_range_member(const JsonObject &object,
                                                       std::string_view key);
} // namespace nenenib::core
