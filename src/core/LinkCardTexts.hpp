#pragma once

#include <string>

namespace nenenib::core
{
// リンクの札の文（施主決定 D106・#1869 の段 L2）。題・開き方の 1 行・鍵の案内・2 つのボタン
// （secondary は左の やめる / 閉じる、primary は右の 開く / 書く / 写す）。作るのは link_card_texts
// の 1 本で、文は ui_text の表から引く（CNF-016）。公開 aggregate（CPP-003）。
struct LinkCardTexts
{
    std::string title;
    std::string way;
    std::string hint;
    std::string secondary;
    std::string primary;
};
} // namespace nenenib::core
