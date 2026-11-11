// Markdown プレビューの画像の段 1（施主決定 D79・ADR 0155 の追補 47〜53・#1381）。core の画像の節・
// 参照の解決・形式の判定・大きさの純関数と、application の PreviewImages と ImagePort の道（見える
// 範囲だけを頼む・券・引き継ぎ・上限・静かな道）。
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "ImageFailure.hpp"
#include "ImageFormat.hpp"
#include "ImageGeometry.hpp"
#include "ImageLimits.hpp"
#include "ImageOutcome.hpp"
#include "ImagePixels.hpp"
#include "ImageReference.hpp"
#include "ImageReferenceRefusal.hpp"
#include "ImageRequest.hpp"
#include "ImageSize.hpp"
#include "InsertText.hpp"
#include "MarkdownDocument.hpp"
#include "MarkdownImage.hpp"
#include "MarkdownOutcome.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "PreviewFrame.hpp"
#include "PreviewImageStatus.hpp"
#include "PreviewImageWindow.hpp"
#include "PreviewImages.hpp"
#include "PreviewStyle.hpp"
#include "PreviewTick.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedImages.hpp"
#include "ScriptedMarkdown.hpp"
#include "ScrollPane.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "TogglePreview.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using core::ImageFormat;
using core::ImageReferenceRefusal;
using core::ImageSize;
using Status = app::PreviewImageStatus;

constexpr std::string_view readme = "C:\\docs\\readme.md";

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

[[nodiscard]] core::MarkdownDocument parsed_document(std::string_view text)
{
    return core::parse_markdown(text, core::MarkdownStop::never()).value();
}

// ---------------------------------------------------------------- core: 画像の節（追補 47）

void verify_image_node()
{
    const auto document = parsed_document("![alt](img/a.png \"Title\")");
    expect(document.images.size() == 1U && document.blocks.size() == 1U,
           "one image paragraph gives one block and one image node");
    const core::MarkdownImage &image = document.images.front();
    const auto &block = document.blocks.front();
    expect(image.block == 0U && image.source == "img/a.png" && image.title == "Title" &&
               image.alt == "alt",
           "the image node keeps the destination, the title and the alt");
    expect(block.text == "[画像: alt]" && image.begin == core::Offset{0} &&
               image.end == core::Offset{block.text.size()},
           "the image node covers the whole [画像: alt] text of the block");
    expect(block.spans.size() == 1U && block.spans.front().style == core::PreviewStyle::image &&
               block.spans.front().begin == image.begin && block.spans.front().end == image.end,
           "the image node has the same range as the image span");
}

void verify_image_attributes()
{
    const auto entities = parsed_document("![a &amp; b](x&amp;y.png 'T &quot;q&quot;')");
    expect(entities.images.size() == 1U && entities.images.front().source == "x&y.png" &&
               entities.images.front().title == "T \"q\"" && entities.images.front().alt == "a & b",
           "entities in the destination, the title and the alt are resolved");
    const auto escaped = parsed_document("![a](b\\_c.png)");
    expect(escaped.images.size() == 1U && escaped.images.front().source == "b_c.png",
           "a backslash escape in the destination is resolved like CommonMark");
    const auto untitled = parsed_document("![](plain.png)");
    expect(untitled.images.size() == 1U && untitled.images.front().title.empty() &&
               untitled.images.front().alt.empty() && untitled.blocks.front().text == "[画像: ]",
           "an image without title and alt keeps both empty");
    const auto reference = parsed_document("![r][ref]\n\n[ref]: r%20x.png \"t\"\n");
    expect(reference.images.size() == 1U && reference.images.front().source == "r%20x.png" &&
               reference.images.front().title == "t",
           "a reference style image takes the destination and the title of the definition");
    const auto pointed = parsed_document("![a](<my pic.png>)");
    expect(pointed.images.size() == 1U && pointed.images.front().source == "my pic.png",
           "a destination in angle brackets keeps its spaces");
}

void verify_image_order()
{
    const auto document =
        parsed_document("one ![x](1.png) and ![y](2.png)\n\n# head ![z](3.png)\n\nplain\n\n"
                        "[![badge](b.svg)](https://example.com)\n");
    expect(document.images.size() == 4U, "every image node is listed");
    expect(document.images.at(0).block == 0U && document.images.at(1).block == 0U &&
               document.images.at(2).block == 1U && document.images.at(3).block == 3U,
           "the image nodes keep the block of the element in the body order");
    const auto &first = document.blocks.at(0);
    expect(first.text.substr(document.images.at(1).begin.value,
                             document.images.at(1).end.value - document.images.at(1).begin.value) ==
               "[画像: y]",
           "the second image of a paragraph points at its own text");
    expect(document.images.at(3).source == "b.svg" && document.images.at(3).alt == "badge",
           "an image inside a link is an image node");
    const auto nested = parsed_document("![a ![b](in.png) c](out.png)");
    expect(nested.images.size() == 2U && nested.images.at(0).source == "out.png" &&
               nested.images.at(1).source == "in.png" &&
               nested.images.at(0).alt == "a [画像: b] c" && nested.images.at(1).alt == "b",
           "an image inside an image is listed after the outer one");
    expect(parsed_document("# no image\n\n[link](a.png)\n").images.empty(),
           "a document without images has no image node");
    expect(parsed_document("`![code](a.png)`\n").images.empty(),
           "an image inside a code span is not an image node");
}

// ---------------------------------------------------------------- core: 参照の解決（追補 48）

[[nodiscard]] std::string resolved(std::string_view reference,
                                   const std::optional<core::FilePath> &document)
{
    const auto path = core::resolve_image_reference(reference, document);
    return path ? std::string(path.value().text()) : std::string("<refused>");
}

[[nodiscard]] bool refused(std::string_view reference,
                           const std::optional<core::FilePath> &document,
                           ImageReferenceRefusal reason)
{
    const auto path = core::resolve_image_reference(reference, document);
    return !path && path.error() == reason;
}

void verify_relative_references()
{
    const std::optional<core::FilePath> base = path_of(readme);
    expect(resolved("a.png", base) == "C:\\docs\\a.png" &&
               resolved("./img/a.png", base) == "C:\\docs\\img\\a.png" &&
               resolved("img\\b.png", base) == "C:\\docs\\img\\b.png",
           "a relative path is resolved from the folder of the document");
    expect(resolved("../a.png", base) == "C:\\a.png" &&
               resolved("../../../a.png", base) == "C:\\a.png" &&
               resolved("img/../x/./a.png", base) == "C:\\docs\\x\\a.png",
           "dot segments are folded and never go above the root");
    expect(resolved("img%20x.png", base) == "C:\\docs\\img x.png" &&
               resolved("%E7%94%BB.png", base) == "C:\\docs\\画.png" &&
               resolved("100%.png", base) == "C:\\docs\\100%.png" &&
               resolved("a%2", base) == "C:\\docs\\a%2",
           "percent-encoding is decoded and an incomplete one stays as it is");
    expect(resolved("a.png?raw=1", base) == "C:\\docs\\a.png" &&
               resolved("a.png#top", base) == "C:\\docs\\a.png",
           "the query and the fragment are dropped");
    expect(resolved("/root/a.png", base) == "C:\\root\\a.png" &&
               resolved("\\root\\a.png", base) == "C:\\root\\a.png",
           "a path starting with a separator is resolved from the root of the drive");
    expect(refused("a.png", std::nullopt, ImageReferenceRefusal::no_base) &&
               refused("/a.png", std::nullopt, ImageReferenceRefusal::no_base),
           "an untitled document cannot resolve a relative path");
    expect(refused("a.png", path_of("\\\\server\\share\\doc.md"), ImageReferenceRefusal::network),
           "a relative path from a document on the network is refused");
    expect(refused("a.png", path_of("doc.md"), ImageReferenceRefusal::no_base),
           "a document place that is not an absolute drive path cannot be a base");
    expect(refused("%FF.png", base, ImageReferenceRefusal::malformed) &&
               refused("%00a.png", base, ImageReferenceRefusal::malformed),
           "a decoded path that is not UTF-8 or has a control character is malformed");
}

void verify_absolute_references()
{
    const std::optional<core::FilePath> base = path_of(readme);
    expect(resolved("C:\\pics\\a.png", base) == "C:\\pics\\a.png" &&
               resolved("C:/pics/./b/../a.png", base) == "C:\\pics\\a.png" &&
               resolved("D:/x%20y.png", base) == "D:\\x%20y.png" &&
               resolved("C:\\a#b.png", base) == "C:\\a#b.png",
           "an absolute drive path is taken as written with the separators folded");
    expect(resolved("C:\\pics\\a.png", std::nullopt) == "C:\\pics\\a.png",
           "an untitled document still shows an absolute path");
    expect(resolved("file:///C:/pics/a%20b.png", base) == "C:\\pics\\a b.png" &&
               resolved("FILE://localhost/C:/a.png", base) == "C:\\a.png" &&
               resolved("file:/C:/a.png", std::nullopt) == "C:\\a.png" &&
               resolved("file:///C:/a.png?x#y", base) == "C:\\a.png",
           "a file URL is decoded to a drive path");
    expect(refused("file://server/share/a.png", base, ImageReferenceRefusal::network) &&
               refused("file:///etc/a.png", base, ImageReferenceRefusal::malformed) &&
               refused("file:", base, ImageReferenceRefusal::malformed),
           "a file URL with a host or without a drive path is refused");
    expect(refused("\\\\server\\share\\a.png", base, ImageReferenceRefusal::network) &&
               refused("//host/a.png", base, ImageReferenceRefusal::network),
           "UNC paths and host relative references are network places");
    expect(refused("C:a.png", base, ImageReferenceRefusal::malformed),
           "a drive relative path is malformed");
}

// レビュー V-1385 の参照の表の釘（いまの実装はどれも安全側・退行を止める）。
void verify_reference_pins()
{
    const std::optional<core::FilePath> base = path_of(readme);
    expect(refused("\\\\?\\C:\\a.png", base, ImageReferenceRefusal::network) &&
               refused("\\\\.\\CON", base, ImageReferenceRefusal::network) &&
               refused("\\\\?\\UNC\\s\\x", base, ImageReferenceRefusal::network),
           "device paths are refused as network places");
    expect(refused("file:////server/share/a.png", base, ImageReferenceRefusal::malformed) &&
               refused("file:///%5C%5Cserver%5Cshare%5Ca.png", base,
                       ImageReferenceRefusal::malformed) &&
               refused("file://%5C%5Cserver/a.png", base, ImageReferenceRefusal::network),
           "a file URL never reaches the network through extra slashes or encoded backslashes");
    expect(
        resolved("%5C%5Cserver%5Cshare%5Ca.png", base) == "C:\\server\\share\\a.png" &&
            resolved("%2F%2Fhost%2Fa.png", base) == "C:\\host\\a.png",
        "an encoded UNC prefix is resolved from the drive root and never becomes a network path");
    expect(refused("file:///c|/a.png", base, ImageReferenceRefusal::malformed),
           "the old form file:///c|/ is refused");
    // 予約のデバイス名は経路としては通る（段 2 の adapter が開いた後に GetFileType が
    // FILE_TYPE_DISK でなければ断る・追補 54）。
    expect(resolved("CON", base) == "C:\\docs\\CON" && resolved("NUL", base) == "C:\\docs\\NUL" &&
               resolved("COM1", base) == "C:\\docs\\COM1" &&
               resolved("a/CON", base) == "C:\\docs\\a\\CON" &&
               resolved("file:///C:/CON", base) == "C:\\CON",
           "reserved device names pass as paths (stage 2 refuses what is not a disk file)");
    // 文書のフォルダの外もローカルのファイルなので指せる（D79 が断るのはネットだけ・追補 54）。
    expect(resolved("a%2F..%2F..%2F..%2Fwindows%2Fwin.ini", base) == "C:\\windows\\win.ini" &&
               resolved("/a.png", base) == "C:\\a.png",
           "a reference may leave the folder of the document (a local file)");
    expect(refused("a.png:stream", base, ImageReferenceRefusal::unknown_scheme) &&
               resolved("%zz.png", base) == "C:\\docs\\%zz.png" &&
               resolved("C%3A%5Cwindows%5Cwin.ini", base) == "C:\\docs\\C:\\windows\\win.ini",
           "a stream name is a scheme, a bad escape stays and an encoded drive is not a drive");
}

void verify_refused_schemes()
{
    const std::optional<core::FilePath> base = path_of(readme);
    expect(refused("http://example.com/a.png", base, ImageReferenceRefusal::remote) &&
               refused("HTTPS://example.com/a.png", base, ImageReferenceRefusal::remote),
           "images on the net are never fetched");
    expect(refused("data:image/png;base64,AAAA", base, ImageReferenceRefusal::data),
           "data URIs are not shown");
    expect(refused("ftp://example.com/a.png", base, ImageReferenceRefusal::unknown_scheme) &&
               refused("mailto:a@example.com", base, ImageReferenceRefusal::unknown_scheme) &&
               refused("x-y.z+w:a", base, ImageReferenceRefusal::unknown_scheme),
           "other schemes are refused");
    expect(refused("", base, ImageReferenceRefusal::empty) &&
               refused("#top", base, ImageReferenceRefusal::empty) &&
               refused("?q", base, ImageReferenceRefusal::empty),
           "an empty destination is refused");
}

// ---------------------------------------------------------------- core: 形式と大きさ（追補
// 48・49）

[[nodiscard]] std::string bytes_of(std::initializer_list<unsigned> values)
{
    std::string bytes;
    for (const unsigned value : values)
    {
        bytes.push_back(static_cast<char>(value));
    }
    return bytes;
}

[[nodiscard]] std::string bitmap_head(unsigned header)
{
    std::string head = "BM" + std::string(12U, '\0');
    head += bytes_of(
        {header & 0xFFU, (header >> 8U) & 0xFFU, (header >> 16U) & 0xFFU, (header >> 24U) & 0xFFU});
    return head;
}

void verify_formats()
{
    const std::string png = bytes_of({0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0, 0});
    expect(core::image_format_of(png) == ImageFormat::png &&
               core::image_format_of(png.substr(0, 7)) == ImageFormat::unknown,
           "PNG is the 8 byte signature and a shorter head is unknown");
    expect(core::image_format_of(bytes_of({0xFF, 0xD8, 0xFF, 0xE0})) == ImageFormat::jpeg &&
               core::image_format_of(bytes_of({0xFF, 0xD8})) == ImageFormat::unknown,
           "JPEG is FF D8 FF");
    expect(core::image_format_of("GIF89a\x01\x00") == ImageFormat::gif &&
               core::image_format_of("GIF87a") == ImageFormat::gif &&
               core::image_format_of("GIF88a") == ImageFormat::unknown,
           "GIF is GIF87a or GIF89a");
    expect(core::image_format_of(bitmap_head(40U)) == ImageFormat::bmp &&
               core::image_format_of(bitmap_head(12U)) == ImageFormat::bmp &&
               core::image_format_of(bitmap_head(124U)) == ImageFormat::bmp &&
               core::image_format_of(bitmap_head(41U)) == ImageFormat::unknown &&
               core::image_format_of("BM") == ImageFormat::unknown,
           "BMP is BM with a known information header size");
    expect(core::image_format_of("") == ImageFormat::unknown &&
               core::image_format_of("<svg xmlns=") == ImageFormat::unknown &&
               core::image_format_of("RIFF\x10\x00\x00\x00WEBPVP8 ") == ImageFormat::unknown,
           "SVG, WebP and an empty head are unknown");
    expect(core::image_format_probe_bytes >= bitmap_head(40U).size(),
           "the probe length covers every signature");
}

void verify_sizes()
{
    expect(!core::image_pixels_fit(ImageSize{0U, 1U}) &&
               core::image_pixels_fit(ImageSize{8000U, 8000U}) &&
               !core::image_pixels_fit(ImageSize{8001U, 8000U}) &&
               core::image_pixels_fit(ImageSize{65535U, 1U}),
           "an image fits up to 64,000,000 pixels");
    expect(core::image_decoded_size(ImageSize{100U, 50U}) == ImageSize{100U, 50U} &&
               core::image_decoded_size(ImageSize{2048U, 2048U}) == ImageSize{2048U, 2048U},
           "an image within 2048 pixels is kept as it is");
    expect(core::image_decoded_size(ImageSize{4096U, 1024U}) == ImageSize{2048U, 512U} &&
               core::image_decoded_size(ImageSize{1024U, 4096U}) == ImageSize{512U, 2048U} &&
               core::image_decoded_size(ImageSize{3000U, 2000U}) == ImageSize{2048U, 1365U} &&
               core::image_decoded_size(ImageSize{2049U, 2049U}) == ImageSize{2048U, 2048U},
           "a larger image is shrunk to the long side 2048 keeping the ratio");
    expect(core::image_decoded_size(ImageSize{10000U, 1U}) == ImageSize{2048U, 1U} &&
               core::image_decoded_size(ImageSize{4097U, 3U}) == ImageSize{2048U, 1U},
           "a shrunk side never goes below 1");
    expect(core::image_placement(ImageSize{100U, 50U}, 200U) == ImageSize{100U, 50U} &&
               core::image_placement(ImageSize{200U, 50U}, 200U) == ImageSize{200U, 50U},
           "an image narrower than the frame keeps its size (never enlarged)");
    expect(core::image_placement(ImageSize{400U, 300U}, 200U) == ImageSize{200U, 150U} &&
               core::image_placement(ImageSize{3U, 5U}, 2U) == ImageSize{2U, 3U} &&
               core::image_placement(ImageSize{401U, 3U}, 200U) == ImageSize{200U, 1U},
           "a wider image is shrunk to the frame width with the height rounded");
    expect(core::image_placement(ImageSize{4096U, 1024U}, 3000U) == ImageSize{2048U, 512U} &&
               core::image_placement(ImageSize{4096U, 1024U}, 800U) == ImageSize{800U, 200U} &&
               core::image_placement(ImageSize{1000U, 20000U}, 800U) == ImageSize{102U, 2048U} &&
               core::image_placement(ImageSize{1U, 64'000'000U}, 800U) == ImageSize{1U, 2048U} &&
               core::image_placement(ImageSize{20000U, 1U}, 800U) == ImageSize{800U, 1U},
           "the decoded size is the upper bound (pixels are never enlarged) and no side is 0");
    expect(core::image_placement(ImageSize{300U, 100U}, 0U) == ImageSize{0U, 0U} &&
               core::image_placement(ImageSize{0U, 5U}, 10U) == ImageSize{0U, 0U},
           "a zero size or frame places nothing");
}

// ---------------------------------------------------------------- application: PreviewImages

// 決まりに合う画素（色は 0）。
[[nodiscard]] std::shared_ptr<const app::ImagePixels> pixels_of(ImageSize original)
{
    const ImageSize size = core::image_decoded_size(original);
    return std::make_shared<const app::ImagePixels>(
        app::ImagePixels{original, size,
                         std::vector<std::uint8_t>(std::size_t{size.width} * size.height *
                                                   core::image_pixel_bytes)});
}

[[nodiscard]] app::ImageOutcome loaded(std::uint64_t ticket, ImageSize original)
{
    return app::ImageOutcome{ticket, pixels_of(original)};
}

[[nodiscard]] app::ImageOutcome failed(std::uint64_t ticket, app::ImageFailure failure)
{
    return app::ImageOutcome{ticket, std::unexpected(failure)};
}

// 段落ごとに画像 1 つの文書（n 個・経路は p<i>.png）。
[[nodiscard]] std::string paragraphs(std::size_t count)
{
    std::string text;
    for (std::size_t index = 0; index < count; ++index)
    {
        text += "![i" + std::to_string(index) + "](p" + std::to_string(index) + ".png)\n\n";
    }
    return text;
}

// 頼んだ要求の券（頼まなかったら 0・券は 1 から）。
[[nodiscard]] std::uint64_t ticket_of(const std::optional<app::ImageRequest> &request)
{
    return request.has_value() ? request.value().ticket : 0U;
}

// 頼んだ要求の経路（頼まなかったら空）。
[[nodiscard]] std::string requested_path(const std::optional<app::ImageRequest> &request)
{
    return request.has_value() ? std::string(request.value().path.text()) : std::string();
}

// 窓で頼めなくなるまで、頼むたびに original の大きさの結果を置く。頼んだ経路を返す（券はこの中の
// 数・頼み中は高々 1 つなので外の券と重なっても取り違えない）。
[[nodiscard]] std::vector<std::string> read_all(app::PreviewImages &images,
                                                const core::MarkdownDocument &document,
                                                app::PreviewImageWindow window, ImageSize original)
{
    std::vector<std::string> paths;
    std::uint64_t tickets = 0;
    for (auto request = images.claim(document, window, true, tickets); request.has_value();
         request = images.claim(document, window, true, tickets))
    {
        paths.emplace_back(requested_path(request));
        static_cast<void>(images.received(loaded(ticket_of(request), original)));
    }
    return paths;
}

void verify_slots()
{
    const auto document = parsed_document("![a](a.png) ![b](http://x/b.png) ![c](a.png) "
                                          "![d](sub/d.png)");
    const auto images = app::PreviewImages::of(document, path_of(readme), nullptr);
    expect(images.count() == 4U && images.status_of(0) == Status::waiting &&
               images.refusal_of(1) == ImageReferenceRefusal::remote &&
               !images.status_of(1).has_value() && images.status_of(3) == Status::waiting,
           "each image node is a waiting path or a refusal");
    expect(images.path_of(0) != nullptr && images.path_of(2) != nullptr &&
               images.path_of(0)->text() == "C:\\docs\\a.png" &&
               images.path_of(0) == images.path_of(2),
           "two nodes with the same path share one entry");
    expect(images.pixels_of(0) == nullptr && images.held_bytes() == 0U && !images.status_of(4),
           "nothing is held before reading and an ordinal past the nodes has no state");
    const auto untitled = app::PreviewImages::of(document, std::nullopt, nullptr);
    expect(untitled.refusal_of(0) == ImageReferenceRefusal::no_base &&
               untitled.refusal_of(3) == ImageReferenceRefusal::no_base,
           "an untitled document shows no relative image");
    const auto many =
        app::PreviewImages::of(parsed_document(paragraphs(300U)), path_of(readme), nullptr);
    expect(many.count() == core::preview_image_limit && !many.status_of(256).has_value() &&
               !many.refusal_of(256).has_value() && many.status_of(255) == Status::waiting,
           "only the first 256 images of a document are shown");
    std::string repeated;
    for (std::size_t index = 0; index < 300U; ++index)
    {
        repeated += "![r](r" + std::to_string(index % 7U) + ".png)\n\n";
    }
    const auto shared = app::PreviewImages::of(parsed_document(repeated), path_of(readme), nullptr);
    expect(shared.count() == core::preview_image_limit && shared.path_of(0) == shared.path_of(7) &&
               shared.path_of(6) == shared.path_of(251) && shared.path_of(0) != shared.path_of(1) &&
               shared.path_of(255)->text() == "C:\\docs\\r3.png",
           "256 nodes over 7 paths share 7 entries (the paths are matched through a sorted index)");
}

void verify_claims()
{
    const auto document = parsed_document(paragraphs(6U));
    auto images = app::PreviewImages::of(document, path_of(readme), nullptr);
    std::uint64_t tickets = 0;
    expect(images.wants(document, {1U, 3U}, true) && !images.wants(document, {1U, 3U}, false) &&
               !images.claim(document, {1U, 3U}, false, tickets).has_value() && tickets == 0U,
           "waiting images in the window are requested only while the port has no request");
    const auto first = images.claim(document, {1U, 3U}, true, tickets);
    expect(first.has_value() && ticket_of(first) == 1U &&
               requested_path(first) == "C:\\docs\\p1.png" && tickets == 1U &&
               images.status_of(1) == Status::requested && images.status_of(2) == Status::waiting &&
               images.status_of(0) == Status::waiting,
           "only the image nearest the anchor block is requested, with the next ticket");
    expect(!images.wants(document, {1U, 3U}, true) &&
               !images.claim(document, {1U, 3U}, true, tickets).has_value(),
           "while one image is requested no other image is requested");
    expect(!images.received(loaded(7U, ImageSize{4U, 2U})) &&
               images.status_of(1) == Status::requested,
           "a result for a ticket nobody waits for is dropped");
    expect(images.received(loaded(1U, ImageSize{4U, 2U})) &&
               images.status_of(1) == Status::loaded && images.pixels_of(1) != nullptr &&
               images.pixels_of(1)->original == ImageSize{4U, 2U} && images.held_bytes() == 32U,
           "a loaded result holds its pixels");
    expect(!images.received(loaded(1U, ImageSize{4U, 2U})), "the same ticket is taken only once");
    const auto second = images.claim(document, {1U, 3U}, true, tickets);
    expect(second.has_value() && ticket_of(second) == 2U &&
               requested_path(second) == "C:\\docs\\p2.png",
           "the next nearest image is requested after the result is taken");
    expect(images.received(failed(2U, app::ImageFailure::unsupported)) &&
               images.status_of(2) == Status::failed && images.pixels_of(2) == nullptr,
           "a failed image stays as text");
    expect(read_all(images, document, {0U, 6U}, ImageSize{1U, 1U}) ==
                   std::vector<std::string>{"C:\\docs\\p0.png", "C:\\docs\\p3.png",
                                            "C:\\docs\\p4.png", "C:\\docs\\p5.png"} &&
               images.status_of(2) == Status::failed,
           "images are requested one by one nearest the anchor first and a failed one never again");
    static_cast<void>(images.claim(document, {3U, 4U}, true, tickets));
    expect(!images.claim(document, {0U, 6U}, true, tickets).has_value() &&
               images.status_of(2) == Status::failed,
           "leaving the window and coming back does not request a failed image either");
}

void verify_malformed_pixels()
{
    const auto document = parsed_document(paragraphs(4U));
    auto images = app::PreviewImages::of(document, path_of(readme), nullptr);
    std::uint64_t tickets = 0;
    auto wrong_size = std::make_shared<const app::ImagePixels>(
        app::ImagePixels{ImageSize{4096U, 4U}, ImageSize{4096U, 4U}, {}});
    auto short_bytes = std::make_shared<const app::ImagePixels>(
        app::ImagePixels{ImageSize{2U, 2U}, ImageSize{2U, 2U}, std::vector<std::uint8_t>(15U)});
    const auto first = images.claim(document, {0U, 4U}, true, tickets);
    const bool wrong = images.received(app::ImageOutcome{ticket_of(first), wrong_size});
    const auto second = images.claim(document, {0U, 4U}, true, tickets);
    const bool short_taken = images.received(app::ImageOutcome{ticket_of(second), short_bytes});
    const auto third = images.claim(document, {0U, 4U}, true, tickets);
    const bool null_taken = images.received(app::ImageOutcome{ticket_of(third), nullptr});
    expect(wrong && short_taken && null_taken && images.status_of(0) == Status::failed &&
               images.status_of(1) == Status::failed && images.status_of(2) == Status::failed &&
               images.held_bytes() == 0U,
           "pixels that break the rules of ImagePixels are taken as a corrupt image");
}

// 必須 M1（V-1385）: 3 枚の 2048 × 2048 の文書は 2 枚を持ち、本文を変えた再解析で頼み直さない。
void verify_held_limit()
{
    const std::string body = "![a](a.png)\n\n![b](b.png)\n\n![c](c.png)\n";
    const auto document = parsed_document(body);
    auto images = app::PreviewImages::of(document, path_of(readme), nullptr);
    std::uint64_t tickets = 0;
    const ImageSize largest{2048U, 2048U};
    expect(read_all(images, document, {0U, 3U}, largest) ==
                   std::vector<std::string>{"C:\\docs\\a.png", "C:\\docs\\b.png"} &&
               images.held_bytes() == core::preview_image_held_byte_limit,
           "two images of 2048 by 2048 fill the 32 MiB");
    expect(
        images.status_of(0) == Status::loaded && images.status_of(1) == Status::loaded &&
            images.status_of(2) == Status::waiting && !images.wants(document, {0U, 3U}, true),
        "the third is not requested: only farther images may be dropped (no read and drop loop)");
    const auto edited = parsed_document(body + "x");
    auto next = app::PreviewImages::of(edited, path_of(readme), &images);
    expect(!next.wants(edited, {0U, 4U}, true) &&
               !next.claim(edited, {0U, 4U}, true, tickets).has_value() &&
               next.status_of(0) == Status::loaded && next.status_of(1) == Status::loaded,
           "a new parse after typing requests nothing again (M1)");
}

// 範囲の中で捨てた画像（evicted）は、範囲の中にある間は再解析の後も頼み直さず、範囲を出て戻ると頼
// む。
void verify_evicted()
{
    const auto document = parsed_document(paragraphs(4U));
    auto images = app::PreviewImages::of(document, path_of(readme), nullptr);
    std::uint64_t tickets = 0;
    const ImageSize largest{2048U, 2048U};
    expect(read_all(images, document, {1U, 4U}, largest) ==
               std::vector<std::string>{"C:\\docs\\p1.png", "C:\\docs\\p2.png"},
           "from block 1 the two nearest images are held");
    const auto nearer = images.claim(document, {0U, 4U}, true, tickets);
    expect(nearer.has_value() && requested_path(nearer) == "C:\\docs\\p0.png" &&
               images.status_of(2) == Status::evicted && images.status_of(1) == Status::loaded &&
               images.held_bytes() == core::image_decoded_byte_limit,
           "a nearer image drops the farthest one in the window before it is requested");
    expect(images.received(loaded(ticket_of(nearer), largest)) &&
               !images.wants(document, {0U, 4U}, true) && images.status_of(2) == Status::evicted &&
               images.held_bytes() == core::preview_image_held_byte_limit,
           "the dropped image is not requested while it stays in the window");
    const auto edited = parsed_document(paragraphs(4U) + "x");
    auto next = app::PreviewImages::of(edited, path_of(readme), &images);
    expect(next.status_of(2) == Status::evicted && !next.wants(edited, {0U, 5U}, true) &&
               !next.claim(edited, {0U, 5U}, true, tickets).has_value(),
           "a new parse keeps the evicted image evicted and requests nothing (M1)");
    expect(next.wants(edited, {3U, 5U}, false) &&
               !next.claim(edited, {3U, 5U}, false, tickets).has_value() &&
               next.status_of(2) == Status::waiting,
           "leaving the window makes the evicted image wait again");
    const auto back = next.claim(edited, {2U, 5U}, true, tickets);
    expect(back.has_value() && requested_path(back) == "C:\\docs\\p2.png" &&
               next.status_of(0) == Status::waiting && next.status_of(1) == Status::loaded,
           "coming back requests it, dropping the image outside the window farthest first");
}

// 必須 M2（V-1385）: 1920 × 1080 の 41 枚の窓は、錨に近い画像を持ち、窓の下端の画像で捨てない。
void verify_nearest_kept()
{
    const auto document = parsed_document(paragraphs(41U));
    auto images = app::PreviewImages::of(document, path_of(readme), nullptr);
    std::uint64_t tickets = 0;
    const ImageSize screenshot{1920U, 1080U};
    const std::size_t one = std::size_t{1920U} * 1080U * core::image_pixel_bytes;
    expect(read_all(images, document, {0U, 41U}, screenshot) ==
                   std::vector<std::string>{"C:\\docs\\p0.png", "C:\\docs\\p1.png",
                                            "C:\\docs\\p2.png"} &&
               images.held_bytes() == 3U * one,
           "of 41 screenshots in the window the 3 nearest the anchor are read, one at a time");
    expect(images.status_of(0) == Status::loaded && images.status_of(2) == Status::loaded &&
               images.status_of(3) == Status::waiting && images.status_of(40) == Status::waiting,
           "the images on the screen near the anchor are kept and the rest wait");
    const auto edited = parsed_document(paragraphs(41U) + "x");
    auto next = app::PreviewImages::of(edited, path_of(readme), &images);
    expect(!next.claim(edited, {0U, 42U}, true, tickets).has_value(),
           "a new parse requests none of the 41 again (M1)");
}

void verify_carried()
{
    const auto first = parsed_document("![a](a.png)\n\n![b](b.png)\n\n![c](c.png)\n");
    auto images = app::PreviewImages::of(first, path_of(readme), nullptr);
    std::uint64_t tickets = 0;
    const auto a = images.claim(first, {0U, 3U}, true, tickets);
    const bool a_taken = images.received(loaded(ticket_of(a), ImageSize{2U, 2U}));
    const auto b = images.claim(first, {0U, 3U}, true, tickets);
    const bool b_taken = images.received(failed(ticket_of(b), app::ImageFailure::missing));
    const auto c = images.claim(first, {0U, 3U}, true, tickets);
    expect(a_taken && b_taken && c.has_value() && ticket_of(c) == 3U,
           "a is loaded, b failed and c is still requested");
    const auto edited = parsed_document("# new\n\n![c](c.png)\n\n![a2](a.png)\n\n![d](d.png)\n\n"
                                        "![b](b.png)\n");
    auto next = app::PreviewImages::of(edited, path_of(readme), &images);
    expect(next.status_of(1) == Status::loaded && next.pixels_of(1) == images.pixels_of(0) &&
               next.status_of(0) == Status::requested && next.status_of(2) == Status::waiting &&
               next.status_of(3) == Status::failed && next.held_bytes() == 16U,
           "a new parse of the same document keeps the state of the same paths");
    expect(!next.claim(edited, {0U, 5U}, true, tickets).has_value(),
           "the request made before the new parse is still the one request");
    expect(next.received(loaded(3U, ImageSize{1U, 1U})) && next.status_of(0) == Status::loaded,
           "a result for a request made before the new parse is still taken");
    const auto request = next.claim(edited, {0U, 5U}, true, tickets);
    const bool d_taken = next.received(loaded(ticket_of(request), ImageSize{1U, 1U}));
    expect(requested_path(request) == "C:\\docs\\d.png" && d_taken &&
               !next.claim(edited, {0U, 5U}, true, tickets).has_value(),
           "only the new path is requested and the failed one is not");
}

// ---------------------------------------------------------------- application: 道（追補 50）

// C:\docs\readme.md を開いた窓。visible は本文の見えている行の数。
void open_readme(Editing &editing, std::string_view text, std::size_t visible)
{
    editing.files().hold_at(std::string(readme), Bytes{std::string(text)});
    applied(editing.controller(), app::VisibleLines{visible});
    applied(editing.controller(), app::OpenDocument{path_of(readme)});
}

// 最後の解析の要求に、その本文を本物の parse_markdown で解いた結果を返して合図を送る。
[[nodiscard]] app::EditorFrame parsed_back(Editing &editing)
{
    const auto &request = editing.markdown().requests().back();
    editing.markdown().serve(app::MarkdownOutcome{
        request.ticket, core::parse_markdown(*request.text, core::MarkdownStop::never())});
    return editing.controller().apply_frame(app::WorkCompleted{});
}

[[nodiscard]] const app::PreviewFrame *preview_of(const app::EditorFrame &frame)
{
    if (frame.full_preview.has_value())
    {
        return &frame.full_preview.value();
    }
    return frame.other.has_value() ? std::get_if<app::PreviewFrame>(&frame.other.value()) : nullptr;
}

[[nodiscard]] std::vector<std::string> requested_paths(const ScriptedImages &images)
{
    std::vector<std::string> paths;
    for (const auto &request : images.requests())
    {
        paths.emplace_back(request.path.text());
    }
    return paths;
}

// 最後の要求に original の大きさの結果を返して合図を送ることを、新しい要求が来なくなるまで繰り返
// す。
void serve_until_quiet(Editing &editing, ImageSize original)
{
    auto &images = editing.images();
    std::size_t served = 0;
    while (images.requests().size() > served)
    {
        served = images.requests().size();
        images.serve(loaded(images.requests().back().ticket, original));
        applied(editing.controller(), app::WorkCompleted{});
    }
}

void verify_requests_visible_images()
{
    Editing editing;
    open_readme(editing, paragraphs(10U) + "![net](https://x/n.png)\n", 2U);
    auto &controller = editing.controller();
    applied(controller, app::TogglePreview{});
    expect(editing.images().requests().empty() && editing.images().collects() == 1U,
           "opening drops what was left once and requests no image before the parse result");
    const app::EditorFrame shown = parsed_back(editing);
    const auto *preview = preview_of(shown);
    expect(preview != nullptr && preview->images != nullptr &&
               preview->images->status_of(0) == Status::requested &&
               preview->images->pixels_of(0) == nullptr,
           "the frame carries the image state and a requested image is still text");
    expect(requested_paths(editing.images()) == std::vector<std::string>{"C:\\docs\\p0.png"},
           "the parse result requests only the image of the anchor block");
    applied(controller, app::PreviewTick{});
    expect(editing.images().requests().size() == 1U,
           "a tick while one image is requested requests nothing");
    serve_until_quiet(editing, ImageSize{1U, 1U});
    expect(requested_paths(editing.images()) == std::vector<std::string>{"C:\\docs\\p0.png",
                                                                         "C:\\docs\\p1.png",
                                                                         "C:\\docs\\p2.png"} &&
               editing.images().requests().back().ticket == 3U,
           "each result asks the next image of the anchor block and the next visible lines + 1");
    applied(controller, app::PreviewTick{});
    expect(editing.images().requests().size() == 3U, "a tick with nothing new requests nothing");
    // 錨は行頭で、画像の段落の source_begin は alt の文字の位置なので、行 5（p2
    // の段落）の行頭の錨の 要素は 1 つ前の p1 の段落（preview_anchor_block）。見える範囲は [1, 4)
    // で、新しいのは p3 だけ。
    applied(controller, app::ScrollPane{4});
    expect(editing.images().requests().size() == 4U &&
               editing.images().requests().back().path.text() == "C:\\docs\\p3.png",
           "moving the anchor requests the images that came into the window");
    serve_until_quiet(editing, ImageSize{1U, 1U});
    applied(controller, app::ScrollPane{12});
    serve_until_quiet(editing, ImageSize{1U, 1U});
    expect(requested_paths(editing.images()) ==
               std::vector<std::string>{"C:\\docs\\p0.png", "C:\\docs\\p1.png", "C:\\docs\\p2.png",
                                        "C:\\docs\\p3.png", "C:\\docs\\p7.png", "C:\\docs\\p8.png",
                                        "C:\\docs\\p9.png"},
           "the images skipped by the anchor are not requested and the anchor block goes first");
    applied(controller, app::ScrollPane{100});
    expect(editing.images().requests().size() == 7U, "the net image at the end is never requested");
}

// 必須 M2 の道: 41 枚の 1920 × 1080 を見えている 40 行の窓で開くと、1 枚ずつ錨に近い 3 枚を読み、打
// った後の再解析で頼み直さない。
void verify_one_request_at_a_time()
{
    Editing editing;
    open_readme(editing, paragraphs(41U), 40U);
    auto &controller = editing.controller();
    applied(controller, app::TogglePreview{});
    static_cast<void>(parsed_back(editing));
    expect(requested_paths(editing.images()) == std::vector<std::string>{"C:\\docs\\p0.png"},
           "a window of 41 images puts only one request on the worker");
    serve_until_quiet(editing, ImageSize{1920U, 1080U});
    const app::EditorFrame read = controller.frame();
    const auto *images = preview_of(read)->images.get();
    expect(requested_paths(editing.images()) == std::vector<std::string>{"C:\\docs\\p0.png",
                                                                         "C:\\docs\\p1.png",
                                                                         "C:\\docs\\p2.png"} &&
               images->status_of(0) == Status::loaded && images->status_of(2) == Status::loaded &&
               images->status_of(3) == Status::waiting,
           "the 3 images nearest the anchor are read one by one and kept");
    applied(controller, app::InsertText{"x"});
    applied(controller, app::PreviewTick{});
    applied(controller, app::PreviewTick{});
    const auto reparsed = parsed_back(editing);
    expect(editing.images().requests().size() == 3U &&
               preview_of(reparsed)->images->status_of(0) == Status::loaded,
           "a new parse after typing requests none of them again");
}

void verify_results_reach_the_frame()
{
    Editing editing;
    open_readme(editing, "![a](a.png)\n\n![b](b.png)\n", 10U);
    auto &controller = editing.controller();
    applied(controller, app::TogglePreview{});
    const app::EditorFrame first = parsed_back(editing);
    const auto *before = preview_of(first)->images.get();
    editing.images().serve(loaded(1U, ImageSize{8U, 4U}));
    editing.images().serve(loaded(99U, ImageSize{8U, 4U}));
    const app::EditorDelivery delivered = controller.apply(app::WorkCompleted{});
    const app::EditorFrame taken = controller.frame();
    const auto *after = preview_of(taken);
    expect(delivered.repaint && after->images.get() != before &&
               after->images->status_of(0) == Status::loaded &&
               after->images->pixels_of(0)->size == ImageSize{8U, 4U} &&
               after->images->status_of(1) == Status::requested &&
               editing.images().requests().size() == 2U,
           "WorkCompleted puts the result of the waiting ticket and asks the next image");
    editing.images().serve(failed(2U, app::ImageFailure::corrupt));
    applied(controller, app::WorkCompleted{});
    const auto *kept = preview_of(controller.frame())->images.get();
    expect(kept->status_of(1) == Status::failed, "a failed result stays as text");
    editing.images().serve(loaded(1U, ImageSize{8U, 4U}));
    applied(controller, app::WorkCompleted{});
    expect(preview_of(controller.frame())->images.get() == kept,
           "a result nobody waits for leaves the state as it is");
    // 打鍵の後の解析は同じ経路の状態を引き継ぎ、読めた画像も失敗した画像も頼み直さない。
    applied(controller, app::InsertText{"x"});
    applied(controller, app::PreviewTick{});
    applied(controller, app::PreviewTick{});
    const auto reparsed = parsed_back(editing);
    expect(editing.markdown().requests().size() == 2U && editing.images().requests().size() == 2U &&
               preview_of(reparsed)->images->status_of(0) == Status::loaded &&
               preview_of(reparsed)->images->status_of(1) == Status::failed,
           "a new parse of the edited body keeps the loaded and the failed image without asking");
}

void verify_switch_drops_images()
{
    Editing editing;
    open_readme(editing, "![a](a.png)\n", 10U);
    auto &controller = editing.controller();
    applied(controller, app::NewTab{});
    applied(controller, app::SwitchTab{0});
    applied(controller, app::TogglePreview{});
    static_cast<void>(parsed_back(editing));
    expect(editing.images().requests().size() == 1U, "the image of the first tab is requested");
    const app::EditorFrame switched = controller.apply_frame(app::SwitchTab{1});
    expect(preview_of(switched)->images == nullptr, "switching the document drops the image state");
    editing.images().serve(loaded(1U, ImageSize{2U, 2U}));
    const app::EditorFrame later = controller.apply_frame(app::WorkCompleted{});
    expect(preview_of(later)->images == nullptr, "a result for the previous document is dropped");
    static_cast<void>(parsed_back(editing));
    const app::EditorFrame back = controller.apply_frame(app::SwitchTab{0});
    static_cast<void>(back);
    static_cast<void>(parsed_back(editing));
    expect(editing.images().requests().size() == 2U &&
               editing.images().requests().back().ticket == 2U,
           "coming back asks again with the next ticket");
}

// 文書が替わっても、前の文書の頼み中の結果を引くまでは次を頼まない（ワーカーの列は高々 1 件）。届い
// た前の券の結果は新しい文書の画像に置かない。
void verify_one_request_across_switch()
{
    Editing editing;
    open_readme(editing, "![a](a.png)\n", 10U);
    auto &controller = editing.controller();
    applied(controller, app::NewTab{});
    applied(controller, app::InsertText{"![b](C:\\pics\\b.png)\n"});
    applied(controller, app::SwitchTab{0});
    applied(controller, app::TogglePreview{});
    static_cast<void>(parsed_back(editing));
    applied(controller, app::SwitchTab{1});
    const app::EditorFrame other = parsed_back(editing);
    expect(requested_paths(editing.images()) == std::vector<std::string>{"C:\\docs\\a.png"} &&
               preview_of(other)->images->status_of(0) == Status::waiting,
           "the new document waits while the request of the previous document is in the worker");
    editing.images().serve(loaded(1U, ImageSize{2U, 2U}));
    const app::EditorFrame settled = controller.apply_frame(app::WorkCompleted{});
    expect(requested_paths(editing.images()) ==
                   std::vector<std::string>{"C:\\docs\\a.png", "C:\\pics\\b.png"} &&
               editing.images().requests().back().ticket == 2U &&
               preview_of(settled)->images->status_of(0) == Status::requested &&
               preview_of(settled)->images->pixels_of(0) == nullptr,
           "the old result is dropped and then the new document asks with the next ticket");
}

void verify_untitled_relative_stays_text()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"![a](a.png)\n\n![b](C:\\pics\\b.png)\n"});
    applied(controller, app::TogglePreview{});
    const auto shown = parsed_back(editing);
    expect(preview_of(shown)->images->refusal_of(0) == ImageReferenceRefusal::no_base &&
               requested_paths(editing.images()) == std::vector<std::string>{"C:\\pics\\b.png"},
           "an untitled document requests only absolute paths");
}

void verify_closed_preview_is_quiet()
{
    Editing editing;
    open_readme(editing, "![a](a.png)\n", 10U);
    auto &controller = editing.controller();
    applied(controller, app::InsertText{"typed"});
    applied(controller, app::PreviewTick{});
    applied(controller, app::WorkCompleted{});
    applied(controller, app::NewTab{});
    applied(controller, app::SwitchTab{0});
    applied(controller, app::SaveDocument{path_of(readme), core::TextEncoding::utf8});
    expect(editing.images().touches() == 0U, "without a preview no path touches the image port");
    applied(controller, app::TogglePreview{});
    applied(controller, app::TogglePreview{});
    applied(controller, app::TogglePreview{});
    const std::size_t touched = editing.images().touches();
    applied(controller, app::WorkCompleted{});
    applied(controller, app::PreviewTick{});
    applied(controller, app::InsertText{"more"});
    expect(editing.images().touches() == touched,
           "after closing the preview the signal, the tick and typing stay quiet");
    applied(controller, app::TogglePreview{});
    expect(editing.images().collects() == 2U, "opening again drops what was left once more");
}
} // namespace

void verify_preview_image_contracts()
{
    verify_image_node();
    verify_image_attributes();
    verify_image_order();
    verify_relative_references();
    verify_absolute_references();
    verify_refused_schemes();
    verify_reference_pins();
    verify_formats();
    verify_sizes();
    verify_slots();
    verify_claims();
    verify_malformed_pixels();
    verify_held_limit();
    verify_evicted();
    verify_nearest_kept();
    verify_carried();
    verify_requests_visible_images();
    verify_one_request_at_a_time();
    verify_results_reach_the_frame();
    verify_switch_drops_images();
    verify_one_request_across_switch();
    verify_untitled_relative_stays_text();
    verify_closed_preview_is_quiet();
}

void verify_preview_image_scope()
{
    verify_preview_image_contracts();
}
} // namespace nenenib::tests
