#include "PreviewImages.hpp"

#include "FilePath.hpp"
#include "ImageGeometry.hpp"
#include "ImageLimits.hpp"
#include "ImageReference.hpp"
#include "ImageSize.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace nenenib::application
{
namespace
{
// 画素の決まり（ImagePixels の注記）に合うか。
[[nodiscard]] bool well_formed(const ImagePixels &pixels) noexcept
{
    return core::image_pixels_fit(pixels.original) &&
           pixels.size == core::image_decoded_size(pixels.original) &&
           pixels.bgra.size() ==
               std::size_t{pixels.size.width} * pixels.size.height * core::image_pixel_bytes;
}

// 前の解析の同じ経路の entry を引き継いだ値（無ければ waiting の新しい entry）。evicted は頼み直せ
// るように waiting に戻す。
[[nodiscard]] PreviewImageEntry carried(const std::vector<PreviewImageEntry> *previous,
                                        const core::FilePath &path)
{
    if (previous == nullptr)
    {
        return PreviewImageEntry{path};
    }
    const auto same = std::ranges::find_if(*previous, [&path](const PreviewImageEntry &entry)
                                           { return entry.path == path; });
    if (same == previous->end())
    {
        return PreviewImageEntry{path};
    }
    PreviewImageEntry kept = *same;
    if (kept.status == PreviewImageStatus::evicted)
    {
        kept.status = PreviewImageStatus::waiting;
    }
    return kept;
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
    images.slots_.reserve(count);
    const auto *earlier = previous != nullptr ? &previous->entries_ : nullptr;
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal)
    {
        images.add_slot(core::resolve_image_reference(document.images.at(ordinal).source, base),
                        earlier);
    }
    images.held_bytes_ = held_bytes_of(images.entries_);
    images.loads_ = previous != nullptr ? previous->loads_ : 0U;
    return images;
}

// 序数を 1 つ足す。同じ経路の entry が既にあれば共有し、無ければ前の状態から引き継いで足す。
void PreviewImages::add_slot(std::expected<core::FilePath, core::ImageReferenceRefusal> path,
                             const std::vector<PreviewImageEntry> *earlier)
{
    if (!path)
    {
        slots_.emplace_back(std::unexpected(path.error()));
        return;
    }
    const auto same = std::ranges::find_if(entries_, [&path](const PreviewImageEntry &entry)
                                           { return entry.path == path.value(); });
    const auto index = static_cast<std::size_t>(same - entries_.begin());
    if (same == entries_.end())
    {
        entries_.push_back(carried(earlier, path.value()));
    }
    slots_.emplace_back(index);
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

// entry ごとに、[first_block, end_block) の要素に節があるか。
std::vector<bool> PreviewImages::windowed(const core::MarkdownDocument &document,
                                          std::size_t first_block, std::size_t end_block) const
{
    std::vector<bool> inside(entries_.size(), false);
    const std::size_t count = std::min(slots_.size(), document.images.size());
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal)
    {
        const std::size_t block = document.images.at(ordinal).block;
        const auto entry = entry_of(ordinal);
        if (entry.has_value() && block >= first_block && block < end_block)
        {
            inside.at(entry.value()) = true;
        }
    }
    return inside;
}

bool PreviewImages::wants(const core::MarkdownDocument &document, std::size_t first_block,
                          std::size_t end_block) const
{
    if (entries_.empty())
    {
        return false;
    }
    const auto inside = windowed(document, first_block, end_block);
    for (std::size_t index = 0; index < entries_.size(); ++index)
    {
        const PreviewImageStatus status = entries_.at(index).status;
        if ((status == PreviewImageStatus::waiting && inside.at(index)) ||
            (status == PreviewImageStatus::evicted && !inside.at(index)))
        {
            return true;
        }
    }
    return false;
}

std::vector<ImageRequest> PreviewImages::claim(const core::MarkdownDocument &document,
                                               std::size_t first_block, std::size_t end_block,
                                               std::uint64_t &tickets)
{
    std::vector<ImageRequest> requests;
    const auto inside = windowed(document, first_block, end_block);
    for (std::size_t index = 0; index < entries_.size(); ++index)
    {
        PreviewImageEntry &entry = entries_.at(index);
        if (entry.status == PreviewImageStatus::evicted && !inside.at(index))
        {
            entry.status = PreviewImageStatus::waiting;
            continue;
        }
        if (entry.status == PreviewImageStatus::waiting && inside.at(index))
        {
            entry.status = PreviewImageStatus::requested;
            entry.ticket = ++tickets;
            requests.push_back(ImageRequest{entry.ticket, entry.path});
        }
    }
    return requests;
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
    loaded(entry, std::move(pixels));
    evict_beyond_limit(entry);
    return true;
}

void PreviewImages::loaded(PreviewImageEntry &entry, std::shared_ptr<const ImagePixels> pixels)
{
    held_bytes_ += pixels->bgra.size();
    entry.status = PreviewImageStatus::loaded;
    entry.failure.reset();
    entry.pixels = std::move(pixels);
    entry.loaded_order = ++loads_;
}

// kept のほかで読めた順のいちばん古い loaded（無ければ nullptr）。
PreviewImageEntry *PreviewImages::oldest_loaded(const PreviewImageEntry &kept)
{
    PreviewImageEntry *oldest = nullptr;
    for (PreviewImageEntry &entry : entries_)
    {
        const bool older = oldest == nullptr || entry.loaded_order < oldest->loaded_order;
        if (&entry != &kept && entry.status == PreviewImageStatus::loaded && older)
        {
            oldest = &entry;
        }
    }
    return oldest;
}

// 持つ量が上限を超えている間、kept のほかで読めた順のいちばん古い loaded を evicted にする。
void PreviewImages::evict_beyond_limit(const PreviewImageEntry &kept)
{
    while (held_bytes_ > core::preview_image_held_byte_limit)
    {
        PreviewImageEntry *oldest = oldest_loaded(kept);
        if (oldest == nullptr)
        {
            return;
        }
        held_bytes_ -= oldest->pixels->bgra.size();
        oldest->pixels.reset();
        oldest->status = PreviewImageStatus::evicted;
    }
}
} // namespace nenenib::application
