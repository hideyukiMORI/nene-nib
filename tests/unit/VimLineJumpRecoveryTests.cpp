// scope `--vim-line-jump-recovery` の単体テスト（ADR 0042 決定 2）。
#include "Scopes.hpp"

namespace nenenib::tests
{
void verify_vim_line_jump_recovery()
{
    verify_vim_line_jump_continuations();
    verify_vim_line_jump_operator_undo();
}
} // namespace nenenib::tests
