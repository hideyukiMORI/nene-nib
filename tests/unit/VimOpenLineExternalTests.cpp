// scope `--vim-open-line-external` の単体テスト（ADR 0042 決定 2）。
#include "Scopes.hpp"

namespace nenenib::tests
{
void verify_vim_open_line_external_scope()
{
    verify_vim_interrupt_discards();
    verify_vim_open_line_external_input();
    verify_vim_open_line_external_edit();
    verify_vim_open_line_movement();
    verify_vim_insert_motion_breaks_the_unit();
    verify_vim_put_line_endings();
}
} // namespace nenenib::tests
