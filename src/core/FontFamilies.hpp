#pragma once

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace nenenib::core
{
// `:set guifont=` の補完に出す OS の字体の family 名の列（施主決定
// D58・#597）。名前順で重複が無く、 補完の候補（`set
// guifont=<名前>`）として表示できる名前だけを持つ。写しは参照の共有だけ。
class FontFamilies final
{
  public:
    // まだ読んでいない・読めなかった列（候補は打った値だけ）。
    [[nodiscard]] static FontFamilies none();
    // 表示できない名前と空の名前を外し、名前順に並べて重複を除く。
    [[nodiscard]] static FontFamilies from(std::vector<std::string> names);
    [[nodiscard]] std::span<const std::string> names() const & noexcept;
    std::span<const std::string> names() const && = delete;

  private:
    explicit FontFamilies(std::shared_ptr<const std::vector<std::string>> names);
    std::shared_ptr<const std::vector<std::string>> names_;
};
} // namespace nenenib::core
