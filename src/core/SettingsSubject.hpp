#pragma once

#include "EditorFeature.hpp"
#include "SettingsValue.hpp"

#include <variant>

namespace nenenib::core
{
// 設定の面の 1 行が指すもの（機能のスイッチか、値の行か）。閉じた和型で、読む所は std::visit。
using SettingsSubject = std::variant<EditorFeature, SettingsValue>;
} // namespace nenenib::core
