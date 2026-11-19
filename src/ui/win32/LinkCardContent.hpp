#pragma once

#include "LinkCardInks.hpp"
#include "LinkCardTexts.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace nenenib::ui::win32
{
// 札に描く中身（施主決定 D106・#1869 の段 L2）。texts は core::link_card_texts、target は宛先の綴り
// （core::LinkDestination の text）、emphasis はその中の太字の範囲（core::link_card_emphasis・
// バイト位置）、inks は色。renderer が frame から作り、試験は直接作る。公開 aggregate（CPP-003）。
struct LinkCardContent
{
    core::LinkCardTexts texts;
    std::string target;
    std::pair<std::size_t, std::size_t> emphasis;
    LinkCardInks inks;
};
} // namespace nenenib::ui::win32
