#include "DeleteDirection.hpp"
#include "EditMode.hpp"
#include "WindowChecks.hpp"
#include "WordDeletionKey.hpp"

#include <windows.h>

namespace nenenib::tests::ui
{
// Ctrl+Backspace / Ctrl+Delete の振り分け（ADR 0009 の追補 #1394）。通常モードだけが語の削除を
// 送る。Vim のモードと、Shift も押した Ctrl+Shift+Backspace / Ctrl+Shift+Delete（メモ帳の実測が
// 無い・#1410 のレビュー L4）は何も送らない。ほかの鍵も送らない。
void verify_word_deletion_keys()
{
    using nenenib::core::DeleteDirection;
    using nenenib::core::EditMode;
    using nenenib::ui::win32::word_deletion_for;
    expect(word_deletion_for(VK_BACK, false, EditMode::ordinary) == DeleteDirection::backward,
           "Ctrl+Backspace deletes the previous word in the ordinary mode");
    expect(word_deletion_for(VK_DELETE, false, EditMode::ordinary) == DeleteDirection::forward,
           "Ctrl+Delete deletes the next word in the ordinary mode");
    expect(!word_deletion_for(VK_BACK, true, EditMode::ordinary).has_value(),
           "Ctrl+Shift+Backspace sends nothing");
    expect(!word_deletion_for(VK_DELETE, true, EditMode::ordinary).has_value(),
           "Ctrl+Shift+Delete sends nothing");
    expect(!word_deletion_for(VK_BACK, false, EditMode::vim).has_value(),
           "Ctrl+Backspace sends nothing in the Vim mode");
    expect(!word_deletion_for(VK_DELETE, false, EditMode::vim).has_value(),
           "Ctrl+Delete sends nothing in the Vim mode");
    expect(!word_deletion_for('X', false, EditMode::ordinary).has_value(),
           "a character key is not a word deletion key");
}
} // namespace nenenib::tests::ui
