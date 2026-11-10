#include "PreviewImages.hpp"

#include "FilePath.hpp"
#include "ImageGeometry.hpp"
#include "ImageLimits.hpp"
#include "ImageReference.hpp"
#include "ImageSize.hpp"
#include "PreviewImageWindow.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace nenenib::application
{
namespace
{
using Resolved = std::expected<core::FilePath, core::ImageReferenceRefusal>;
// PreviewImages::Reach と同じ形（entry ごとの「範囲の中か」と「錨の要素からの差」）。
using Reach = std::pair<std::vector<bool>, std::vector<std::size_t>>;

// 画素の決まり（ImagePixels の注記）に合うか。
[[nodiscard]] bool well_formed(const ImagePixels &pixels) noexcept
{
    return core::image_pixels_fit(pixels.original) &&
           pixels.size == core::image_decoded_size(pixels.original) &&
           pixels.bgra.size() ==
               std::size_t{pixels.size.width} * pixels.size.height * core::image_pixel_bytes;
}

// entries の位置を経路の綴りの順に並べた索引（同じ経路の entry は 1 つしかない）。
[[nodiscard]] std::vector<std::size_t> sorted_by_path(const std::vector<PreviewImageEntry> &entries)
{
    std::vector<std::size_t> order(entries.size());
    for (std::size_t index = 0; index < order.size(); ++index)
    {
        order.at(index) = index;
    }
    std::ranges::sort(order, [&entries](std::size_t left, std::size_t right)
                      { return entries.at(left).path.text() < entries.at(right).path.text(); });
    return order;
}

// 前の解析の同じ経路の entry を引き継いだ値（無ければ waiting の新しい entry）。状態は evicted も含
// めてそのまま（範囲の中の evicted を再解析で頼み直さない・追補 51）。
[[nodiscard]] PreviewImageEntry carried(const std::vector<PreviewImageEntry> &previous,
                                        const std::vector<std::size_t> &sorted,
                                        const core::FilePath &path)
{
    const auto same =
        std::ranges::lower_bound(sorted, path.text(), {}, [&previous](std::size_t index)
                                 { return previous.at(index).path.text(); });
    if (same == sorted.end() || !(previous.at(*same).path == path))
    {
        return PreviewImageEntry{path};
    }
    return previous.at(*same);
}

// 序数ごとに、同じ経路の最初の序数（解けない序数は自分）。経路の綴りと序数で並べて引く。
[[nodiscard]] std::vector<std::size_t> first_of_same_path(const std::vector<Resolved> &resolved)
{
    std::vector<std::size_t> owner(resolved.size());
    std::vector<std::size_t> order;
    for (std::size_t ordinal = 0; ordinal < resolved.size(); ++ordinal)
    {
        owner.at(ordinal) = ordinal;
        if (resolved.at(ordinal).has_value())
        {
            order.push_back(ordinal);
        }
    }
    const auto text = [&resolved](std::size_t ordinal)
    { return resolved.at(ordinal).value().text(); };
    std::ranges::sort(
        order, [&text](std::size_t left, std::size_t right)
        { return text(left) != text(right) ? text(left) < text(right) : left < right; });
    for (std::size_t index = 1; index < order.size(); ++index)
    {
        if (text(order.at(index)) == text(order.at(index - 1U)))
        {
            owner.at(order.at(index)) = owner.at(order.at(index - 1U));
        }
    }
    return owner;
}

// 錨の要素 first からの要素の数の差。
[[nodiscard]] std::size_t apart(std::size_t block, std::size_t first) noexcept
{
    return block >= first ? block - first : first - block;
}

// 捨てる順で left が right より先か（範囲の外が先・同じ側なら錨から遠いものが先・同じ差なら本文の後
// ろが先）。
[[nodiscard]] bool dropped_before(const Reach &reach, std::size_t left, std::size_t right)
{
    if (reach.first.at(left) != reach.first.at(right))
    {
        return !reach.first.at(left);
    }
    if (reach.second.at(left) != reach.second.at(right))
    {
        return reach.second.at(left) > reach.second.at(right);
    }
    return left > right;
}

// loaded の entry が持つ画素のバイト数の合計。
[[nodiscard]] std::size_t held_bytes_of(const std::vector<PreviewImageEntry> &entries)
{
    std::size_t bytes = 0;
    for (const PreviewImageEntry &entry : entries)
    {
        if (entry.status == PreviewImageStatus::loaded && entry.pixels != nullptr)
        {
            bytes += entry.pixels->bgra.size();
        }
    }
    return bytes;
}
} // namespace

PreviewImages PreviewImages::of(const core::MarkdownDocument &document,
                                const std::optional<core::FilePath> &base,
                                const PreviewImages *previous)
{
    PreviewImages images;
    const std::size_t count = std::min(document.images.size(), core::preview_image_limit);
    std::vector<Resolved> resolved;
    resolved.reserve(count);
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal)
    {
        resolved.push_back(core::resolve_image_reference(document.images.at(ordinal).source, base));
    }
    const std::vector<std::size_t> owner = first_of_same_path(resolved);
    const std::vector<PreviewImageEntry> none;
    const std::vector<PreviewImageEntry> &earlier = previous != nullptr ? previous->entries_ : none;
    const std::vector<std::size_t> sorted = sorted_by_path(earlier);
    std::vector<std::size_t> entry_at(count, 0U);
    images.slots_.reserve(count);
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal)
    {
        if (!resolved.at(ordinal).has_value())
        {
            images.slots_.emplace_back(std::unexpected(resolved.at(ordinal).error()));
            continue;
        }
        if (owner.at(ordinal) == ordinal)
        {
            entry_at.at(ordinal) = images.entries_.size();
            images.entries_.push_back(carried(earlier, sorted, resolved.at(ordinal).value()));
        }
        images.slots_.emplace_back(entry_at.at(owner.at(ordinal)));
    }
    images.held_bytes_ = held_bytes_of(images.entries_);
    return images;
}

std::size_t PreviewImages::count() const noexcept
{
    return slots_.size();
}

std::optional<std::size_t> PreviewImages::entry_of(std::size_t ordinal) const
{
    if (ordinal >= slots_.size() || !slots_.at(ordinal).has_value())
    {
        return std::nullopt;
    }
    return slots_.at(ordinal).value();
}

std::optional<PreviewImageStatus> PreviewImages::status_of(std::size_t ordinal) const
{
    const auto entry = entry_of(ordinal);
    if (!entry.has_value())
    {
        return std::nullopt;
    }
    return entries_.at(entry.value()).status;
}

std::optional<core::ImageReferenceRefusal> PreviewImages::refusal_of(std::size_t ordinal) const
{
    if (ordinal >= slots_.size() || slots_.at(ordinal).has_value())
    {
        return std::nullopt;
    }
    return slots_.at(ordinal).error();
}

const core::FilePath *PreviewImages::path_of(std::size_t ordinal) const
{
    const auto entry = entry_of(ordinal);
    return entry.has_value() ? &entries_.at(entry.value()).path : nullptr;
}

std::shared_ptr<const ImagePixels> PreviewImages::pixels_of(std::size_t ordinal) const
{
    const auto entry = entry_of(ordinal);
    if (!entry.has_value() || entries_.at(entry.value()).status != PreviewImageStatus::loaded)
    {
        return nullptr;
    }
    return entries_.at(entry.value()).pixels;
}

std::size_t PreviewImages::held_bytes() const noexcept
{
    return held_bytes_;
}

PreviewImages::Reach PreviewImages::reach(const core::MarkdownDocument &document,
                                          PreviewImageWindow window) const
{
    Reach result{
        std::vector<bool>(entries_.size(), false),
        std::vector<std::size_t>(entries_.size(), std::numeric_limits<std::size_t>::max())};
    const std::size_t count = std::min(slots_.size(), document.images.size());
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal)
    {
        const auto entry = entry_of(ordinal);
        if (!entry.has_value())
        {
            continue;
        }
        const std::size_t block = document.images.at(ordinal).block;
        if (block >= window.first_block && block < window.end_block)
        {
            result.first.at(entry.value()) = true;
        }
        result.second.at(entry.value()) =
            std::min(result.second.at(entry.value()), apart(block, window.first_block));
    }
    return result;
}

// 範囲の中の waiting のうち錨に最も近いもの（同じ差なら本文の順で先のもの）。
std::optional<std::size_t> PreviewImages::nearest_waiting(const Reach &reach) const
{
    std::optional<std::size_t> nearest;
    for (std::size_t index = 0; index < entries_.size(); ++index)
    {
        const bool nearer =
            !nearest.has_value() || reach.second.at(index) < reach.second.at(nearest.value());
        if (entries_.at(index).status == PreviewImageStatus::waiting && reach.first.at(index) &&
            nearer)
        {
            nearest = index;
        }
    }
    return nearest;
}

// nearest より錨から遠い（か範囲の外の）loaded を、捨てる順に並べたもの。
std::vector<std::size_t> PreviewImages::farther_loaded(const Reach &reach,
                                                       std::size_t nearest) const
{
    std::vector<std::size_t> farther;
    for (std::size_t index = 0; index < entries_.size(); ++index)
    {
        const bool beyond =
            !reach.first.at(index) || reach.second.at(index) > reach.second.at(nearest);
        if (entries_.at(index).status == PreviewImageStatus::loaded && beyond)
        {
            farther.push_back(index);
        }
    }
    std::ranges::sort(farther, [&reach](std::size_t left, std::size_t right)
                      { return dropped_before(reach, left, right); });
    return farther;
}

// 頼む 1 つと、復号の最大の 1 枚を収めるために捨てる loaded（範囲の外→錨から遠い順・頼むものより遠
// いものだけ）。頼めない（頼み中・waiting が無い・遠いものを全部捨てても収まらない）なら空。
PreviewImages::Plan PreviewImages::plan(const Reach &reach, bool idle) const
{
    const bool requested =
        std::ranges::any_of(entries_, [](const PreviewImageEntry &entry)
                            { return entry.status == PreviewImageStatus::requested; });
    const auto nearest = idle && !requested ? nearest_waiting(reach) : std::nullopt;
    if (!nearest.has_value())
    {
        return Plan{};
    }
    const std::vector<std::size_t> farther = farther_loaded(reach, nearest.value());
    std::size_t held = held_bytes_;
    std::vector<std::size_t> dropped;
    for (const std::size_t index : farther)
    {
        if (held + core::image_decoded_byte_limit <= core::preview_image_held_byte_limit)
        {
            break;
        }
        held -= entries_.at(index).pixels->bgra.size();
        dropped.push_back(index);
    }
    if (held + core::image_decoded_byte_limit > core::preview_image_held_byte_limit)
    {
        return Plan{};
    }
    return Plan{nearest, std::move(dropped)};
}

bool PreviewImages::wants(const core::MarkdownDocument &document, PreviewImageWindow window,
                          bool idle) const
{
    if (entries_.empty())
    {
        return false;
    }
    const Reach where = reach(document, window);
    for (std::size_t index = 0; index < entries_.size(); ++index)
    {
        if (entries_.at(index).status == PreviewImageStatus::evicted && !where.first.at(index))
        {
            return true;
        }
    }
    return plan(where, idle).first.has_value();
}

std::optional<ImageRequest> PreviewImages::claim(const core::MarkdownDocument &document,
                                                 PreviewImageWindow window, bool idle,
                                                 std::uint64_t &tickets)
{
    const Reach where = reach(document, window);
    release_outside(where);
    const Plan chosen = plan(where, idle);
    if (!chosen.first.has_value())
    {
        return std::nullopt;
    }
    drop(where, chosen.second);
    PreviewImageEntry &entry = entries_.at(chosen.first.value());
    entry.status = PreviewImageStatus::requested;
    entry.ticket = ++tickets;
    return ImageRequest{entry.ticket, entry.path};
}

// 範囲の外の evicted を waiting に戻す（範囲を出たので、戻れば頼む）。
void PreviewImages::release_outside(const Reach &reach)
{
    for (std::size_t index = 0; index < entries_.size(); ++index)
    {
        PreviewImageEntry &entry = entries_.at(index);
        if (entry.status == PreviewImageStatus::evicted && !reach.first.at(index))
        {
            entry.status = PreviewImageStatus::waiting;
        }
    }
}

// indices の loaded の画素を捨てる。範囲の外で捨てたものは waiting（戻れば頼む）、範囲の中で捨てた
// ものは evicted（範囲を出るまで頼み直さない・追補 51）。
void PreviewImages::drop(const Reach &reach, const std::vector<std::size_t> &indices)
{
    for (const std::size_t index : indices)
    {
        PreviewImageEntry &dropped = entries_.at(index);
        held_bytes_ -= dropped.pixels->bgra.size();
        dropped.pixels.reset();
        dropped.status =
            reach.first.at(index) ? PreviewImageStatus::evicted : PreviewImageStatus::waiting;
    }
}

bool PreviewImages::received(ImageOutcome outcome)
{
    const auto waiting = std::ranges::find_if(
        entries_,
        [&outcome](const PreviewImageEntry &entry)
        {
            return entry.status == PreviewImageStatus::requested && entry.ticket == outcome.ticket;
        });
    if (waiting == entries_.end())
    {
        return false;
    }
    PreviewImageEntry &entry = *waiting;
    if (!outcome.result)
    {
        entry.status = PreviewImageStatus::failed;
        entry.failure = outcome.result.error();
        return true;
    }
    std::shared_ptr<const ImagePixels> pixels = std::move(outcome.result).value();
    if (pixels == nullptr || !well_formed(*pixels))
    {
        entry.status = PreviewImageStatus::failed;
        entry.failure = ImageFailure::corrupt;
        return true;
    }
    // 頼む前に復号の最大の 1 枚の分を空けてあるので、置いても上限を超えない（plan）。
    held_bytes_ += pixels->bgra.size();
    entry.status = PreviewImageStatus::loaded;
    entry.failure.reset();
    entry.pixels = std::move(pixels);
    return true;
}
} // namespace nenenib::application
