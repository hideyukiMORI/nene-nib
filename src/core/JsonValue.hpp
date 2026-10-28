#pragma once

#include "JsonNull.hpp"
#include "JsonNumber.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::core
{
struct JsonValue;

// 配列と object（ADR 0160 の決定 4）。object は並びを保ち、鍵の重複もそのまま持つ（読みは
// json_member が最後を返す＝後勝ち）。std::map にしないのは write_json の決定性のため。
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::vector<std::pair<std::string, JsonValue>>;

// JSON の値の閉じた和型（RFC 8259・ADR 0160 の決定 4）。文字列は正しい UTF-8 のバイト列。
// 読む所は std::visit か std::get_if で選択肢を分ける。
struct JsonValue
{
    std::variant<JsonNull, bool, JsonNumber, std::string, JsonArray, JsonObject> value;
};

[[nodiscard]] bool operator==(const JsonValue &left, const JsonValue &right);

// object の鍵 key の値。重複する鍵は最後のもの（後勝ち）。無ければ値なし。
[[nodiscard]] std::optional<std::reference_wrapper<const JsonValue>>
json_member(const JsonObject &object, std::string_view key);
} // namespace nenenib::core
