#pragma once

#include "AutosaveFailure.hpp"
#include "AutosaveId.hpp"
#include "AutosaveRemnant.hpp"
#include "AutosaveSnapshot.hpp"

#include <cstddef>
#include <expected>
#include <vector>

namespace nenenib::application
{
// 未保存の編集を裏で退避する口（ADR 0137 の決定 4〜6・ADR 0004 の仕事の種類ごとの port）。
// スレッドは adapters に閉じる。store と discard は頼んですぐ戻り、worker が書く・消す
// （FolderPort の list と同じ形・ADR 0062 の決定 5）。collect は worker の失敗を全部受け取る
// （無ければ空）。clear は窓が閉じていくときに進行中の仕事を待って自分の置き場を消す（同期）。
// remnants / read / forget は起動で前回の残骸を扱う（同期）。どれも UI スレッドだけが呼ぶ。
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
    [[nodiscard]] virtual std::vector<AutosaveFailure> collect() = 0;
    virtual void clear() = 0;
    [[nodiscard]] virtual std::expected<std::vector<AutosaveRemnant>, AutosaveFailure>
    remnants() = 0;
    [[nodiscard]] virtual std::expected<std::vector<std::byte>, AutosaveFailure>
    read(const AutosaveRemnant &remnant) = 0;
    virtual void forget(const AutosaveRemnant &remnant) = 0;
};
} // namespace nenenib::application
