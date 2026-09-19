#pragma once

#include "KeyValueFailure.hpp"

#include <expected>
#include <map>
#include <string_view>

namespace nenenib::adapters::win32
{
// 値は呼出元のbytesを借りる。codecの検証中だけ使い、domainは所有値へ写す。
using KeyValueFields = std::map<std::string_view, std::string_view, std::less<>>;

[[nodiscard]] std::expected<KeyValueFields, KeyValueFailure>
key_value_fields(std::string_view bytes);
} // namespace nenenib::adapters::win32
