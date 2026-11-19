#pragma once

#include "FilePath.hpp"
#include "RevealFailure.hpp"

#include <expected>
#include <vector>

namespace nenenib::application
{
// エクスプローラーでファイルを選んだ状態でフォルダを開かせる口（ADR 0078 の追補 #1790・ADR 0004 の
// 仕事の種類ごとの port・ARC-007: シェルに触れるのは src/adapters/win32 だけ）。reveal は頼んで
// すぐ戻る（シェルの呼び出しは UI スレッドを止めないよう adapters が裏で行う）。終わった頼みの
// 結果（成功か失敗の理由）は WorkCompleted の合図の後に collect
// が頼んだ順で全部返す（無ければ空）。 UI スレッドだけが呼ぶ。 controller
// が呼ぶのは「エクスプローラーで場所を開く」のときと、頼んだ後の合図のときだけ。
class RevealPort
{
  public:
    RevealPort() = default;
    virtual ~RevealPort() = default;
    RevealPort(const RevealPort &) = delete;
    RevealPort(RevealPort &&) = delete;
    RevealPort &operator=(const RevealPort &) = delete;
    RevealPort &operator=(RevealPort &&) = delete;

    virtual void reveal(core::FilePath path) = 0;
    [[nodiscard]] virtual std::vector<std::expected<void, RevealFailure>> collect() = 0;
};
} // namespace nenenib::application
