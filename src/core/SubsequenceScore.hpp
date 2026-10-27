#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace nenenib::core
{
// Ctrl+P の面・補完・操作の一覧が共用する照合の核（ADR 0060 / 0061 の決定 6 / 0091 の決定 2）。
// query のコードポイントを順に候補（完結した UTF-8 の区間の並び）の中から探す部分列の照合で、
// 空白は飛ばし、ASCII の A〜Z は大文字と小文字を区別しない。点は候補の長さと飛ばした量の和で、
// 最初の文字までは 4 倍（単位はバイト）。外れは値なし。
//
// query の全バイトが 0x01〜0x7F のときは候補のバイト列をそのまま歩く速道を選ぶ（#912・ADR 0091
// の追補）。答え（当たり・点）はコードポイントの道と同じで、性質の試験が両方の道を突き合わせる。
[[nodiscard]] std::optional<std::size_t>
subsequence_score(std::string_view query, std::span<const std::string_view> command);

// 速道を選べる query か（全バイトが 0x01〜0x7F）。NUL は無効な先頭バイトの復号値 0 と一致するので
// コードポイントの道に残す。
[[nodiscard]] bool byte_walkable_query(std::string_view query) noexcept;

// 試験だけが使う 2 つの道。by_bytes の前提は byte_walkable_query(query)。
[[nodiscard]] std::optional<std::size_t>
subsequence_score_by_code_points(std::string_view query, std::span<const std::string_view> command);
[[nodiscard]] std::optional<std::size_t>
subsequence_score_by_bytes(std::string_view query, std::span<const std::string_view> command);
} // namespace nenenib::core
