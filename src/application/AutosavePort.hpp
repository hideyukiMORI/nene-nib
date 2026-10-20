#pragma once

#include "AutosaveFailure.hpp"
#include "AutosaveId.hpp"
#include "AutosaveOutcome.hpp"
#include "AutosaveRemnant.hpp"
#include "AutosaveSnapshot.hpp"

#include <cstddef>
#include <expected>
#include <vector>

namespace nenenib::application
{
// 未保存の編集を裏で退避する口（ADR 0137 の決定 4〜6・ADR 0004 の仕事の種類ごとの port）。
// スレッドは adapters に閉じる。store と discard は頼んですぐ戻り、worker が書く・消す
// （FolderPort の list と同じ形・ADR 0062 の決定 5）。collect は worker が終えた仕事の結果を
// 終えた順に全部受け取る（成功も失敗も 1 件ずつ・無ければ空）。clear
// は窓が閉じていくときに進行中の仕事を待って自分の置き場を消す（同期）。flush は確かめずに
// 未保存が残る終わり方で、頼んだ仕事（順番待ちと進行中）が終わるまで上限つきで待つ（同期・置き場は
// 消さない・ADR 0137 の決定 9）。remnants / read / forget は起動で前回の残骸を扱う（同期）。
// どれも UI スレッドだけが呼ぶ。
class AutosavePort
{
  public:
    AutosavePort() = default;
    virtual ~AutosavePort() = default;
    AutosavePort(const AutosavePort &) = delete;
    AutosavePort(AutosavePort &&) = delete;
    AutosavePort &operator=(const AutosavePort &) = delete;
    AutosavePort &operator=(AutosavePort &&) = delete;

    virtual void store(AutosaveSnapshot snapshot) = 0;
    virtual void discard(AutosaveId id) = 0;
    [[nodiscard]] virtual std::vector<AutosaveOutcome> collect() = 0;
    virtual void clear() = 0;
    virtual void flush() = 0;
    [[nodiscard]] virtual std::expected<std::vector<AutosaveRemnant>, AutosaveFailure>
    remnants() = 0;
    [[nodiscard]] virtual std::expected<std::vector<std::byte>, AutosaveFailure>
    read(const AutosaveRemnant &remnant) = 0;
    virtual void forget(const AutosaveRemnant &remnant) = 0;
};
} // namespace nenenib::application
