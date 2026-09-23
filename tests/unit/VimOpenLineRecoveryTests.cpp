// scope `--vim-open-line-recovery` の単体テスト（ADR 0042 決定 2）。
#include "Scopes.hpp"

namespace nenenib::tests
{
void verify_vim_open_line_recovery()
{
    verify_open_line_round_trip("", "3O<Esc>", "\r\n\r\n\r\n");
    verify_vim_open_line_capacity();
    verify_vim_open_line_fixtures();
}
} // namespace nenenib::tests
