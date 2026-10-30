#pragma once

namespace nenenib::application
{
// Windows のエクスプローラーへの登録（ADR 0178 の決定 3）。exe のパスと鍵と値は controller が
// ShellRegistrationPort と core の計画から作るので中身は無い。
// 使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct RegisterShell
{
};
} // namespace nenenib::application
